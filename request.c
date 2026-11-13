#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Find "\r\n\r\n" boundary safely
static char *find_double_crlf(char *buf, size_t len) {
    if (len < 4)
        return NULL;

    for (size_t i = 0; i <= len - 4; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return buf + i;
        }
    }
    return NULL;
}

void print_request(Request *req) {
    if (!req) {
        printf("Request is NULL\n");
        return;
    }

    printf("\n===== Parsed Request =====\n");

    // Request line
    printf("Method:  %s\n", req->request_line.method ? req->request_line.method : "(null)");
    printf("URI:     %s\n", req->request_line.uri ? req->request_line.uri : "(null)");
    printf("Version: %s\n", req->request_line.version ? req->request_line.version : "(null)");

    // Headers
    printf("\nHeaders (%zu):\n", req->header_count);
    for (size_t i = 0; i < req->header_count; i++) {
        printf("  %s: %s\n", req->header_list[i].key ? req->header_list[i].key : "(null)",
            req->header_list[i].value ? req->header_list[i].value : "(null)");
    }

    // Body
    printf("\nBody Length: %zu\n", req->body_length);

    if (req->body_length > 0 && req->body) {
        printf("Body (hex): ");
        for (size_t i = 0; i < req->body_length; i++) {
            printf("%02X ", (unsigned char) req->body[i]);
        }
        printf("\n");

        printf("Body (as text, may contain binary): \"");
        fwrite(req->body, 1, req->body_length, stdout);
        printf("\"\n");
    } else {
        printf("Body: (empty)\n");
    }

    printf("==========================\n\n");
}

Request *new_request(char *request_raw, size_t request_len) {
    // printf("Parsing request:\n");

    if (!request_raw)
        return NULL;

    char *working_copy = malloc(request_len + 1);
    if (!working_copy)
        return NULL;

    memcpy(working_copy, request_raw, request_len);
    working_copy[request_len] = '\0';

    Request *req = calloc(1, sizeof(Request));
    if (!req) {
        free(working_copy);
        return NULL;
    }

    req->header_capacity = 8;
    req->header_list = malloc(sizeof(Header) * req->header_capacity);
    if (!req->header_list) {
        free(working_copy);
        free(req);
        return NULL;
    }

    req->raw_buffer = working_copy;

    size_t total_len = request_len;
    char *boundary = find_double_crlf(working_copy, total_len);

    if (boundary) {
        boundary[0] = '\0';
        boundary[1] = '\0';
        boundary[2] = '\0';
        boundary[3] = '\0';
    } else {
        boundary = working_copy + total_len;
    }
    char *body_start = working_copy + total_len;
    if (boundary != working_copy + total_len) {
        body_start = boundary + 4;
    }
    // 1. Parse Request Line

    char *line_save = NULL;
    char *line = strtok_r(working_copy, "\r\n", &line_save);

    int space_count = 0;
    for (size_t i = 0; line[i] != '\0'; i++) {
        if (line[i] == ' ')
            space_count++;
    }
    if (space_count != 2) {
        free_request(req);
        return NULL; // will cause 400
    }

    if (line) {
        char *reqline_save = NULL;
        req->request_line.method = strtok_r(line, " ", &reqline_save);
        req->request_line.uri = strtok_r(NULL, " ", &reqline_save);
        req->request_line.version = strtok_r(NULL, " ", &reqline_save);
    }

    if (!req->request_line.method || !req->request_line.uri || !req->request_line.version) {
        free_request(req);
        return NULL;
    }

    {
        char *method = req->request_line.method;
        size_t method_length = strlen(method);

        if (method_length < 1 || method_length > 8) {
            free_request(req);
            return NULL;
        }

        for (size_t i = 0; i < method_length; i++) {
            if (!((method[i] >= 'A' && method[i] <= 'Z')
                    || (method[i] >= 'a' && method[i] <= 'z'))) {
                free_request(req);
                return NULL;
            }
        }
    }

    // /[A-Za-z0-9.-]{1,63}
    {

        char *uri = req->request_line.uri;

        if (uri[0] != '/') {
            free_request(req);
            return NULL;
        }

        size_t uri_length = strlen(uri + 1);
        if (uri_length < 1 || uri_length > 63) {
            free_request(req);

            printf("URI rejected. Returning NULL.\n");
            return NULL;
        }

        for (size_t i = 1; i < strlen(uri); i++) {
            char c = uri[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                    || c == '.' || c == '-')) {
                free_request(req);
                return NULL;
            }
        }
    }
    {
        char *version = req->request_line.version;
        size_t version_len = strlen(version);

        if (version_len != 8) {
            free_request(req);
            return NULL;
        }

        if (strncmp(version, "HTTP/", 5) != 0) {
            free_request(req);
            return NULL;
        }

        if (version[5] < '0' || version[5] > '9') {
            free_request(req);
            return NULL;
        }

        if (version[6] != '.') {
            free_request(req);
            return NULL;
        }

        if (version[7] < '0' || version[7] > '9') {
            free_request(req);
            return NULL;
        }
    }
    // 2. Parse Headers
    int content_length = 0;

    while ((line = strtok_r(NULL, "\r\n", &line_save)) != NULL) {

        if (line >= boundary || *line == '\0')
            break;

        char *colon = strchr(line, ':');
        if (!colon) {
            free_request(req);
            return NULL;
        }

        *colon = '\0';
        char *key = line;
        char *value = colon + 1;

        while (*value == ' ')
            value++;

        if (strcasecmp(key, "Content-Length") == 0)
            content_length = atoi(value);

        if (req->header_count >= req->header_capacity) {
            req->header_capacity *= 2;
            req->header_list = realloc(req->header_list, sizeof(Header) * req->header_capacity);
        }

        req->header_list[req->header_count].key = key;
        req->header_list[req->header_count].value = value;
        req->header_count++;
    }

    for (size_t i = 0; i < req->header_count; i++) {
        char *key = req->header_list[i].key;
        char *value = req->header_list[i].value;

        size_t key_length = strlen(key);
        if (key_length < 1 || key_length > 128) {
            free_request(req);
            return NULL;
        }
        for (size_t k = 0; k < key_length; k++) {
            char c = key[k];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                    || c == '.' || c == '-')) {
                free_request(req);
                return NULL;
            }
        }

        size_t value_length = strlen(value);
        if (value_length < 1 || value_length > 128) {
            free_request(req);
            return NULL;
        }
        for (size_t v = 0; v < value_length; v++) {
            unsigned char c = value[v];
            if (c < 32 || c > 126) {
                free_request(req);
                return NULL;
            }
        }
    }
    // 3. Get body from body start to (body_start + content_length)
    if (content_length > 0) {
        req->body = malloc(content_length);
        if (req->body) {
            memcpy(req->body, body_start, content_length);
            req->body_length = content_length;
        }
    }

    return req;
}

void free_request(Request *req) {
    if (!req)
        return;
    free(req->raw_buffer);
    free(req->body);
    free(req->header_list);
    free(req);
}
