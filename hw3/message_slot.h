#ifndef MSGSLOT_MESSAGE_SLOT_H
#define MSGSLOT_MESSAGE_SLOT_H

#include <linux/ioctl.h> // Include the ioctl header for ioctl command definitions

// Define the major number for the device
#define MAJOR_NUM 240

// Define an ioctl command to set the message slot channel
// _IOW is a macro that creates an ioctl command that writes a value to the device
// MAJOR_NUM is the major number associated with the device
// 0 is the command identifier
// unsigned long is the type of data that will be passed with this command
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

// Define the name of the device as it will appear in /dev
#define DEVICE_RANGE_NAME "message_slot"

// Define the maximum buffer length for messages
#define BUF_LEN 128

// Define a success status code
#define SUCCESS 0

#endif // End of header file inclusion guard
