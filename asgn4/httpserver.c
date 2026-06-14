// Asgn 4: A simple HTTP server.
// By:  Andi Quinn

#include "connection.h"
#include "listener_socket.h"
#include "request.h"
#include "response.h"
#include "queue.h"
#include "rwlock.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include <sys/stat.h>

void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);
void *worker_thread(void *arg);
void audit_log(
    const char *method, const char *uri, int status, const char *request_id);

queue_t *q;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct file_lock { //reader/writer lock per file
    char *uri;
    rwlock_t *lock;
} file_lock_t;

file_lock_t file_locks[1024]; // Array to store file locks
int num_locks = 0;

pthread_mutex_t file_locks_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {

    int port_index = 1;
    int threads = 4;

    //./httpserver [-t threads] <port>

    if (argc == 4) {
        if (strcmp(argv[1], "-t") == 0) {
            threads = atoi(argv[2]);
            port_index = 3;
        } else { //wrong option
            fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (port_index >= argc) { //no port provided
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[port_index], &endptr, 10);
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[port_index]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket_t *sock = ls_new(port);
    if (!sock) {
        warnx("cannot open socket");
        return EXIT_FAILURE;
    }

    //int threads = 4; //default nums of threads
    q = queue_new(1024);

    pthread_t workers[threads]; // array of worker threads

    for (int i = 0; i < threads; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    while (1) {
        int connfd = ls_accept(sock);
        if (connfd < 0) {
            continue;
        }

        queue_push(q, (void *) (intptr_t) connfd);
    }
    ls_delete(&sock);

    return EXIT_SUCCESS;
}

// return lock for specific uri
rwlock_t *get_file_lock(const char *uri) {
    pthread_mutex_lock(&file_locks_mutex);
    for (int i = 0; i < num_locks; i++) {
        if (strcmp(file_locks[i].uri, uri) == 0) {
            pthread_mutex_unlock(&file_locks_mutex);
            return file_locks[i].lock;
        }
    }

    // if(num_locks >= 1024) {
    //     pthread_mutex_unlock(&file_locks_mutex);
    //     return NULL; // No more locks available
    // }

    file_locks[num_locks].uri = strdup(uri); // Duplicate the URI string

    file_locks[num_locks].lock
        = rwlock_new(WRITERS, 0); // Create a new rwlock for the file

    rwlock_t *lock = file_locks[num_locks].lock; // get the lock for the file

    num_locks++;

    pthread_mutex_unlock(&file_locks_mutex);
    return lock;
}

void *worker_thread(void *arg) {
    (void) arg;

    while (1) {
        void *element;
        queue_pop(q, &element);
        int connfd = (intptr_t) element;
        handle_connection(connfd);
        close(connfd);
    }
    return NULL;
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        //printf("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_PUT) {
            handle_put(conn);
        }
        // add cases for other types of requests here
        else if (req == &REQUEST_GET) {
            handle_get(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);

    char *request_id = conn_get_header(conn, "Request-Id");
    if (request_id == NULL) {
        request_id = "0";
    }

    rwlock_t *lock = get_file_lock(uri); //get lock for writing

    const Response_t *res = NULL;

    //bool existed = access(uri, F_OK) == 0;

    //writer_lock(lock); //get lock for writing

    // bool existed = access(uri, F_OK) == 0;

    char temp[] = "tempfileXXXXXX";
    int temp_fd = mkstemp(temp);
    if (temp_fd < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        audit_log("PUT", uri, response_get_code(res), request_id);
        conn_send_response(conn, res);
        // goto out;
        return;
    }

    res = conn_recv_file(conn, temp_fd);

    if (close(temp_fd) < 0) {
        unlink(temp);
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        audit_log("PUT", uri, response_get_code(res), request_id);
        conn_send_response(conn, res);
        // goto out;
        return;
    }

    if (res != NULL) {
        unlink(temp);
        audit_log("PUT", uri, response_get_code(res), request_id);
        conn_send_response(conn, res);
        // goto out;
        return;
    }

    writer_lock(lock); //get lock for writing

    bool existed = access(uri, F_OK) == 0;

    if (rename(temp, uri) != 0) {
        unlink(temp);
        if (errno == EACCES || errno == EISDIR) {
            res = &RESPONSE_FORBIDDEN;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
        goto out;
    } else if (existed) {
        res = &RESPONSE_OK;
    } else {
        res = &RESPONSE_CREATED;
    }

out:
    audit_log("PUT", uri, response_get_code(res), request_id);

    conn_send_response(conn, res);
    if (lock != NULL) {
        writer_unlock(lock); //release lock for writing
    }
}

void handle_get(conn_t *conn) {
    rwlock_t *lock = get_file_lock(conn_get_uri(conn)); //get lock for reading

    // if (lock == NULL) {
    //     conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
    //     return;
    // }
    reader_lock(lock); //get lock for reading

    char *uri = conn_get_uri(conn);

    char *request_id = conn_get_header(conn, "Request-Id");
    if (request_id == NULL) {
        request_id = "0";
    }

    const Response_t *res = NULL;

    // Open the file
    int fd = open(uri, O_RDONLY);

    if (fd < 0) {
        // printf("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR) {
            res = &RESPONSE_FORBIDDEN;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
        goto out;
    }

    struct stat st;
    fstat(fd, &st);

    res = conn_send_file(
        conn, fd, st.st_size); //send response first, then send file

    close(fd);
    if (res == NULL) {
        audit_log("GET", uri, 200, request_id);
    } else {
        audit_log("GET", uri, response_get_code(res), request_id);
    }
    reader_unlock(lock); //release lock for reading

    return;

out:

    audit_log("GET", uri, response_get_code(res), request_id);
    conn_send_response(conn, res);

    reader_unlock(lock);
}

void handle_unsupported(conn_t *conn) {
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void audit_log(
    const char *method, const char *uri, int status, const char *request_id) {
    char audit_uri[128];
    if (uri[0] == '/') {
        snprintf(audit_uri, sizeof(audit_uri), "%s", uri);
    } else {
        snprintf(audit_uri, sizeof(audit_uri), "/%s", uri);
    }

    pthread_mutex_lock(&mutex);
    fprintf(stderr, "%s,%s,%d,%s\n", method, audit_uri, status, request_id);
    pthread_mutex_unlock(&mutex);
}
