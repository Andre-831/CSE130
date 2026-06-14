
#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

struct queue {
    void **data;
    int head;
    int tail;
    int size;
    int capacity;
    pthread_mutex_t mutex;

    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

queue_t *queue_new(int size) {
    queue_t *q = malloc(sizeof(queue_t));
    q->data = malloc(size * sizeof(void *));
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    q->capacity = size;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return q;
}

void queue_delete(queue_t **q) {
    if (q == NULL || *q == NULL) {
        return;
    }
    pthread_cond_destroy(&(*q)->not_empty);
    pthread_cond_destroy(&(*q)->not_full);
    pthread_mutex_destroy(&(*q)->mutex);

    free((*q)->data);
    free(*q);
    *q = NULL;
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) {
        return false;
    }
    pthread_mutex_lock(&q->mutex);

    while (q->size == q->capacity) { //wait while q is full
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->data[q->tail] = elem;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (!q || !elem) {
        return false;
    }
    pthread_mutex_lock(&q->mutex);

    while (q->size == 0) { //wait while q is empty
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    *elem = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return true;
}
