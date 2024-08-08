#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define PRINTABLE_CHAR_MIN 32
#define PRINTABLE_CHAR_MAX 126
#define NUM_PRINTABLE_CHARS (PRINTABLE_CHAR_MAX - PRINTABLE_CHAR_MIN + 1)

uint32_t pcc_total[NUM_PRINTABLE_CHARS] = {0};

void handle_sigint(int sig) {
    for (int i = 0; i < NUM_PRINTABLE_CHARS; ++i) {
        printf("char '%c' : %u times\n", i + PRINTABLE_CHAR_MIN, pcc_total[i]);
    }
    exit(EXIT_SUCCESS);
}

void print_error_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void count_printable_characters(int client_fd) {
    uint32_t net_file_size;
    ssize_t received_bytes = recv(client_fd, &net_file_size, sizeof(net_file_size), 0);
    if (received_bytes <= 0) {
        if (received_bytes == 0) {
            fprintf(stderr, "Client closed the connection unexpectedly\n");
        } else {
            perror("Error receiving file size");
        }
        return;
    }

    uint32_t file_size = ntohl(net_file_size);
    uint32_t printable_count = 0;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    uint32_t printable_chars_local[NUM_PRINTABLE_CHARS] = {0};

    while (file_size > 0) {
        ssize_t to_read = (file_size < BUFFER_SIZE) ? file_size : BUFFER_SIZE;
        bytes_read = recv(client_fd, buffer, to_read, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                fprintf(stderr, "Client closed the connection unexpectedly\n");
            } else {
                perror("Error receiving file content");
            }
            return;
        }
        for (ssize_t i = 0; i < bytes_read; ++i) {
            if (buffer[i] >= PRINTABLE_CHAR_MIN && buffer[i] <= PRINTABLE_CHAR_MAX) {
                printable_chars_local[buffer[i] - PRINTABLE_CHAR_MIN]++;
                printable_count++;
            }
        }
        file_size -= bytes_read;
    }

    uint32_t net_printable_count = htonl(printable_count);
    if (send(client_fd, &net_printable_count, sizeof(net_printable_count), 0) < 0) {
        perror("Error sending printable character count");
        return;
    }

    for (int i = 0; i < NUM_PRINTABLE_CHARS; ++i) {
        pcc_total[i] += printable_chars_local[i];
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[1]);

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        print_error_and_exit("Error setting up SIGINT handler");
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        print_error_and_exit("Error creating socket");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd);
        print_error_and_exit("Error setting socket options");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        print_error_and_exit("Error binding socket");
    }

    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        print_error_and_exit("Error listening on socket");
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                break;  // Interrupted by signal, exit gracefully
            } else {
                perror("Error accepting connection");
                continue;
            }
        }
        count_printable_characters(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
