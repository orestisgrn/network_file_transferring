#include <netinet/in.h>
#include "string.h"

typedef struct buffer_queue * Buffer_Queue;

struct work_record {
    struct sockaddr_in sock_tuple[2];
    String source_dir;
    String target_dir;
    String file;
};

Buffer_Queue buffer_queue_create(int size);
void buffer_queue_push(Buffer_Queue q,struct work_record *rec);
struct work_record buffer_queue_pop(Buffer_Queue q);
void buffer_queue_free(Buffer_Queue q);