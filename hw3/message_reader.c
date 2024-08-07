#include <stdlib.h>      // For memory allocation, exit, etc.
#include <stdio.h>       // For standard I/O functions like printf, perror
#include <errno.h>       // For error number handling
#include <string.h>      // For string manipulation functions
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
    char *deviceFile, *msg;     // Pointers to hold device file path and message buffer
    long channelId;             // Variable to store channel ID
    int fd, retVal;             // File descriptor and return value variables

    // Check if the number of arguments is correct
    if (c != 3) {
        printf("wrong amount of arguments given: %s", strerror(1));
        exit(1); // Exit if incorrect number of arguments
    }

    // Assign the arguments to respective variables
    deviceFile = args[1];
    channelId = strtol(args[2], NULL, 10); // Convert string to long integer for channelId
    if (channelId == UINT_MAX && errno == ERANGE) {
        perror("wrong channelId given: %s");
        exit(1); // Exit if conversion fails or out of range
    }

    // Open the device file
    fd = open(deviceFile, O_RDWR);
    if (fd < 0) {
        perror("could not open file: %s");
        exit(1); // Exit if file cannot be opened
    }

    // Allocate memory for the message buffer
    if ((msg = calloc(BUF_LEN, sizeof(char))) == NULL) {
        errno = -1; // Set error number to indicate failure
        exit(1); // Exit if memory allocation fails
    }

    // Set the message slot channel using ioctl
    if (ioctl(fd, MSG_SLOT_CHANNEL, (unsigned long) channelId) < 0) {
        perror("ioctl failed");
        exitAndClean(fd); // Exit and clean up if ioctl fails
    }

    // Read the message from the message slot
    if ((retVal = read(fd, msg, BUF_LEN)) < 0) {
        perror("read failed");
        exitAndClean(fd); // Exit and clean up if reading fails
    }

    clean(fd); // Close the file descriptor

    // Write the message to standard output
    if ((write(STDOUT_FILENO, msg, retVal) != retVal)) {
        perror("failed writing to STDOUT");
    }

    free(msg); // Free the allocated memory for the message buffer
}
