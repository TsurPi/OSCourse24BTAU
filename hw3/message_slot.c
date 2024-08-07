#include "message_slot.h" // Include header for message slot definitions
#undef __KERNEL__         // Undefine __KERNEL__ if previously defined
#define __KERNEL__        // Define __KERNEL__ for kernel module code
#undef MODULE             // Undefine MODULE if previously defined
#define MODULE            // Define MODULE for kernel module code

#include <linux/kernel.h>  // For kernel functions like printk
#include <linux/module.h>  // For module initialization and cleanup macros
#include <linux/fs.h>      // For file operations structure and functions
#include <linux/uaccess.h> // For copy_to_user and copy_from_user functions
#include <linux/string.h>  // For string manipulation functions like memcpy
#include <linux/slab.h>    // For memory allocation functions

MODULE_LICENSE("GPL");     // Specify the module's license as GPL
#define SUCCESS 0          // Define SUCCESS as 0 for successful operations
#define MSG_MAX_LENGTH 128 // Define maximum message length
#define MAX_DEVICES ((1u<<8u)+1) // Define maximum number of devices (256 + 1)

typedef struct _ListNode {
    int channelId;        // Channel ID for the node
    char *msg;            // Pointer to message
    int msgLength;        // Length of the message
    struct _ListNode *next; // Pointer to the next node in the list
} ListNode;

typedef struct _LinkedList {
    ListNode *first;      // Pointer to the first node in the list
    int size;             // Size of the linked list
} LinkedList;

static LinkedList *devices[MAX_DEVICES] = {NULL}; // Array to store devices' linked lists

// Function to find a node with a specific channelId in a linked list
ListNode *findChannelId(LinkedList *lst, int channelId) {
    ListNode *node = lst->first;
    while (node != NULL) {
        if (node->channelId == channelId) {
            return node; // Return node if channelId matches
        }
        node = node->next;
    }
    return NULL; // Return NULL if not found
}

// Function to set a message for a specific node
void setMsg(ListNode *node, const char *msg, int msgLength) {
    memcpy(node->msg, msg, msgLength); // Copy the message to the node's msg
    node->msgLength = msgLength;       // Set the message length
}

// Function to create a new node for a specific channelId
ListNode *createNode(int channelId) {
    ListNode *node;
    node = kmalloc(sizeof(ListNode), GFP_KERNEL); // Allocate memory for the node
    if (node == NULL) return NULL; // Return NULL if allocation fails
    node->msg = kmalloc(sizeof(char) * MSG_MAX_LENGTH, GFP_KERNEL); // Allocate memory for the message
    if (node->msg == NULL) {
        kfree(node); // Free the node if message allocation fails
        return NULL;
    }
    node->channelId = channelId; // Set the channelId
    node->next = NULL;           // Initialize the next pointer to NULL
    return node;
}

// Function to get or create a node for a specific channelId in a linked list
ListNode *getOrCreateNode(LinkedList *lst, int channelId) {
    ListNode *newNode, *prevNode, *node;
    if (lst->size == 0) { // If the list is empty
        newNode = createNode(channelId);
        if (newNode == NULL) return NULL; // Return NULL if node creation fails
        lst->first = newNode;             // Set the first node in the list
        lst->size++;
        return newNode;
    } else {
        prevNode = NULL;
        node = lst->first;
        do {
            if (channelId == node->channelId) {
                return node; // Return existing node if channelId matches
            } else if (channelId < node->channelId) {
                newNode = createNode(channelId);
                if (newNode == NULL) return NULL; // Return NULL if node creation fails
                if (prevNode == NULL) {
                    newNode->next = node;
                    lst->first = newNode; // Insert the new node at the beginning
                } else {
                    newNode->next = node;
                    prevNode->next = newNode; // Insert the new node between prevNode and node
                }
                lst->size++;
                return newNode;
            }
            prevNode = node;
            node = node->next;
        } while (node != NULL);

        newNode = createNode(channelId);
        if (newNode == NULL) return NULL; // Return NULL if node creation fails
        prevNode->next = newNode; // Insert the new node at the end of the list
        lst->size++;
        return newNode;
    }
}

// Function to free a node's memory
void freeNode(ListNode *node) {
    kfree(node->msg); // Free the message memory
    kfree(node);      // Free the node memory
}

// Function to free a linked list's memory
void freeLst(LinkedList *lst) {
    ListNode *prev, *cur;
    if (lst->size != 0) {
        prev = lst->first;
        cur = prev->next;
        while (cur != NULL) {
            freeNode(prev); // Free each node in the list
            prev = cur;
            cur = cur->next;
        }
        freeNode(prev); // Free the last node
    }
    kfree(lst); // Free the linked list structure
}

// Device open function
static int device_open(struct inode *inode,
                       struct file *file) {
    int minor;
    printk(KERN_INFO "Device opened (file pointer: %p)\n", file);

    minor = iminor(inode); // Get the minor number of the device
    if (devices[minor + 1] == NULL) {
        printk(KERN_INFO "Adding minor %d to device array\n", minor);
        devices[minor + 1] = kmalloc(sizeof(LinkedList), GFP_KERNEL); // Allocate memory for the linked list
        if (devices[minor + 1] == NULL) return -1; // Return error if allocation fails
        devices[minor + 1]->size = 0; // Initialize the size of the list to 0
    }
    return SUCCESS;
}

// Device read function
static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset) {
    int status, minor;
    unsigned long channelId;
    LinkedList *lst;
    ListNode *node;
    char *tmpBuffer;
    printk(KERN_INFO "Device read invoked (file pointer: %p, length: %ld)\n", file, length);
    if (file->private_data == NULL) {
        return -EINVAL; // Return error if no channel is set
    }
    channelId = (unsigned long) file->private_data;
    minor = iminor(file->f_inode);
    lst = devices[minor + 1];
    if (lst == NULL) {
        return -EINVAL; // Return error if no linked list is associated with the device
    }
    node = findChannelId(lst, channelId);
    if (node != NULL) {
        if (length < node->msgLength || buffer == NULL) {
            return -ENOSPC; // Return error if buffer is too small or NULL
        }
        if ((tmpBuffer = kmalloc(sizeof(char) * node->msgLength, GFP_KERNEL)) == NULL) {
            return -ENOSPC; // Return error if temporary buffer allocation fails
        }
        memcpy(tmpBuffer, node->msg, node->msgLength); // Copy message to temporary buffer
        if ((status = copy_to_user(buffer, tmpBuffer, node->msgLength)) != 0) {
            kfree(tmpBuffer); // Free temporary buffer before returning error
            return -ENOSPC; // Return error if copy to user space fails
        }
        kfree(tmpBuffer); // Free temporary buffer
        return node->msgLength; // Return the length of the message
    }
    return -EWOULDBLOCK; // Return error if no message is found for the channel
}

// Device ioctl function
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param) {
    if (MSG_SLOT_CHANNEL == ioctl_command_id && ioctl_param != 0) {
        file->private_data = (void *) ioctl_param; // Set channel ID as private data
        printk(KERN_INFO "IOCTL invoked: setting channelId to %ld\n", ioctl_param);
        return SUCCESS;
    } else {
        return -EINVAL; // Return error if invalid command or parameter
    }
}

// Device write function
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset) {
    unsigned long channelId;
    int status, minor;
    char *tmpBuffer;
    ListNode *node;
    printk(KERN_INFO "Device write invoked (file pointer: %p, length: %ld)\n", file, length);
    if (length > BUF_LEN || length <= 0) {
        return -EMSGSIZE; // Return error if message size is invalid
    }
    if (file->private_data == NULL) {
        return -EINVAL; // Return error if no channel is set
    }
    channelId = (unsigned long) file->private_data;
    if (buffer == NULL) {
        return -ENOSPC; // Return error if buffer is NULL
    }
    minor = iminor(file->f_inode);
    if (devices[minor + 1] == NULL) {
        return -EINVAL; // Return error if no linked list is associated with the device
    }
    if ((node = getOrCreateNode(devices[minor + 1], channelId)) == NULL) {
        return -ENOSPC; // Return error if node creation fails
    }
    node->msgLength = length; // Set the length of the message
    if ((tmpBuffer = kmalloc(sizeof(char) * length, GFP_KERNEL)) == NULL) {
        return -ENOSPC; // Return error if temporary buffer allocation fails
    }
    if ((status = copy_from_user(tmpBuffer, buffer, length)) != 0) {
        kfree(tmpBuffer); // Free temporary buffer before returning error
        return -ENOSPC; // Return error if copy from user space fails
    }
    setMsg(node, tmpBuffer, length); // Set the message in the node
    kfree(tmpBuffer); // Free temporary buffer
    return length;
}

// File operations structure
struct file_operations Fops = {
    .owner          = THIS_MODULE,
    .read           = device_read,
    .write          = device_write,
    .open           = device_open,
    .unlocked_ioctl = device_ioctl,
};

// Module initialization function
static int __init simple_init(void) {
    int success;
    printk(KERN_INFO "Registering device with major number %d and name %s\n", MAJOR_NUM, DEVICE_RANGE_NAME);
    success = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    if (success < 0) {
        printk(KERN_ERR "Registration failed for %s with major number %d. Received status: %d\n",
               DEVICE_RANGE_NAME, MAJOR_NUM, success);
        return MAJOR_NUM;
    }
    printk(KERN_INFO "Registration successful.\n");
    return 0;
}

// Module cleanup function
static void __exit simple_cleanup(void) {
    LinkedList **tmp, **limit;
    limit = devices + MAX_DEVICES;
    printk(KERN_INFO "Freeing allocated memory for devices\n");
    for (tmp = devices + 1; tmp < limit; tmp++) {
        if (*tmp != NULL) {
            freeLst(*tmp); // Free memory for each device's linked list
        }
    }
    printk(KERN_INFO "Memory freeing complete\n");
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); // Unregister the character device
    printk(KERN_INFO "Device successfully unregistered\n");
}

module_init(simple_init); // Define module initialization function
module_exit(simple_cleanup); // Define module cleanup function
