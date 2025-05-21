#include <stdlib.h>
#include <pthread.h>
#include "buffer_queue.h"

struct buffer_queue {
    struct work_record **buffer;
    int start;
    int end;
    int count;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t nonempty;
    pthread_cond_t nonfull;
};

Buffer_Queue buffer_queue_create(int size) {
    Buffer_Queue q = malloc(sizeof(struct buffer_queue));
    if (q==NULL)
        return NULL;
    q->buffer = malloc(size*sizeof(struct work_record*));
    if (q->buffer==NULL) {
        free(q);
        return NULL;
    }
    q->start = 0;
    q->end = -1;
    q->count = 0;
    q->size = size;
    pthread_mutex_init(&q->mutex,0);
    pthread_cond_init(&q->nonempty,0);
    pthread_cond_init(&q->nonfull,0);
    return q;
}

void buffer_queue_push(Buffer_Queue q,struct work_record *rec) {
    pthread_mutex_lock(&q->mutex);
    while (q->count==q->size)
        pthread_cond_wait(&q->nonfull,&q->mutex);
    q->end=(q->end+1)%q->size;
    q->buffer[q->end]=rec;
    q->count++;
    pthread_mutex_unlock(&q->mutex);
    pthread_cond_signal(&q->nonempty);
}

struct work_record *buffer_queue_pop(Buffer_Queue q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count==0)
        pthread_cond_wait(&q->nonempty,&q->mutex);
    struct work_record *retval;
    retval=q->buffer[q->start];
    q->start=(q->start+1)%q->size;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    pthread_cond_signal(&q->nonfull);
    return retval;
}

void buffer_queue_free(Buffer_Queue q) {
    if (q!=NULL) {
        free(q->buffer);
        pthread_cond_destroy(&q->nonempty);
        pthread_cond_destroy(&q->nonfull);
        pthread_mutex_destroy(&q->mutex);    
        free(q);
    }
}