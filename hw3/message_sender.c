#include "message_slot.h"
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   
#include <linux/module.h>   
#include <linux/fs.h>       
#include <linux/uaccess.h>  
#include <linux/string.h>   
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#define SUCCESS 0
#define MSG_MAX_LENGTH 128
#define MAX_DEVICES ((1u<<8u)+1)

typedef struct _ListNode {
    int channelId;
    char *msg;
    int msgLength;
    struct _ListNode *next;
} ListNode;

typedef struct _LinkedList {
    ListNode *first;
    int size;
} LinkedList;

static LinkedList *devices[MAX_DEVICES] = {NULL};

ListNode *findChannelId(LinkedList *lst, int channelId) {
    ListNode *node = lst->first;
    while (node != NULL) {
        if (node->channelId == channelId) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

void setMsg(ListNode *node, const char *msg, int msgLength) {
    memcpy(node->msg, msg, msgLength);
    node->msgLength = msgLength;
}
ListNode *createNode(int channelId) {
    ListNode *node;
    node = kmalloc(sizeof(ListNode), GFP_KERNEL);
    if (node == NULL) return NULL;
    node->msg = kmalloc(sizeof(char) * MSG_MAX_LENGTH, GFP_KERNEL);
    if (node->msg == NULL) {
        kfree(node);
        return NULL;
    }
    node->channelId = channelId;
    node->next = NULL;
    return node;
}
ListNode *getOrCreateNode(LinkedList *lst, int channelId) {
    ListNode *newNode, *prevNode, *node;
    if (lst->size == 0) {
        newNode = createNode(channelId);
        if (newNode == NULL) return NULL;
        lst->first = newNode;
        lst->size++;
        return newNode;
    } else {
        prevNode = NULL;
        node = lst->first;
        do {
            if (channelId == node->channelId) {
                return node;
            } else if (channelId < node->channelId) {
                newNode = createNode(channelId);
                if (newNode == NULL) return NULL;
                if (prevNode == NULL) {
                    newNode->next = node;
                    lst->first = newNode;
                } else {
                    newNode->next = node;
                    prevNode->next = newNode;
                }
                lst->size++;
                return newNode;
            }
            prevNode = node;
            node = node->next;
        } while (node != NULL);

        newNode = createNode(channelId);
        if (newNode == NULL) return NULL;
        prevNode->next = newNode;
        lst->size++;
        return newNode;
    }
}

void freeNode(ListNode *node) {
    kfree(node->msg);
    kfree(node);
}

void freeLst(LinkedList *lst) {
    ListNode *prev, *cur;
    if (lst->size!=0) {
        prev = lst->first;
        cur = prev->next;
        while (cur != NULL) {
            freeNode(prev);
            prev = cur;
            cur = cur->next;
        }
        freeNode(prev);
    }
    kfree(lst);
}
static int device_open(struct inode *inode,
                       struct file *file) {
    int minor;
    printk("Invoking device_open(%p)\n", file);

    minor = iminor(inode);
    if (devices[minor + 1] == NULL) {
        printk("adding minor %d to array\n", minor);
        devices[minor + 1] = kmalloc(sizeof(LinkedList), GFP_KERNEL);
        if (devices[minor + 1] == NULL) return -1;
        devices[minor + 1]->size = 0;
    }
    return SUCCESS;
}
static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset) {
    int status, minor;
    unsigned long channelId;
    LinkedList *lst;
    ListNode *node;
    char *tmpBuffer;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channelId = (unsigned long) file->private_data;
    minor = iminor(file->f_inode);
    lst = devices[minor + 1];
    if (devices[minor+1] == NULL) {  
        return -EINVAL;
    }
    node = findChannelId(lst, channelId);
    if (node != NULL) {
        if (length < node->msgLength || buffer == NULL) {  
            return -ENOSPC;
        }
        if ((tmpBuffer = kmalloc(sizeof(char) * node->msgLength, GFP_KERNEL)) == NULL) {  
            return -ENOSPC;
        }
        memcpy(tmpBuffer, node->msg, node->msgLength); 
        if ((status = copy_to_user(buffer, tmpBuffer, node->msgLength))!=0) { 
            return -ENOSPC;
        }
        kfree(tmpBuffer);
        return node->msgLength;
    }
    return -EWOULDBLOCK;
}
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param) {
    if (MSG_SLOT_CHANNEL == ioctl_command_id && ioctl_param != 0) {
        file->private_data = (void *) ioctl_param;
        printk("Invoking ioctl: setting channelId flag to %ld\n", ioctl_param);
        return SUCCESS;
    } else {
        return -EINVAL;
    }
}
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset) {

    unsigned long channelId;
    int status, minor;
    char *tmpBuffer;
    ListNode *node;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    if (length > BUF_LEN || length <= 0) {
        return -EMSGSIZE;
    }
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channelId = (unsigned long) file->private_data;
    if (buffer == NULL) {
        return -ENOSPC;
    }

    minor = iminor(file->f_inode);
    if (devices[minor+1] == NULL) {
        return -EINVAL;
    }

    if ((node = getOrCreateNode(devices[minor + 1], channelId)) == NULL) {
        return -ENOSPC;
    }
    node->msgLength = length;
    if ((tmpBuffer = kmalloc(sizeof(char) * length, GFP_KERNEL)) == NULL) {
        return -ENOSPC;
    }
    if ((status = copy_from_user(tmpBuffer, buffer, length))!=0) {
        return -ENOSPC;
    }
    setMsg(node, tmpBuffer, length);
    kfree(tmpBuffer);
    return length;
}
struct file_operations Fops =
        {
                .owner      = THIS_MODULE,
                .read           = device_read,
                .write          = device_write,
                .open           = device_open,
                .unlocked_ioctl          = device_ioctl,
        };
static int __init simple_init(void) {
    int success;
    printk("registering with major %d and name %s\n", MAJOR_NUM, DEVICE_RANGE_NAME);
    success = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    if (success < 0) {
        printk(KERN_ERR "%s registraion failed for  %d. received status %d\n",
               DEVICE_RANGE_NAME, MAJOR_NUM, success);
        return MAJOR_NUM;
    }
    printk("Registeration is successful. YAY");
    return 0;
}
static void __exit simple_cleanup(void) {
    LinkedList **tmp, **limit;
    limit = devices + MAX_DEVICES;
    printk("freeing memory\n");
    for (tmp = devices + 1; tmp < limit; tmp++) {
        if (*tmp != NULL) {
            freeLst(*tmp);
        }
    }
    printk("done freeing memory\n");
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    printk("successfully unregistered\n");
}
module_init(simple_init);
module_exit(simple_cleanup);
