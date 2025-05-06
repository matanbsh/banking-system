

#ifndef READ_WRITE_LOCK_H_
#define READ_WRITE_LOCK_H_

#include <pthread.h>
#include <iostream>

class ReadWriteLock {
private:
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int activeReaders;
	int activeWriters;
	int waitingWriters;

public:
	ReadWriteLock();
	~ReadWriteLock();

	void acquireReadLock();
	void releaseReadLock();
	void acquireWriteLock();
	void releaseWriteLock();

	// Method to access the underlying mutex for use with condition variables
	pthread_mutex_t* getUnderlyingMutex();
};

#endif /* READ_WRITE_LOCK_H_ */
