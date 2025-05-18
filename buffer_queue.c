#include <stdlib.h>
#include "buffer_queue.h"

struct buffer_queue {
    int *buffer;    // Will put something else
    int start;
    int end;
    int count;
    int size;
};

Buffer_Queue buffer_queue_create(int size) {
    Buffer_Queue q = malloc(sizeof(struct buffer_queue));
    if (q==NULL)
        return NULL;
    q->buffer = malloc(size*sizeof(int));
    if (q->buffer==NULL) {
        free(q);
        return NULL;
    }
    q->start = 0;
    q->end = -1;
    q->count = 0;
    q->size = size;
    return q;
}

void buffer_queue_free(Buffer_Queue q) {
    if (q!=NULL) {
        free(q->buffer);
        free(q);
    }
}
