#ifndef BANKING_SYSTEM_H
#define BANKING_SYSTEM_H

#include <cmath>
#include <queue>
#include <atomic>
#include <string>
#include <map>
#include <vector>
#include <pthread.h>
#include <fstream>
#include <utility>
#include "read_write_lock.h"
#include "task_queue.h"
#include "thread_pool.h"

#define MAX_STATES 120

class ATM;

// Account Class
class Account {
private:
	int id;
	std::string password;
	int balance;
	ReadWriteLock rwLock;
	ReadWriteLock logLock;

public:
	Account();
	Account(int id, const std::string& password, int balance);
	Account(const Account& other);



    bool verifyPassword(const std::string& inputPassword) const;
    void deposit(int amount);
    void withdraw(int amount);
    void setBalance(int amount);
    int getBalance() const;
    int getId();
    std::string getPassword() const;

    // Lock management methods for both readers and writers
    void lockWrite();
    void unlockWrite();
    void lockRead();
    void unlockRead();

    ReadWriteLock& getLogLock(); // Access the log lock

};

struct BankState {
    std::map<int, Account> accounts;
};

class BankHistory {
private:
    std::vector<BankState> stateHistory;
    size_t currentIndex;
public:
    BankHistory(size_t maxStates);
    void saveState(const BankState& state);
    BankState getState(int R) const;
};

// Bank Class
class Bank {
private:
    Account bankAccount;
    std::atomic<bool> running;
    BankHistory history;
    TaskQueue vipTaskQueue;           // VIP task queue
    ThreadPool* vipThreadPool;        // VIP thread pool
    size_t totalSavedStates;

    std::vector<ATM*> atms;                // List of ATM pointers
	std::map<int, Account*> accounts;
	std::vector<bool> atmStates;              // Tracks ATM open/closed states

    pthread_t commissionThread;
    pthread_t statusThread;

    std::queue<std::pair<int, int>> restoreRequests;
    std::queue<std::pair<int, int>> atmClosureRequests;

    ReadWriteLock atmClosureLock;
    ReadWriteLock restoreLock;
	ReadWriteLock atmLock; //Lock for the atmStates vector and atms vector
	ReadWriteLock rwLock;
    ReadWriteLock logLock; // Protects the shared log file


    static void* chargeCommission(void* arg);
    static void* printStatus(void* arg);
    BankState getCurrentState();
    void applyState(const BankState& state);

public:
    Bank(size_t numVIPThreads);
    Bank();
    ~Bank();


    bool addRestoreRequest(int R, int atmId);
    void restoreRequestsHandler();
    bool createAccount(int id, const std::string& password, int balance, int atmID, bool isPersist);
    bool deleteAccount(int id, const std::string& password,int atmID, bool isPersist);

    void registerATM(ATM* atm);
    bool requestATMClosure(int atmID, int sourceATMID, bool isPersist);
    void processATMClosures();

    bool deposit(int accountId, int amount, const std::string& password, int atmID, bool isPersist);
    bool withdraw(int accountId, int amount, const std::string& password, int atmID, bool isPersist);
    bool getBalance(int accountId, const std::string& password, int atmID, bool isPersist);
    bool transfer(int srcId, const std::string& password, int destId, int amount, int atmID, bool isPersist);
	void logTransaction(const std::string& message); // Logs transaction to a shared log file
    void submitVIPTask(int priority, const std::function<void()>& task);
    void stop();
    void saveState();
    void restore(int R, int atmID);

};

// ATM Class
class ATM {
private:
	int id;
	bool stop; // Flag to indicate if the ATM should stop
	pthread_mutex_t stopMutex;  // Mutex for synchronizing access to the `stop` flag
	std::string inputFile; //Path to the input file containing the ATM's operations.
	Bank* bank; //Pointer to the shared Bank object, allowing the ATM to perform transactions.
	pthread_t thread; // Thread for the ATM
    ReadWriteLock rwLock;

	static void* run(void* arg);
	void processCommand(const std::string& command); // Processes a single command

public:
	ATM(int id, const std::string& inputFile, Bank* bank);
	 ~ATM();
	void start();
	void join();
	void closeATM();
    ReadWriteLock& getATMLock(); 

};

#endif
