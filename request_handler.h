#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <unistd.h>
#include "request.h"
#include "response.h"

void handle_request(int client_fd);
char *request_read_all(int client_fd, size_t *total_read);
int handle_get(Request *request, int client_fd);
int handle_put(Request *request, int client_fd);
void send_400(int client_fd);
void send_403(int client_fd);
void send_404(int client_fd);
void send_500(int client_fd);
void send_501(int client_fd);
void send_505(int client_fd);

#endif // request_handler_H
