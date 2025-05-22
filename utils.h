enum return_codes {
    ARGS_ERR=1,
    ALLOC_ERR,
    FOPEN_ERR,
    SOCK_ERR,
    USED_ADDR,
    BIND_ERR,
    LISTEN_ERR,
    ACCEPT_ERR,
    PTHREAD_ERR,
    INVALID_IP,
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CMD) { \
    buffer_queue_free(work_queue); \
    buffer_queue_free(producers_queue); \
    free(workers); \
    free(file_producers); \
    if (config_file != NULL) fclose(config_file); \
    PRINT_CMD; \
    RETURN_CMD; \
}
