/*
 * thread_pool.cpp
 *
 *  Created on: Jan 9, 2025
 *      Author: os
 */
#include "thread_pool.h"

ThreadPool::ThreadPool(TaskQueue& taskQueue, size_t numThreads)
: taskQueue(taskQueue) {
	pthread_mutex_init(&stopMutex, nullptr);

	for (size_t i = 0; i < numThreads; ++i) {
		pthread_t thread;
		pthread_create(&thread, nullptr, worker, this);
		threads.push_back(thread);
	}
}

ThreadPool::~ThreadPool() {
	// Signal all threads to stop
	taskQueue.pollShutDown();

	// Broadcast to all waiting threads to wake up
	pthread_cond_broadcast(taskQueue.getCond());

	// Join all threads
	for (pthread_t thread : threads) {
		pthread_join(thread, nullptr);
	}

	pthread_mutex_destroy(&stopMutex);
}

void* ThreadPool::worker(void* arg) {
	ThreadPool* pool = static_cast<ThreadPool*>(arg);

	while (true) {
		// Fetch and execute a task
		
		Task task = pool->taskQueue.pop();

		if (task.isShutdownTask) {
            // If pop returns nullopt, the queue is shutting down
            break;
        }

		task.fn(); // Execute the task
	}
	return nullptr;
}

void ThreadPool::submitTask(int priority, const std::function<void()>& fn) {
	Task task = {priority, fn};
	taskQueue.push(task);
}