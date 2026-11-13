#ifndef RESPONSE_H
#define RESPONSE_H

#include <stddef.h>

typedef struct {
    int status_code;
    char *status_phrase;
} Response;

Response *new_response(int status_code);

int send_response(Response *response, int client_fd, const char *body, size_t body_length);

void free_response(Response *response);

#endif
