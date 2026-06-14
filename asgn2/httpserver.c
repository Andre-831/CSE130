/**
 * @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author [Put your name here]
 */

#include "listener_socket.h"
#include "iowrapper.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define BUFFER_SIZE 4096

void send_response(
    int fd, int status_code, const char *status_message, const char *body) {
    //sends an HTTP response using the given status code, message, and body
    char header[256];
    int body_length = body ? strlen(body) : 0;

    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n", status_code,
        status_message, body_length);

    write_n_bytes(fd, header, n);

    if (body_length > 0) {
        write_n_bytes(fd, (char *) body, body_length);
    }
}

/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {
    /* Your code here */

    char buffer[BUFFER_SIZE + 1];

    ssize_t total = 0;

    ssize_t n;

    //read until  "\r\n\r\n",
    while ((n = read(connfd, buffer + total, BUFFER_SIZE - total)) > 0) {
        total += n;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }

        if (total >= BUFFER_SIZE) {
            break;
        }
    }

    if (total <= 0) {
        close(connfd);
        return;
    }

    //parse the request line
    char method[9], uri[65], version[16];

    if (sscanf(buffer, "%8s %64s %15s", method, uri, version) != 3) {
        send_response(connfd, 505, "Bad Request", "Bad Request\n");
        close(connfd);
        return;
    }

    //check method
    if (strlen(method) > 8) { // if method length is invalid
        send_response(connfd, 400, "Bad Request", "Bad Request\n");
        close(connfd);
        return;
    }

    for (int i = 0; method[i]; i++) { //check if method contains only letters
        if (!isalpha(method[i])) {
            send_response(connfd, 400, "Bad Request", "Bad Request\n");
            close(connfd);
            return;
        }
    }

    //check version format
    if (strlen(version) != 8 || strncmp(version, "HTTP/", 5) != 0 || !version[5]
        || version[6] != '.' || !isdigit(version[7])) {
        send_response(connfd, 400, "Bad Request", "Bad Request\n");
        close(connfd);
    }
    //check version
    if (strcmp(version, "HTTP/1.1") != 0) {
        send_response(
            connfd, 505, "Version Not Supported", "Version Not Supported\n");
        close(connfd);
        return;
    }
    // check uri
    if (uri[0] != '/' || strlen(uri) < 2 || strlen(uri) > 64) {
        send_response(connfd, 400, "Bad Request", "Bad Request\n");
        close(connfd);
        return;
    }

    char *filename = uri + 1; // skip the leading '/'

    //check invalid chars in uri

    for (int i = 1; uri[i]; i++) {
        if (!(isalnum(uri[i]) || uri[i] == '-' || uri[i] == '.')) {
            send_response(connfd, 400, "Bad Request", "Bad Request\n");
            close(connfd);
            return;
        }
    }

    //body start
    char *body = strstr(buffer, "\r\n\r\n");
    if (!body) {
        send_response(connfd, 400, "Bad Request", "Bad Request\n");
        close(connfd);
        return;
    }

    body += 4; // skip the "\r\n\r\n"

    //get
    if (strcmp(method, "GET") == 0) {
        int fd;

        if ((fd = open(filename, O_RDONLY | O_DIRECTORY)) != -1) {
            close(fd);
            send_response(connfd, 403, "Forbidden", "Forbidden\n");
            close(connfd);
            return;
        }

        fd = open(filename, O_RDONLY);

        if (fd < 0) {
            if (errno == ENOENT) {
                send_response(connfd, 404, "Not Found", "Not Found\n");
            } else if (errno == EACCES) {
                send_response(connfd, 403, "Forbidden", "Forbidden\n");
            } else {
                send_response(connfd, 500, "Internal Server Error",
                    "Internal Server Error\n");
            }
            close(connfd);
            return;
        }

        struct stat st;
        fstat(fd, &st);
        int size = st.st_size;

        char header[256];

        int len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", size);

        write_n_bytes(connfd, header, len);
        pass_n_bytes(fd, connfd, size);

        close(fd);
    }

    else if (strcmp(method, "PUT") == 0) {
        char *content_length_str = strstr(buffer, "Content-Length: ");
        if (!content_length_str) {
            send_response(connfd, 400, "Bad Request", "Bad Request\n");
            close(connfd);
            return;
        }

        int content_len = 0;

        sscanf(content_length_str, "Content-Length: %d", &content_len);

        if (content_len < 0) {
            send_response(connfd, 400, "Bad Request", "Bad Request\n");
            close(connfd);
            return;
        }

        //check dir
        int fd;

        if ((fd = open(filename, O_RDONLY | O_DIRECTORY)) != -1) {
            close(fd);
            send_response(connfd, 403, "Forbidden", "Forbidden\n");
            close(connfd);
            return;
        }

        int exist = access(filename, F_OK) == 0;

        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            send_response(connfd, 403, "Forbidden", "Forbidden\n");
            close(connfd);
            return;
        }

        int bytes_read = total - (body - buffer);

        if (bytes_read > 0) {
            int to_write = bytes_read > content_len ? content_len : bytes_read;
            write_n_bytes(fd, body, to_write);
            content_len -= to_write;
        }

        if (content_len > 0) {
            pass_n_bytes(connfd, fd, content_len);
        }

        close(fd);

        if (exist) {
            send_response(connfd, 200, "OK", "OK\n");
        } else {
            send_response(connfd, 201, "Created", "Created\n");
        }
    } else {
        send_response(connfd, 501, "Not Implemented", "Not Implemented\n");
        close(connfd);
        return;
    }

    close(connfd);
    return;
}

/** @brief Main function for the HTTP server.
 *
 *  @param argc The number of arguments.
 *  @param argv The arguments.
 *
 *  @return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[1], &endptr, 10);

    /* Add error checking for the port number */
    if (endptr == argv[1] || *endptr != '\0' || port == 0 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket_t *sock;
    sock = ls_new(port);

    if (sock == NULL) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    while (1) {
        int connfd = ls_accept(sock);
        //handle_connection(connfd);

        if (connfd < 0) {
            continue;
        }
        handle_connection(connfd);
    }

    return EXIT_SUCCESS;
}
