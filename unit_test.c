#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "request.h"

#include "thread_pool.h"

int main(int argc, char *argv[]) {

    (void) argc;
    (void) argv;

    // TESTING Request Parsing
    char test[] = "PUT /hello.txt HTTP/1.1\r\n"
                  "Host: example.com\r\n"
                  "Content-Length: 5\r\n"
                  "Request-ID: 42\r\n"
                  "\r\n"
                  "ABCDE";

    Request *req = new_request(test, sizeof(test) - 1);

    print_request(req);
    // END OF PARSING TESTING

    return EXIT_SUCCESS;
}
