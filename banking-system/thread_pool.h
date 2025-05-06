/*
 * thread_pool.h
 *
 *  Created on: Jan 9, 2025
 *      Author: os
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <vector>
#include <iostream>
#include "task_queue.h"

class ThreadPool {
private:
	std::vector<pthread_t> threads; // Vector of worker threads
	TaskQueue& taskQueue;           // Shared task queue
	pthread_mutex_t stopMutex;      // Mutex to synchronize stop condition

	static void* worker(void* arg); // Worker thread function

public:
	ThreadPool(TaskQueue& taskQueue, size_t numThreads);
	~ThreadPool();

	void submitTask(int priority, const std::function<void()>& fn); // Submit a new task
};

#endif /* THREAD_POOL_H_ */
