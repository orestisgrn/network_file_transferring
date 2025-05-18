typedef struct buffer_queue * Buffer_Queue;

Buffer_Queue buffer_queue_create(int size);
void buffer_queue_free(Buffer_Queue q);