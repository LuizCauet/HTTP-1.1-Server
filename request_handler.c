#define _GNU_SOURCE

#include "request_handler.h"
#include "request.h"
#include "response.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_LOCKS 1024
typedef struct {
    char *path;
    pthread_rwlock_t rwlock;
} FileLock;

static FileLock file_locks[MAX_LOCKS];
static size_t num_locks = 0;
static pthread_mutex_t lock_table_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audit_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_rwlock_t *get_file_lock(const char *path) {
    pthread_rwlock_t *res = NULL;
    pthread_mutex_lock(&lock_table_mutex);
    for (size_t i = 0; i < num_locks; i++) {
        if (strcmp(file_locks[i].path, path) == 0) {
            res = &file_locks[i].rwlock;
            break;
        }
    }
    if (!res && num_locks < MAX_LOCKS) {
        file_locks[num_locks].path = strdup(path);
        pthread_rwlock_init(&file_locks[num_locks].rwlock, NULL);
        res = &file_locks[num_locks].rwlock;
        num_locks++;
    }
    pthread_mutex_unlock(&lock_table_mutex);
    return res;
}

void handle_request(int client_fd) {
    //sleep(3);
    int status_code = 0;
    int request_id = 0;
    Request *this_request = NULL;

    size_t total_read = 0;
    char *buffer = request_read_all(client_fd, &total_read);

    if (!buffer) {
        status_code = 500;
        send_500(client_fd);
        goto done;
    }

    //printf("Read data from fd %d:\n", client_fd);

    this_request = new_request(buffer, total_read);

    if (!this_request) {

        status_code = 400;
        send_400(client_fd);
        goto done;
    }
    for (size_t i = 0; i < this_request->header_count; i++) {
        if (strcasecmp(this_request->header_list[i].key, "Request-ID") == 0) {
            request_id = atoi(this_request->header_list[i].value);
            break;
        }
    }

    if (strcmp(this_request->request_line.version, "HTTP/1.1") != 0) {
        status_code = 505;
        send_505(client_fd);
        goto done;
    }

    if (strcmp(this_request->request_line.method, "PUT") == 0) {
        status_code = handle_put(this_request, client_fd);
        goto done;
    } else if (strcmp(this_request->request_line.method, "GET") == 0) {
        status_code = handle_get(this_request, client_fd);
        goto done;
    } else {
        status_code = 501;
        send_501(client_fd);
        goto done;
    }

    /*<METHOD>,<URI>,<Status-Code>,<Request-ID>\n
where <METHOD> is the request method, <URI> is the request URI, <Status-Code> is the re-
turned status code, and <Request-ID> is the request ID. To identify the request-ID, your
server should look for the optional header, Request-ID, from the request. If the header is
not included in a request, it should treat the request-ID as the value 0.*/

done:

    pthread_mutex_lock(&audit_mutex);
    if (this_request) {
        fprintf(stderr, "%s,%s,%d,%d\n", this_request->request_line.method,
            this_request->request_line.uri, status_code, request_id);
    } else {
        fprintf(stderr, "-,-,%d,%d\n", status_code, request_id);
    }
    fflush(stderr);
    pthread_mutex_unlock(&audit_mutex);

    if (this_request) {
        free_request(this_request);
    }
    if (buffer)
        free(buffer);
}

char *request_read_all(int client_fd, size_t *total_read) {
    size_t buf_size = 4096;
    char *buf = malloc(buf_size);
    memset(buf, 0, buf_size);
    if (!buf) {
        perror("malloc");
        return NULL;
    }

    ssize_t total = 0;
    char *headers_end = NULL;

    while (1) {
        if (total >= (ssize_t) (buf_size - 1)) {

            buf_size *= 2;
            char *new_buf = realloc(buf, buf_size);
            if (!new_buf) {
                perror("realloc");
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }

        ssize_t n = read(client_fd, buf + total, buf_size - 1 - total);
        if (n < 0) {
            perror("read");
            free(buf);
            return NULL;
        }
        if (n == 0) {
            break;
        }
        total += n;

        for (ssize_t i = 0; i <= total - 4; i++) {
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
                headers_end = buf + i + 4;
                break;
            }
        }

        if (headers_end) {
            break;
        }
    }

    char *cl_header = strstr(buf, "Content-Length:");
    if (!cl_header) {
        cl_header = strstr(buf, "content-length:");
    }

    if (cl_header) {
        int content_length = atoi(cl_header + 15);

        char *body_start = strstr(buf, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            size_t headers_len = body_start - buf;
            size_t body_read_so_far = total - headers_len;
            size_t body_remaining = content_length - body_read_so_far;
            size_t total_needed = headers_len + content_length;

            if (total_needed >= buf_size) {
                buf_size = total_needed + 1;
                char *new_buf = realloc(buf, buf_size);
                if (!new_buf) {
                    perror("realloc");
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }

            while (body_remaining > 0) {
                ssize_t n = read(client_fd, buf + total, body_remaining);
                if (n < 0) {
                    perror("read body");
                    free(buf);
                    return NULL;
                }
                if (n == 0) {
                    break;
                }
                total += n;
                body_remaining -= n;
            }
        }
    }

    buf[total] = '\0';
    *total_read = total;
    return buf;
}

int handle_get(Request *request, int client_fd) {
    const char *uri = request->request_line.uri;
    const char *path = (uri[0] == '/') ? uri + 1 : uri;

    pthread_rwlock_t *file_lock = get_file_lock(path);
    if (file_lock) {
        pthread_rwlock_rdlock(file_lock);
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        if (file_lock)
            pthread_rwlock_unlock(file_lock);
        send_404(client_fd);
        return 404;
    }

    if (!S_ISREG(st.st_mode)) {
        if (file_lock)
            pthread_rwlock_unlock(file_lock);
        send_403(client_fd);
        return 403;
    }

    size_t file_size = (size_t) st.st_size;
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (file_lock)
            pthread_rwlock_unlock(file_lock);
        send_500(client_fd);
        return 500;
    }

    char *buf = NULL;
    if (file_size > 0) {
        buf = malloc(file_size);
        if (!buf) {
            fclose(f);
            if (file_lock)
                pthread_rwlock_unlock(file_lock);
            send_500(client_fd);
            return 500;
        }

        size_t read_bytes = fread(buf, 1, file_size, f);
        if (read_bytes != file_size) {
            free(buf);
            fclose(f);
            if (file_lock)
                pthread_rwlock_unlock(file_lock);
            send_500(client_fd);
            return 500;
        }
    }

    fflush(f);
    int fdnum = fileno(f);
    if (fdnum >= 0) {
        fsync(fdnum);
    }
    fclose(f);

    Response *resp = new_response(200);
    send_response(resp, client_fd, buf, file_size);

    if (file_lock)
        pthread_rwlock_unlock(file_lock);

    free(buf);
    free_response(resp);
    close(client_fd);
    //printf("Closed connection with client_fd %d\n", client_fd);
    return 200;
}
int handle_put(Request *request, int client_fd) {
    const char *uri = request->request_line.uri;
    const char *path = (uri[0] == '/') ? uri + 1 : uri;

    if (*path == '\0') {
        send_400(client_fd);
        return 400;
    }

    pthread_rwlock_t *file_lock = get_file_lock(path);
    if (file_lock) {
        pthread_rwlock_wrlock(file_lock);
    }

    struct stat st;
    int existed_before = (stat(path, &st) == 0);

    FILE *f = fopen(path, "wb");
    if (!f) {
        if (file_lock)
            pthread_rwlock_unlock(file_lock);
        if (errno == EACCES || errno == EISDIR) {
            send_403(client_fd);
            return 403;
        } else {
            send_500(client_fd);
            return 500;
        }
    }

    size_t total = 0, to_write = request->body_length;
    while (total < to_write) {
        size_t n = fwrite(request->body + total, 1, to_write - total, f);

        if (n == 0) {
            break;
        }

        if (n == 0 && ferror(f)) {
            fclose(f);
            if (file_lock)
                pthread_rwlock_unlock(file_lock);
            send_500(client_fd);
            return 500;
        }
        total += n;
    }

    fclose(f);

    Response *resp = new_response(existed_before ? 200 : 201);
    const char *body = existed_before ? "OK\n" : "Created\n";

    send_response(resp, client_fd, body, strlen(body));

    if (file_lock)
        pthread_rwlock_unlock(file_lock);

    free_response(resp);
    close(client_fd);
    return existed_before ? 200 : 201;
}

void send_400(int client_fd) {
    Response *response = new_response(400);
    const char *body = "Bad Request\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
void send_403(int client_fd) {
    Response *response = new_response(403);
    const char *body = "Forbidden\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
void send_404(int client_fd) {
    Response *response = new_response(404);
    const char *body = "Not Found\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
void send_500(int client_fd) {
    Response *response = new_response(500);
    const char *body = "Internal Server Error\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
void send_501(int client_fd) {
    Response *response = new_response(501);
    char *body = "Not Implemented\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
void send_505(int client_fd) {
    Response *response = new_response(505);
    const char *body = "Version Not Supported\n";

    send_response(response, client_fd, body, strlen(body));
    free_response(response);
    close(client_fd);
    printf("Closed connection with client_fd %d\n", client_fd);
}
