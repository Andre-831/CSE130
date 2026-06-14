#include "rwlock.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

rwlock_t *rw;

void *reader(void *arg) {
    int id = *(int *) arg;

    reader_lock(rw);
    printf("reader %d enter\n", id);

    usleep(100000);

    printf("reader %d leave\n", id);
    reader_unlock(rw);

    return NULL;
}

void *writer(void *arg) {
    int id = *(int *) arg;

    writer_lock(rw);
    printf("writer %d enter\n", id);

    usleep(100000);

    printf("writer %d leave\n", id);
    writer_unlock(rw);

    return NULL;
}

int main(void) {
    rw = rwlock_new(N_WAY, 2);

    pthread_t t[6];
    int ids[6] = {1,2,3,4,5,6};

    pthread_create(&t[0], NULL, writer, &ids[0]);

    usleep(50000);

    pthread_create(&t[1], NULL, reader, &ids[1]);
    pthread_create(&t[2], NULL, reader, &ids[2]);
    pthread_create(&t[3], NULL, reader, &ids[3]);

    usleep(50000);

    pthread_create(&t[4], NULL, writer, &ids[4]);

    pthread_create(&t[5], NULL, reader, &ids[5]);

    for (int i = 0; i < 6; i++) {
        pthread_join(t[i], NULL);
    }

    rwlock_delete(&rw);

    return 0;
}
