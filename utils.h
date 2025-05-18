enum return_codes {
    ARGS_ERR=1,
    ALLOC_ERR,
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CODE) { \
    buffer_queue_free(work_queue); \
    PRINT_CMD; \
    return RETURN_CODE; \
}
