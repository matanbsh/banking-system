/*
 * read_write_lock.cpp
 *
 *  Created on: Jan 8, 2025
 *      Author: os
 */
#include "read_write_lock.h"

ReadWriteLock::ReadWriteLock() : activeReaders(0), activeWriters(0), waitingWriters(0) {
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&cond, nullptr);
}

ReadWriteLock::~ReadWriteLock() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

void ReadWriteLock::acquireReadLock() {
    pthread_mutex_lock(&mutex);

    while (activeWriters > 0) {  // Only block if there are active writers
        pthread_cond_wait(&cond, &mutex);
    }

    activeReaders++;

    pthread_mutex_unlock(&mutex);
}

void ReadWriteLock::releaseReadLock() {
    pthread_mutex_lock(&mutex);

    activeReaders--;

    if (activeReaders == 0 && waitingWriters > 0) {
        pthread_cond_signal(&cond);
    }

    pthread_mutex_unlock(&mutex);
}

void ReadWriteLock::acquireWriteLock() {
    pthread_mutex_lock(&mutex);

    while (activeReaders > 0 || activeWriters > 0) {
        waitingWriters++;
        pthread_cond_wait(&cond, &mutex);
        waitingWriters--;
    }

    activeWriters++;

    pthread_mutex_unlock(&mutex);
}

void ReadWriteLock::releaseWriteLock() {
    pthread_mutex_lock(&mutex);

    if (activeWriters > 0) {
        activeWriters--;
    } else {
        std::cerr << "Error: releaseWriteLock called without an active writer!\n";
    }

    pthread_cond_broadcast(&cond);

    pthread_mutex_unlock(&mutex);
}

// Expose the underlying mutex for use with pthread_cond_wait
pthread_mutex_t* ReadWriteLock::getUnderlyingMutex() {
    return &mutex;
}
