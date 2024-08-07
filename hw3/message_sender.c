#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include "message_slot.h"

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file> <channel> <message>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_WRONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(1);
    }

    unsigned int channel_id = atoi(argv[2]);
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("ioctl failed");
        close(fd);
        exit(1);
    }

    if (write(fd, argv[3], strlen(argv[3])) < 0) {
        perror("write failed");
        close(fd);
        exit(1);
    }

    close(fd);
    return 0;
}
