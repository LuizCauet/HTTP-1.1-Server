#include "response.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

Response *new_response(int status_code) {
    Response *response = calloc(1, sizeof(Response));
    if (!response)
        return NULL;

    response->status_code = status_code;

    // Figure out phrase based on code;
    if (status_code == 200)
        response->status_phrase = "OK";
    else if (status_code == 201)
        response->status_phrase = "Created";
    else if (status_code == 400)
        response->status_phrase = "Bad Request";
    else if (status_code == 403)
        response->status_phrase = "Forbidden";
    else if (status_code == 404)
        response->status_phrase = "Not Found";
    else if (status_code == 500)
        response->status_phrase = "Internal Server Error";
    else if (status_code == 501)
        response->status_phrase = "Not Implemented";
    else if (status_code == 505)
        response->status_phrase = "Version Not Supported";
    else
        response->status_phrase = "Bad Status Code";

    return response;
}

int send_response(Response *response, int client_fd, const char *body, size_t body_length) {
    if (!response)
        return 1;

    char status_line[256];
    int sl = snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", response->status_code,
        response->status_phrase);

    write(client_fd, status_line, sl);

    char headers[256];
    int hl = snprintf(headers, sizeof(headers),
        "Content-Length: %zu\r\n"
        "\r\n",
        body_length);

    write(client_fd, headers, hl);

    if (body && body_length > 0) {
        size_t written = 0;
        while (written < body_length) {
            ssize_t n = write(client_fd, body + written, body_length - written);
            if (n < 0) {
                perror("body write failed");
                return -1;
            }
            written += n;
        }
    }

    return 0;
}

void free_response(Response *response) {
    free(response);
}
