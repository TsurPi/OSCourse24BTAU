#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024

void print_error_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *file_path = argv[3];

    // Open the file for reading
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        print_error_and_exit("Error opening file");
    }

    // Create a socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        close(file_fd);
        print_error_and_exit("Error creating socket");
    }

    // Convert IP address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Invalid IP address");
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Error connecting to server");
    }

    // Get the file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size < 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Error determining file size");
    }
    lseek(file_fd, 0, SEEK_SET);

    // Send the file size (N)
    uint32_t net_file_size = htonl((uint32_t)file_size);
    if (send(sock_fd, &net_file_size, sizeof(net_file_size), 0) < 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Error sending file size");
    }

    // Send the file content
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_sent;
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t result = send(sock_fd, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (result < 0) {
                close(file_fd);
                close(sock_fd);
                print_error_and_exit("Error sending file content");
            }
            bytes_sent += result;
        }
    }

    if (bytes_read < 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Error reading file");
    }

    // Receive the number of printable characters (C)
    uint32_t net_printable_count;
    if (recv(sock_fd, &net_printable_count, sizeof(net_printable_count), 0) < 0) {
        close(file_fd);
        close(sock_fd);
        print_error_and_exit("Error receiving printable character count");
    }
    uint32_t printable_count = ntohl(net_printable_count);

    // Print the result
    printf("# of printable characters: %u\n", printable_count);

    // Clean up
    close(file_fd);
    close(sock_fd);

    return 0;
}
