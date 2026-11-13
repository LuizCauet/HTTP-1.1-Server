#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

typedef struct {
    char *method;
    char *uri;
    char *version;

} Request_Line;

typedef struct {
    char *key;
    char *value;
} Header;

typedef struct {
    Request_Line request_line;
    Header *header_list;
    size_t header_count;
    size_t header_capacity;
    char *body;
    size_t body_length;
    char *raw_buffer;
} Request;

Request *new_request(char *request, size_t request_len);

void print_request(Request *req);

void free_request(Request *req);

#endif
