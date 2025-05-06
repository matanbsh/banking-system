/*
 * task_queue.h
 *
 *  Created on: Jan 9, 2025
 *      Author: os
 */

#ifndef TASK_QUEUE_H_
#define TASK_QUEUE_H_

#include <iostream>
#include <queue>
#include <functional>
#include <pthread.h>
#include "read_write_lock.h"

// Define a Task structure
struct Task {
    int priority;                  // Task priority (lower = higher priority)
    std::function<void()> fn;      // Task function to execute
    bool isShutdownTask = false;   // Flag indicating if this is a shutdown task (default is false)

    // Default constructor for shutdown or placeholder tasks
    Task() : priority(0), fn(nullptr), isShutdownTask(true) {}

    // Constructor for regular tasks
    Task(int priority, std::function<void()> fn)
        : priority(priority), fn(fn), isShutdownTask(false) {}

    // Comparator for priority queue
    bool operator<(const Task& other) const {
        return priority > other.priority; // Higher priority = smaller key
    }
};

// TaskQueue class
class TaskQueue {
private:
    std::priority_queue<Task> tasks; // Priority queue for tasks
    ReadWriteLock rwLock;           // Reader-writer lock for thread safety
    pthread_cond_t cond;            // Condition variable to notify waiting threads
    bool poolRunning =true; // Flag to indicate 

public:
    TaskQueue();
    ~TaskQueue();

    void push(const Task& task); // Add a task to the queue
    Task pop();                  // Fetch the highest-priority task
    bool empty();                // Check if the queue is empty

    // Expose the condition variable for signaling in the thread pool
    pthread_cond_t* getCond() { return &cond; }
    void pollShutDown();
};

#endif /* TASK_QUEUE_H_ */
