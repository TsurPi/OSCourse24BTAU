#include <threads.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// Structure for each item in the queue
typedef struct QueueItem {
    void* data;
    struct QueueItem* next;
} QueueItem;

// Structure for each thread waiting to dequeue
typedef struct WaitingThread {
    cnd_t cond;
    struct WaitingThread* next;
} WaitingThread;

// Structure for the queue itself
typedef struct {
    QueueItem* head;
    QueueItem* tail;
    size_t count;           // Current number of items in the queue
    size_t visitedCount;    // Number of items that have passed through the queue
    mtx_t mutex;            // Mutex for synchronization
    WaitingThread* waitingHead; // Head of the waiting thread queue
    WaitingThread* waitingTail; // Tail of the waiting thread queue
    size_t waitingCount;    // Number of threads waiting for an item
} Queue;

Queue queue;

// Initialize the queue
void initQueue(void) {
    queue.head = NULL;
    queue.tail = NULL;
    queue.count = 0;
    queue.visitedCount = 0;
    queue.waitingHead = NULL;
    queue.waitingTail = NULL;
    queue.waitingCount = 0;
    mtx_init(&queue.mutex, mtx_plain);
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

    // Free all waiting threads
    while (queue.waitingHead != NULL) {
        WaitingThread* temp = queue.waitingHead;
        queue.waitingHead = queue.waitingHead->next;
        cnd_destroy(&temp->cond);
        free(temp);
    }

    mtx_unlock(&queue.mutex);
    mtx_destroy(&queue.mutex);
}

// Enqueue an item into the queue
void enqueue(void* item) {
    QueueItem* newItem = (QueueItem*)malloc(sizeof(QueueItem));
    newItem->data = item;
    newItem->next = NULL;

    mtx_lock(&queue.mutex);

    if (queue.tail == NULL) {
        queue.head = newItem;
    } else {
        queue.tail->next = newItem;
    }
    queue.tail = newItem;
    queue.count++;

    // Wake up the first waiting thread, if any
    if (queue.waitingHead != NULL) {
        WaitingThread* waitingThread = queue.waitingHead;
        queue.waitingHead = queue.waitingHead->next;
        if (queue.waitingHead == NULL) {
            queue.waitingTail = NULL;
        }
        queue.waitingCount--;
        cnd_signal(&waitingThread->cond);
    }

    mtx_unlock(&queue.mutex);
}

// Dequeue an item from the queue (blocks if empty)
void* dequeue(void) {
    mtx_lock(&queue.mutex);

    // If the queue is empty, add the current thread to the waiting queue
    while (queue.count == 0) {
        WaitingThread myWaitingThread;
        cnd_init(&myWaitingThread.cond);
        myWaitingThread.next = NULL;

        if (queue.waitingTail == NULL) {
            queue.waitingHead = &myWaitingThread;
        } else {
            queue.waitingTail->next = &myWaitingThread;
        }
        queue.waitingTail = &myWaitingThread;
        queue.waitingCount++;

        // Wait for a signal
        cnd_wait(&myWaitingThread.cond, &queue.mutex);

        // After being signaled, remove the thread from the queue
        if (queue.waitingHead == &myWaitingThread) {
            queue.waitingHead = myWaitingThread.next;
        }
        if (queue.waitingTail == &myWaitingThread) {
            queue.waitingTail = NULL;
        }
        cnd_destroy(&myWaitingThread.cond);
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

// Get the total number of items that have passed through the queue
size_t visited(void) {
    return queue.visitedCount;
}

// Get the current number of threads waiting for an item
size_t waiting(void) {
    return queue.waitingCount;
}
