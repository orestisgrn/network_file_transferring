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
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CODE) { \
    buffer_queue_free(work_queue); \
    if (config_file != NULL) fclose(config_file); \
    PRINT_CMD; \
    return RETURN_CODE; \
}
