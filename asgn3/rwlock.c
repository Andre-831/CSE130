#include "rwlock.h"
#include <pthread.h>
#include <stdlib.h>

struct rwlock {
    pthread_mutex_t mutex;

    pthread_cond_t readers_cond;
    pthread_cond_t writers_cond;

    int readers;
    int writers;

    int waiting_writers;
    int waiting_readers;

    PRIORITY priority;

    int n;
    int readers_since_writer;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rw = malloc(sizeof(rwlock_t));

    pthread_mutex_init(&rw->mutex, NULL);

    pthread_cond_init(&rw->readers_cond, NULL);
    pthread_cond_init(&rw->writers_cond, NULL);

    rw->readers = 0;
    rw->writers = 0;
    rw->waiting_writers = 0;
    rw->waiting_readers = 0;
    rw->priority = p;
    rw->n = n;
    rw->readers_since_writer = 0;

    return rw;
}

void rwlock_delete(rwlock_t **rw) {

    if (rw == NULL || *rw == NULL) {
        return;
    }

    pthread_cond_destroy(&(*rw)->writers_cond);
    pthread_cond_destroy(&(*rw)->readers_cond);
    pthread_mutex_destroy(&(*rw)->mutex);
    free(*rw);
    *rw = NULL;
}

void reader_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->waiting_readers++;

    while (rw->writers > 0
           || (rw->priority == WRITERS && rw->waiting_writers > 0)
           || (rw->priority == N_WAY && rw->waiting_writers > 0
               && rw->readers_since_writer >= rw->n)) {
        pthread_cond_wait(&rw->readers_cond, &rw->mutex);
    }
    rw->waiting_readers--;
    rw->readers++;

    if (rw->priority == N_WAY) {
        rw->readers_since_writer++;
    }

    pthread_mutex_unlock(&rw->mutex);
}

void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);

    rw->readers--;

    if (rw->readers == 0) {
        //     pthread_cond_signal(&rw->writers_cond);
        // }

        if (rw->priority == N_WAY) {
            if (rw->waiting_writers > 0 && rw->waiting_readers > 0
                && rw->readers_since_writer < rw->n) {
                pthread_cond_signal(&rw->readers_cond);
            } else if (rw->waiting_readers > 0 && rw->waiting_writers == 0) {
                pthread_cond_broadcast(&rw->readers_cond);
            } else if (rw->waiting_writers > 0) {
                pthread_cond_signal(&rw->writers_cond);
            }
        } else if (rw->priority == WRITERS) {
            if (rw->waiting_writers > 0) {
                pthread_cond_signal(&rw->writers_cond);
            } else if (rw->waiting_readers > 0) {
                pthread_cond_broadcast(&rw->readers_cond);
            }
        } else { //READERS priority
            if (rw->waiting_readers > 0) {
                pthread_cond_broadcast(&rw->readers_cond);
            } else if (rw->waiting_writers > 0) {
                pthread_cond_signal(&rw->writers_cond);
            }
        }
    }
    pthread_mutex_unlock(&rw->mutex);
}

void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);
    rw->waiting_writers++;

    //wait while there are active writers or if writers have priority and there are waiting readers
    while (rw->writers > 0 || rw->readers > 0
           || (rw->priority == N_WAY && rw->waiting_readers > 0
               && rw->readers_since_writer < rw->n)) {
        pthread_cond_wait(&rw->writers_cond, &rw->mutex);
    }
    rw->waiting_writers--;
    rw->writers++;

    pthread_mutex_unlock(&rw->mutex);
}

void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->mutex);

    rw->writers--;
    rw->readers_since_writer = 0;

    if (rw->priority == WRITERS) {
        if (rw->waiting_writers > 0) {
            pthread_cond_signal(&rw->writers_cond);
        } else if (rw->waiting_readers > 0) {
            pthread_cond_broadcast(&rw->readers_cond);
        }
    } else if (rw->priority == READERS) { //READERS priority
        if (rw->waiting_readers > 0) {
            pthread_cond_broadcast(&rw->readers_cond);
        } else if (rw->waiting_writers
                   > 0) { //if no waiting readers, signal a waiting writer
            pthread_cond_signal(&rw->writers_cond);
        }
    } else { //N_WAY priority
        if (rw->waiting_readers > 0) {
            pthread_cond_signal(&rw->readers_cond);
        } else if (rw->waiting_writers > 0) {
            pthread_cond_signal(&rw->writers_cond);
        }
    }

    pthread_mutex_unlock(&rw->mutex);
}
