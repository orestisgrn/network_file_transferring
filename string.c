#include <stdio.h>
#include <stdlib.h>
#include "string.h"

struct string {
    char *arr;
    int length;
    int capacity;
};

String string_create(int cap) {
    String s = malloc(sizeof(struct string));
    if (s==NULL) {
        return NULL;
    }
    s->arr = malloc(sizeof(char)*(cap+1));
    if (s->arr==NULL) {
        free(s);
        return NULL;
    }
    s->length=0;
    s->capacity=cap;
    s->arr[0]='\0';
    return s;
}

void string_free(String s) {
    if (s!=NULL) {
        free(s->arr);
        free(s);
    }
}

int string_push(String s,char data) {   // Maybe don't set s->arr as realloc immediately (to keep old array)
    if (s->length==s->capacity) {
        s->capacity*=2;
        s->arr=realloc(s->arr,sizeof(char)*(s->capacity+1));
        if (s->arr==NULL) {
            free(s);
            return -1;
        }
    }
    s->arr[s->length++]=data;
    s->arr[s->length]='\0';

    return 0;
}

int string_cpy(String s,char *src) {
    while(*src!='\0') {
        if (string_push(s,*(src++))==-1)
            return -1;
    }
    return 0;
}

int string_pos(String s,int pos) {
    return s->arr[pos]; 
}

int string_length(String s) {
    return s->length;
}

const char *string_ptr(String s) {
    return s->arr;
}