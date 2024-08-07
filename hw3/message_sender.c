#include <stdlib.h>      // For standard library functions like exit, strtol
#include <stdio.h>       // For standard I/O functions like printf, perror
#include <errno.h>       // For error handling
#include <string.h>      // For string handling functions
#include <limits.h>      // For defining data type limits, like UINT_MAX
#include <fcntl.h>       // For file control options (open flags)
#include <unistd.h>      // For standard symbolic constants and types, like STDOUT_FILENO
#include <sys/ioctl.h>   // For ioctl system call
#include "message_slot.h" // Include header for message slot ioctl definitions

// Function to close the file descriptor
void clean(int fd) {
    close(fd);
}

// Function to close the file descriptor and exit the program with an error
void exitAndClean(int fd) {
    clean(fd);
    exit(errno); // Exit with the current error number
}

int main(int c, char **args) {
    char *deviceFile, *msg;     // Pointers to hold device file path and message
    long channelId;             // Variable to store channel ID
    int fd;                     // File descriptor

    // Check if the number of arguments is correct (expected 4 arguments)
    if (c != 4) {
        perror("Incorrect number of arguments provided");
        exit(1); // Exit if incorrect number of arguments
    }

    // Assign the arguments to respective variables
    deviceFile = args[1];
    channelId = strtol(args[2], NULL, 10); // Convert string to long integer for channelId
    if (channelId == UINT_MAX && errno == ERANGE) {
        perror("Invalid channel ID provided");
        exit(1); // Exit if conversion fails or channel ID is out of range
    }
    msg = args[3]; // Message to be sent

    // Open the device file
    fd = open(deviceFile, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device file");
        clean(fd); // Clean up if file cannot be opened
    }

    // Set the message slot channel using ioctl
    if (ioctl(fd, MSG_SLOT_CHANNEL, (unsigned long) channelId) < 0) {
        perror("Failed to set channel ID using ioctl");
        exitAndClean(fd); // Exit and clean up if ioctl fails
    }

    // Write the message to the message slot
    if (write(fd, msg, strlen(msg)) < 0) {
        perror("Failed to write the full message to the device");
        exitAndClean(fd); // Exit and clean up if writing fails
    }

    close(fd); // Close the file descriptor
}
