
#include "task_queue.h"

TaskQueue::TaskQueue() {
    pthread_cond_init(&cond, nullptr);
}

TaskQueue::~TaskQueue() {
    pthread_cond_destroy(&cond);
}

void TaskQueue::push(const Task& task) {
    pthread_mutex_lock(rwLock.getUnderlyingMutex()); 
    tasks.push(task);
    pthread_cond_signal(&cond); // Notify one waiting thread 
    pthread_mutex_unlock(rwLock.getUnderlyingMutex());
}

Task TaskQueue::pop() {
    pthread_mutex_lock(rwLock.getUnderlyingMutex());

    // Wait for tasks to be added
    while (tasks.empty() && poolRunning ) {
        pthread_cond_wait(&cond, rwLock.getUnderlyingMutex());
    } 
    if (tasks.empty() && !poolRunning) {
        pthread_mutex_unlock(rwLock.getUnderlyingMutex());
        return Task(); // Indicate shutdown or empty queue
    }

    Task task = tasks.top();
    tasks.pop();

    pthread_mutex_unlock(rwLock.getUnderlyingMutex());
    return task;
}

bool TaskQueue::empty() {
    bool isEmpty = tasks.empty();
    return isEmpty;
}

void TaskQueue::pollShutDown() {
    poolRunning = false;
    pthread_cond_broadcast(&cond);
}