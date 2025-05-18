typedef struct string * String;

String string_create(int cap);
void string_free(String s);
int string_push(String s,char data);
int string_cpy(String s,char *src);
int string_pos(String s,int pos);
int string_length(String s);
const char *string_ptr(String s);