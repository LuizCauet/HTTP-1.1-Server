#include "request.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include "thread_pool.h"

static thread_pool_t *GLOBAL_POOL = NULL;
static int LISTEN_FD = -1;

void cleanup(void) {
    if (GLOBAL_POOL) {
        thread_pool_destroy(GLOBAL_POOL);
        GLOBAL_POOL = NULL;
    }
}

void handle_shutdown(int sig) {
    (void) sig;

    printf("Shutting down...\n");

    if (GLOBAL_POOL) {
        thread_pool_shutdown(GLOBAL_POOL);
    }

    if (LISTEN_FD != -1) {
        close(LISTEN_FD);
        LISTEN_FD = -1;
    }

    exit(0);
}

int main(int argc, char *argv[]) {

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);
    atexit(cleanup);

    int parrelism = 1;
    int port = 0;

    if (argc == 2) {

        for (unsigned long i = 0; i < strlen(argv[1]); i++) {
            if (!isdigit(argv[1][i])) {
                printf("Port must be an integer in [1,65535]\n");
                return 0;
            }
        }

        port = atoi(argv[1]);

        if (port < 1 || port > 65535) {
            printf("Port must be an integer in [1,65535]\n");
            return 0;
        }

    } else if (argc == 4) {
        if (strcmp("-p", argv[1]) == 0) {

            if (argc != 4) {
                printf("Usage: ./httpserver [-p parallelism] <port> \n");
                return 0;
            } else {

                for (unsigned long i = 0; i < strlen(argv[2]); i++) {
                    if (!isdigit(argv[2][i])) {
                        printf("Parallelism must be an integer\n");
                        return 0;
                    }
                }

                for (unsigned long i = 0; i < strlen(argv[3]); i++) {
                    if (!isdigit(argv[3][i])) {
                        printf("Port must be an integer in [1,65535]\n");
                        return 0;
                    }
                }

                parrelism = atoi(argv[2]);
                port = atoi(argv[3]);

                if (port < 1 || port > 65535) {
                    printf("Port must be an integer in [1,65535]\n");
                    return 0;
                }
            }
        }
    } else {
        printf("Usage: ./httpserver [-p parallelism] <port> \n");
        return 0;
    }

    // Create the thread pool
    thread_pool_t *pool = thread_pool_create(parrelism);
    GLOBAL_POOL = pool;

    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    LISTEN_FD = socket_fd;

    // Allow fast reuse of the port after restart
    int yes = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
    }

    // Bind socket to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    int bind_result = bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr));
    if (bind_result < 0) {
        perror("bind failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    int listen_result = listen(socket_fd, 10);
    if (listen_result < 0) {
        perror("listen failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // printf("Listening on port %d with parrelism %d\n", port, parrelism);

    // Accept connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &len);
        if (client_fd < 0) {
            perror("accept failed");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        // printf("Accepted connection on fd %d\n", client_fd);
        //  give the client fd to the thread pool
        thread_pool_add(pool, client_fd);
    }

    close(socket_fd);
    thread_pool_destroy(pool);
    return EXIT_SUCCESS;
}
