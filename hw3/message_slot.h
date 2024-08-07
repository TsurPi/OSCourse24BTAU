#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define MAX_MESSAGE_LENGTH 128
#define MAX_CHANNELS 220

#endif // MESSAGE_SLOT_H