#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include "message_slot.h"

struct channel_t {
    unsigned int id;
    char message[MAX_MESSAGE_LENGTH];
    int message_len;
    struct channel_t* next;
};

struct message_slot_t {
    struct channel_t* channels;
};

static struct message_slot_t* slots[MAX_CHANNELS];

// Function prototypes
static int device_open(struct inode* inode, struct file* file);
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param);
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
static int device_release(struct inode* inode, struct file* file);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .write = device_write,
    .read = device_read,
    .release = device_release,
};

// Helper function to find a channel by id
static struct channel_t* find_channel(struct message_slot_t* slot, unsigned int id) {
    struct channel_t* channel = slot->channels;
    while (channel) {
        if (channel->id == id) {
            return channel;
        }
        channel = channel->next;
    }
    return NULL;
}

static int device_open(struct inode* inode, struct file* file) {
    int minor = iminor(inode);

    if (minor >= MAX_CHANNELS) {
        return -ENODEV;
    }

    if (slots[minor] == NULL) {
        slots[minor] = kmalloc(sizeof(struct message_slot_t), GFP_KERNEL);
        if (!slots[minor]) {
            printk(KERN_ERR "message_slot: Failed to allocate memory for message slot.\n");
            return -ENOMEM;
        }
        slots[minor]->channels = NULL;
    }

    file->private_data = slots[minor];
    return 0;
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    struct message_slot_t* slot;
    struct channel_t* channel;
    unsigned int channel_id = (unsigned int)ioctl_param;

    if (ioctl_command_id != MSG_SLOT_CHANNEL || channel_id == 0) {
        return -EINVAL;
    }

    slot = (struct message_slot_t*)file->private_data;
    if (!slot) {
        return -EINVAL;
    }

    channel = find_channel(slot, channel_id);
    if (!channel) {
        channel = kmalloc(sizeof(struct channel_t), GFP_KERNEL);
        if (!channel) {
            return -ENOMEM;
        }
        channel->id = channel_id;
        channel->message_len = 0;
        channel->next = slot->channels;
        slot->channels = channel;
    }

    file->private_data = channel;
    return 0;
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
    struct channel_t* channel = (struct channel_t*)file->private_data;

    if (!channel) {
        return -EINVAL;
    }

    if (length == 0 || length > MAX_MESSAGE_LENGTH) {
        return -EMSGSIZE;
    }

    if (buffer == NULL) {
        return -EINVAL;
    }

    if (copy_from_user(channel->message, buffer, length)) {
        return -EFAULT;
    }

    channel->message_len = length;
    return length;
}

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
    struct channel_t* channel = (struct channel_t*)file->private_data;

    if (!channel) {
        return -EINVAL;
    }

    if (channel->message_len == 0) {
        return -EWOULDBLOCK;
    }

    if (length < channel->message_len) {
        return -ENOSPC;
    }

    if (buffer == NULL) {
        return -EINVAL;
    }

    if (copy_to_user(buffer, channel->message, channel->message_len)) {
        return -EFAULT;
    }

    return channel->message_len;
}

static int device_release(struct inode* inode, struct file* file) {
    return 0;
}

static int __init message_slot_init(void) {
    int result = register_chrdev(MAJOR_NUM, "message_slot", &fops);
    if (result < 0) {
        printk(KERN_ERR "message_slot: Failed to register device.\n");
        return result;
    }
    printk(KERN_INFO "message_slot: Module loaded.\n");
    return 0;
}

static void __exit message_slot_cleanup(void) {
    int i;
    struct channel_t* channel;
    struct channel_t* tmp;

    for (i = 0; i < MAX_CHANNELS; i++) {
        if (slots[i]) {
            channel = slots[i]->channels;
            while (channel) {
                tmp = channel;
                channel = channel->next;
                kfree(tmp);
            }
            kfree(slots[i]);
        }
    }

    unregister_chrdev(MAJOR_NUM, "message_slot");
    printk(KERN_INFO "message_slot: Module unloaded.\n");
}

module_init(message_slot_init);
module_exit(message_slot_cleanup);

MODULE_LICENSE("GPL");
