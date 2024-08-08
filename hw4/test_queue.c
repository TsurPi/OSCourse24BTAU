#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "queue.c"  // Include your queue implementation here

#define NUM_THREADS 10
#define NUM_ITEMS 1000

int producer_thread(void* arg) {
    for (int i = 0; i < NUM_ITEMS; i++) {
        int* item = malloc(sizeof(int));
        *item = i;
        enqueue(item);
    }
    return 0;
}

int consumer_thread(void* arg) {
    for (int i = 0; i < NUM_ITEMS; i++) {
        int* item = (int*)dequeue();
        printf("Consumed: %d\n", *item);
        free(item);
    }
    return 0;
}

int try_consumer_thread(void* arg) {
    for (int i = 0; i < NUM_ITEMS; i++) {
        int* item = NULL;
        while (!tryDequeue((void**)&item)) {
            thrd_yield(); // Yield the thread if dequeue fails
        }
        printf("Try Consumed: %d\n", *item);
        free(item);
    }
    return 0;
}

int mixed_thread(void* arg) {
    for (int i = 0; i < NUM_ITEMS / 2; i++) {
        int* item = malloc(sizeof(int));
        *item = i;
        enqueue(item);

        int* dequeuedItem = (int*)dequeue();
        printf("Mixed Dequeued: %d\n", *dequeuedItem);
        free(dequeuedItem);
    }
    return 0;
}

void basic_test() {
    initQueue();

    printf("Starting basic test...\n");

    // Single producer and consumer
    thrd_t producer, consumer;
    thrd_create(&producer, producer_thread, NULL);
    thrd_create(&consumer, consumer_thread, NULL);

    thrd_join(producer, NULL);
    thrd_join(consumer, NULL);

    destroyQueue();
    printf("Basic test completed.\n\n");
}

void concurrent_test() {
    initQueue();

    printf("Starting concurrent test...\n");

    // Multiple producers and consumers
    thrd_t producers[NUM_THREADS];
    thrd_t consumers[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&producers[i], producer_thread, NULL);
        thrd_create(&consumers[i], consumer_thread, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(producers[i], NULL);
        thrd_join(consumers[i], NULL);
    }

    destroyQueue();
    printf("Concurrent test completed.\n\n");
}

void try_dequeue_test() {
    initQueue();

    printf("Starting tryDequeue test...\n");

    // Multiple producers and try-consumers
    thrd_t producers[NUM_THREADS];
    thrd_t consumers[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&producers[i], producer_thread, NULL);
        thrd_create(&consumers[i], try_consumer_thread, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(producers[i], NULL);
        thrd_join(consumers[i], NULL);
    }

    destroyQueue();
    printf("tryDequeue test completed.\n\n");
}

void mixed_operations_test() {
    initQueue();

    printf("Starting mixed operations test...\n");

    // Threads that both enqueue and dequeue
    thrd_t mixedThreads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&mixedThreads[i], mixed_thread, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(mixedThreads[i], NULL);
    }

    destroyQueue();
    printf("Mixed operations test completed.\n\n");
}

void edge_case_test() {
    initQueue();

    printf("Starting edge case test...\n");

    // Test dequeue from an empty queue
    thrd_t consumer;
    thrd_create(&consumer, consumer_thread, NULL);

    thrd_sleep(&(struct timespec){.tv_sec = 2, .tv_nsec = 0}, NULL); // Increased sleep time to 2 seconds
    int* item = malloc(sizeof(int));
    *item = 42; // Initialize the value to ensure it's valid
    enqueue(item);

    thrd_join(consumer, NULL);

    // Check sizes and stats
    printf("Final size of queue: %zu\n", size());
    printf("Final number of waiting threads: %zu\n", waiting());
    printf("Total items visited: %zu\n", visited());

    destroyQueue();
    printf("Edge case test completed.\n\n");
}


int main() {
    basic_test();
    concurrent_test();
    try_dequeue_test();
    mixed_operations_test();
    edge_case_test();

    return 0;
}
