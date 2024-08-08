#include <threads.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// Structure for each item in the queue
typedef struct QueueItem {
    void* data;
    struct QueueItem* next;
} QueueItem;

// Structure for the queue itself
typedef struct {
    QueueItem* head;
    QueueItem* tail;
    size_t count;           // Current number of items in the queue
    size_t waitingCount;    // Number of threads waiting for an item
    size_t visitedCount;    // Number of items that have passed through the queue
    mtx_t mutex;            // Mutex for synchronization
    cnd_t cond;             // Condition variable for signaling
} Queue;

Queue queue;

// Initialize the queue
void initQueue(void) {
    queue.head = NULL;
    queue.tail = NULL;
    queue.count = 0;
    queue.waitingCount = 0;
    queue.visitedCount = 0;
    mtx_init(&queue.mutex, mtx_plain);
    cnd_init(&queue.cond);
}

// Destroy the queue and free resources
void destroyQueue(void) {
    mtx_lock(&queue.mutex);

    // Free all remaining items in the queue
    while (queue.head != NULL) {
        QueueItem* temp = queue.head;
        queue.head = queue.head->next;
        free(temp);
    }
    
    mtx_unlock(&queue.mutex);

    // Destroy the mutex and condition variable
    mtx_destroy(&queue.mutex);
    cnd_destroy(&queue.cond);
}

// Enqueue an item into the queue
void enqueue(void* item) {
    QueueItem* newItem = (QueueItem*)malloc(sizeof(QueueItem));
    newItem->data = item;
    newItem->next = NULL;

    mtx_lock(&queue.mutex);

    // Add the new item to the queue
    if (queue.tail == NULL) {
        queue.head = newItem;
    } else {
        queue.tail->next = newItem;
    }
    queue.tail = newItem;
    queue.count++;

    // Signal one waiting thread, if any
    if (queue.waitingCount > 0) {
        cnd_signal(&queue.cond);
    }

    mtx_unlock(&queue.mutex);
}

// Dequeue an item from the queue (blocks if empty)
void* dequeue(void) {
    mtx_lock(&queue.mutex);

    // Wait until there is an item to dequeue
    while (queue.count == 0) {
        printf("Queue is empty, thread is waiting...\n");
        queue.waitingCount++;
        cnd_wait(&queue.cond, &queue.mutex);
        queue.waitingCount--;
        printf("Thread woke up, checking queue...\n");
    }

    // Remove the item from the front of the queue
    QueueItem* item = queue.head;
    queue.head = item->next;
    if (queue.head == NULL) {
        queue.tail = NULL;
    }
    queue.count--;
    queue.visitedCount++;

    void* data = item->data;
    free(item);

    printf("Dequeued item: %d\n", *((int*)data)); // Assuming data is of type int

    mtx_unlock(&queue.mutex);

    return data;
}


// Try to dequeue an item from the queue without blocking
bool tryDequeue(void** item) {
    mtx_lock(&queue.mutex);

    if (queue.count == 0) {
        mtx_unlock(&queue.mutex);
        return false;
    }

    // Remove the item from the front of the queue
    QueueItem* dequeuedItem = queue.head;
    queue.head = dequeuedItem->next;
    if (queue.head == NULL) {
        queue.tail = NULL;
    }
    queue.count--;
    queue.visitedCount++;

    *item = dequeuedItem->data;
    free(dequeuedItem);

    mtx_unlock(&queue.mutex);
    return true;
}

// Get the current number of items in the queue
size_t size(void) {
    return queue.count;
}

// Get the current number of threads waiting for an item
size_t waiting(void) {
    return queue.waitingCount;
}

// Get the total number of items that have passed through the queue
size_t visited(void) {
    return queue.visitedCount;
}
