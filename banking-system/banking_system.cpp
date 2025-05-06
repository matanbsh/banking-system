#include "banking_system.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include <string>


// Account Class Implementation
Account::Account():id(0), password(""), balance(0) {}

Account::Account(int id, const std::string& password, int balance)
    : id(id), password(password), balance(balance) {}

Account::Account(const Account& other)
	: id(other.id),  password(other.password), balance(other.balance) {}

bool Account::verifyPassword(const std::string& inputPassword) const {
	return password == inputPassword;
}

void Account::deposit(int amount) {
	balance += amount;
}

void Account::withdraw(int amount) {
	balance -= amount;
}

void Account::setBalance(int amount){
	balance = amount;
}

int Account::getBalance() const {
	return balance;
}

int Account::getId() {
	return id;
}

std::string Account::getPassword() const {
	return password;
}

void Account::lockWrite() {
	rwLock.acquireWriteLock();
}

void Account::unlockWrite() {
	rwLock.releaseWriteLock();
}

void Account::lockRead() {
	rwLock.acquireReadLock();
}

void Account::unlockRead() {
	rwLock.releaseReadLock();
}

ReadWriteLock& Account::getLogLock() {
    return logLock;
}

BankHistory::BankHistory(size_t maxStates)
    : stateHistory(maxStates), currentIndex(0) {
}

void BankHistory::saveState(const BankState& state) {
	currentIndex = (currentIndex + 1) % MAX_STATES;
	stateHistory[currentIndex] = state;
}

BankState BankHistory::getState(int R) const {
	size_t restoreIndex = (currentIndex + MAX_STATES + 1 - R) % MAX_STATES;
	return stateHistory[restoreIndex];
}

Bank::Bank(size_t numVIPThreads) : bankAccount(0, "bank_password", 0), running(true), history(120), vipTaskQueue(),
 vipThreadPool(new ThreadPool(vipTaskQueue, numVIPThreads)), totalSavedStates(0) {
	pthread_create(&statusThread, nullptr, Bank::printStatus, this);
	pthread_create(&commissionThread, nullptr, Bank::chargeCommission, this);
}

Bank::Bank() : bankAccount(0, "bank_password", 0), running(true), history(120),
  totalSavedStates(0) {
	pthread_create(&statusThread, nullptr, Bank::printStatus, this);
	pthread_create(&commissionThread, nullptr, Bank::chargeCommission, this);
}

Bank::~Bank() {

    stop();
    pthread_join(statusThread, nullptr);
    pthread_join(commissionThread, nullptr);
    for (auto& pair : accounts) {
        delete pair.second;
    }
	 

}

void Bank::submitVIPTask(int priority, const std::function<void()>& task) {
    vipThreadPool->submitTask(priority, task);
}

void* Bank::chargeCommission(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);  // Cast the argument back to a Bank pointer

    // Seed the random number generator (once per thread)
    srand(time(nullptr));

    while (bank->running) {
        // Wait for 3 seconds before charging commission
        sleep(3);

		// Generate a random percentage between 1% and 5%
		int percentage = (rand() % 5) + 1;

        // Loop through all accounts and charge a random commission
        for (auto& accountPair : bank->accounts) {
            Account* account = accountPair.second;

            // Calculate the commission
            account->lockWrite();
			if(account!=nullptr){
				int commission = std::round(account->getBalance() * percentage / 100.0);
            	
				Account* bankA = &bank->bankAccount;
				// Deduct the commission from the account balance
            	account->withdraw(commission);
				bankA->setBalance(bankA->getBalance() + commission);

				bank->logTransaction("Bank: commissions of "+std::to_string(percentage)+" % were charged, bank gained "+std::to_string(commission)+" from account " + std::to_string(account->getId()) + "\n");
			}
            account->unlockWrite();
        }
    }

    return nullptr;
}

void* Bank::printStatus(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);  // Cast the 'arg' to Bank*

    while (bank->running) {
        usleep(500000);  // Sleep for 0.5 seconds (500,000 microseconds)

		bank->rwLock.acquireReadLock();

		// Save the current state before printing
        bank->saveState();

        // Clear the screen and move the cursor to the top-left corner
        printf("\033[2J\033[1;1H");

        // Print the status of all accounts
        std::cout << "Current Bank Status\n";
        for (auto& accountPair : bank->accounts) {
            Account* account = accountPair.second;
            std::cout << "Account " << account->getId()
                      << ": Balance - " << account->getBalance()
                      << " $, Account Password - " << account->getPassword() << "\n";
        }

		bank->rwLock.releaseReadLock();
	  	bank->processATMClosures();
		bank->restoreRequestsHandler();
   
}

    return nullptr;
}

bool Bank::requestATMClosure(int atmID, int sourceATMID, bool isPersist) {
    atmClosureLock.acquireWriteLock();
    atmLock.acquireReadLock();



    if (atmID < 0 || atmID >= static_cast<int>(atms.size()) || atms[atmID] == nullptr) {
		if(!isPersist){
					logTransaction("Error " + std::to_string(sourceATMID) +
					": Your transaction failed – ATM ID " + std::to_string(atmID) + " does not exist\n");
		}
					atmClosureLock.releaseWriteLock();
					atmLock.releaseReadLock();
					   return false;
    } else if (!atmStates[atmID]) {
		if(!isPersist){
					logTransaction("Error " + std::to_string(sourceATMID) +
					": Your close operation failed – ATM ID " + std::to_string(atmID) + " is already in a closed state\n");
					}
					atmClosureLock.releaseWriteLock();
					atmLock.releaseReadLock();
					   return true;
    } else {
            atmClosureRequests.push({atmID, sourceATMID});
		atmClosureLock.releaseWriteLock();
					atmLock.releaseReadLock();
					   return true;
    }
    
}


void Bank::processATMClosures() {
	atmClosureLock.acquireWriteLock();
	atmLock.acquireWriteLock();


    while (!atmClosureRequests.empty()) {

    	std::pair<int, int> closureRequest = atmClosureRequests.front();
        atmClosureRequests.pop();
        int atmID = closureRequest.first;
        int sourceATMID = closureRequest.second;

if (atmStates[atmID]){

		        ReadWriteLock& Lock = atms[atmID]->getATMLock();
            Lock.acquireWriteLock(); // Lock the ATM-specific lock

        atmStates[atmID] = false; // Mark as closed
        atms[atmID]->closeATM(); // Signal the ATM to close
        atms[atmID]->join();      // Ensure the thread is joined

        delete atms[atmID];       // Free memory for the ATM object
        atms[atmID] = nullptr;    // Set the pointer to nullptr

        logTransaction("Bank: ATM " + std::to_string(atmID) + " successfully closed\n");
Lock.releaseWriteLock(); 
            
 } else {
	logTransaction("Error " + std::to_string(sourceATMID) +
					": Your transaction failed – ATM ID " + std::to_string(atmID) + " does not exist\n");

 }


    }

    atmClosureLock.releaseWriteLock();
    atmLock.releaseWriteLock();
}



void Bank::registerATM(ATM* atm) {
	atmLock.acquireWriteLock();
	atms.push_back(atm);
	atmStates.push_back(true); // Mark as open
	atmLock.releaseWriteLock();
}


bool Bank::addRestoreRequest(int R, int atmId) {
	restoreLock.acquireWriteLock();

	if (R < 1 || R > static_cast<int>(totalSavedStates)){
		restoreLock.releaseWriteLock();
		return false;
	}
	else {
		restoreRequests.push(std::make_pair(R, atmId));
		restoreLock.releaseWriteLock();
		return true;
	}
}


void Bank::restoreRequestsHandler() {
	restoreLock.acquireWriteLock();
	while (!restoreRequests.empty()) {
		std::pair<int, int> front = restoreRequests.front();
		int R = front.first;
		int atmId = front.second;
		restoreRequests.pop();               // Remove it from the queue
		restore(R, atmId);
	}
	restoreLock.releaseWriteLock();

}

BankState Bank::getCurrentState() {
    BankState currentState;

    // Deep copy: copy the actual Account objects
    for (const auto& accountPair : accounts) {
        currentState.accounts[accountPair.first] = *accountPair.second;  // Copy Account by value
    }

    return currentState;
}

void Bank::applyState(const BankState& state) {
	rwLock.acquireWriteLock();
    // Step 1: Update or restore accounts in the current state
	 for (const auto& pair : state.accounts) {
	        const int& id = pair.first;
	        const Account& restoredAccount = pair.second;

	        if (accounts.find(id) != accounts.end()) {
	            // Update existing account
	            accounts[id]->setBalance(restoredAccount.getBalance());
	        } else {
	            // Add account from restored state
	            accounts[id] = new Account(restoredAccount);
	        }
	    }

    // Step 2: Remove accounts not present in the restored state
    for (auto it = accounts.begin(); it != accounts.end();) {
        if (state.accounts.find(it->first) == state.accounts.end()) {
			delete it->second;
            it = accounts.erase(it); // Remove account
        } else {
            ++it;
        }
    }
	rwLock.releaseWriteLock();

}

bool Bank::createAccount(int id, const std::string& password, int balance, int atmID, bool isPersist) {
	// Acquire the write lock on the accounts map
	rwLock.acquireWriteLock();

	// Check if the account already exists
	if (accounts.find(id) != accounts.end()) {

		if(!isPersist){
			// Log the error message
			logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account with the same id exists\n");
		}
		rwLock.releaseWriteLock();
		return false; // Account creation failed
	}

	// Create a new account and insert it into the map
	Account* newAccount = new Account(id, password, balance);
	accounts[id] = newAccount;

	logTransaction(
				std::to_string(atmID) + ": New account id is " + std::to_string(id)
						+ " with password " + password + " and initial balance "
						+ std::to_string(balance) + "\n");
	// Release the lock on the accounts map
	rwLock.releaseWriteLock();
	return true;
}

bool Bank::deleteAccount(int id, const std::string& password,int atmID, bool isPersist) {

	Account* account = nullptr;

	//Read lock to check existence of the account
	rwLock.acquireReadLock();
	auto it = accounts.find(id);
	if (it == accounts.end()) {
		if(!isPersist){
		logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(id)+" does not exist\n");
		}
		rwLock.releaseReadLock();
		return false; // Account does not exist
	}
	account = it->second;

	//Lock the specific account to ensure no operations are ongoing
	account->lockWrite();
	rwLock.releaseReadLock();

	if (!account->verifyPassword(password)) {
		if(!isPersist){
		logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – password for account id "+std::to_string(id)+" is incorrect\n");
		}
		account->unlockWrite();
		return false; // Incorrect password
	}

	int balance = account->getBalance();

	//Safely remove and delete the account
	rwLock.acquireWriteLock();
	accounts.erase(it);
	rwLock.releaseWriteLock();

	//Release account lock and delete the account
	logTransaction(std::to_string(atmID)+": Account "+std::to_string(id)+" is now closed. Balance was "+std::to_string(balance)+"\n");
	account->unlockWrite();
	delete account;

	return true;
}

bool Bank::deposit(int accountId, int amount, const std::string& password, int atmID, bool isPersist) {
	Account* account = nullptr;

	//Acquire a read lock to locate the account
	rwLock.acquireReadLock();
	auto it = accounts.find(accountId);
	if (it == accounts.end()) {

		if(!isPersist){
		// Log the error: incorrect password
		logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(accountId)+" does not exist\n");
		}
		rwLock.releaseReadLock();
		return false;
	}
	account = it->second;

	account->lockWrite();
	rwLock.releaseReadLock();

	//Verify the password
	if (!account->verifyPassword(password)) {

		if(!isPersist){
		// Log the error: incorrect password
		logTransaction("Error: Deposit failed for account ID " + std::to_string(accountId) +
				" - incorrect password\n");
		}
		account->unlockWrite();
		return false;
	}
	//Perform the deposit
	account->deposit(amount);

	// Log the successful deposit
	logTransaction( std::to_string(atmID) + ": Account "
			+ std::to_string(accountId) + " new balance is "
			+ std::to_string(account->getBalance()) +" after "
			+ std::to_string(amount) + " $ was deposited\n");


	//Unlock the account
	account->unlockWrite();
	return true;
}

bool Bank::withdraw(int accountId, int amount, const std::string& password, int atmID, bool isPersist) {
	Account* account = nullptr;

	// Step 1: Acquire a read lock to locate the account
	rwLock.acquireReadLock();
	auto it = accounts.find(accountId);
	if (it == accounts.end()) {
		if(!isPersist){
		// Log the error: account does not exist
		logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(accountId)+" does not exist\n");
		}
		rwLock.releaseReadLock();

		return false;
	}
	account = it->second;

	account->lockWrite();
	rwLock.releaseReadLock();

	//Verify the password
	if (!account->verifyPassword(password)) {

		if(!isPersist){
		// Log the error: incorrect password
		logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – password for account id "+std::to_string(accountId)+" is incorrect\n");
		account->unlockWrite();
		}
		return false;
	}
	//Check if the account has sufficient balance
	if (account->getBalance() < amount) {
		if(!isPersist){
		// Log the error: insufficient balance
		logTransaction(
				"Error " + std::to_string(atmID)
						+ ": Your transaction failed – account id "
						+ std::to_string(accountId) + " balance is lower than "
						+ std::to_string(amount) + "\n");
		}
		account->unlockWrite();

		return false;
	}

	//Perform the withdrawal
	account->withdraw(amount);

	// Log the successful withdrawal

	logTransaction( std::to_string(atmID) + ": Account "
				+ std::to_string(accountId) + " new balance is "
				+ std::to_string(account->getBalance()) +" after "
				+ std::to_string(amount) + " $ was withdrawn\n");

	//Unlock the account
	account->unlockWrite();

	return true;
}

bool Bank::getBalance(int accountId, const std::string& password, int atmID, bool isPersist) {
    Account* account = nullptr;

    //Acquire a read lock to locate the account
    rwLock.acquireReadLock();
    auto it = accounts.find(accountId);
    if (it == accounts.end()) {

        // Log the error: account does not exist
        logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(accountId)+" does not exist\n");
        rwLock.releaseReadLock();
        return false;
    }

    account = it->second;
    account->lockRead();
    rwLock.releaseReadLock();

    //Verify the password
    if (!account->verifyPassword(password)) {
    	if(!isPersist){
        // Log the error: incorrect password
		logTransaction(
				"Error " + std::to_string(atmID)
						+ ": Your transaction failed – password for account id "
						+ std::to_string(accountId) + " is incorrect\n");
    	}
		  account->unlockRead();

        return false;
    }

    //Retrieve the balance
    int balance = account->getBalance();
	account->getLogLock().acquireWriteLock();
	// Log the successful balance check
	logTransaction(
			std::to_string(atmID) + ": Account " + std::to_string(accountId)
					+ " balance is " + std::to_string(balance) + "\n");
	account->getLogLock().releaseWriteLock();

    //Unlock the account
    account->unlockRead();

    return true; // Return the balance
}

bool Bank::transfer(int srcId, const std::string& password, int destId, int amount, int atmID, bool isPersist) {
    Account* srcAccount = nullptr;
    Account* destAccount = nullptr;

    //Locate both source and destination accounts
    rwLock.acquireReadLock();

    auto srcIt = accounts.find(srcId);
    auto destIt = accounts.find(destId);

    if (srcIt == accounts.end() || destIt == accounts.end()) {

        // Log the error: one or both accounts do not exist
        if (srcIt == accounts.end()) {
        	if(!isPersist){
        	logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(srcId)+" does not exist\n");
        	}
        	rwLock.releaseReadLock();
        	return false;
        }
        if (destIt == accounts.end()) {
        	if(!isPersist){
        	logTransaction("Error "+std::to_string(atmID)+": Your transaction failed – account id "+std::to_string(destId)+" does not exist\n");
        	}
        	rwLock.releaseReadLock();
        	return false;
        }
    }
    srcAccount = srcIt->second;
    destAccount = destIt->second;

    //Lock accounts in consistent order to avoid deadlocks
	if (srcId < destId) {
		srcAccount->lockWrite();
		destAccount->lockWrite();
	} else {
		destAccount->lockWrite();
		srcAccount->lockWrite();
	}

	rwLock.releaseReadLock();

	//Verify the source account's password
	if (!srcAccount->verifyPassword(password)) {
		if(!isPersist){
		logTransaction(
				"Error " + std::to_string(atmID)
						+ ": Your transaction failed – password for account id "
						+ std::to_string(srcId) + " is incorrect\n");
		}
		srcAccount->unlockWrite();
		destAccount->unlockWrite();
		return false;
	}

    //Check for sufficient balance in the source account
    if (srcAccount->getBalance() < amount) {
    	if(!isPersist){
        // Log the error: insufficient balance
        logTransaction(
        				"Error " + std::to_string(atmID)
        						+ ": Your transaction failed – account id "
        						+ std::to_string(srcId) + " balance is lower than "
        						+ std::to_string(amount) + "\n");
    	}
        srcAccount->unlockWrite();
        destAccount->unlockWrite();
        return false;
    }

    //Perform the transfer
    srcAccount->withdraw(amount);
    destAccount->deposit(amount);

    // Log the successful transfer
    logTransaction(std::to_string(atmID) + ": Transfer "
			+ std::to_string(amount) + " from account "
			+ std::to_string(srcId) + " to account "
			+ std::to_string(destId) + " new account balance is "
			+ std::to_string(srcAccount->getBalance())
	+ " new target account balance is "
	+ std::to_string(destAccount->getBalance()) + "\n");

    //Unlock both accounts
    srcAccount->unlockWrite();
    destAccount->unlockWrite();

    return true;
	
}

void Bank::stop() {
    // Set the running flag to false to signal threads to stop
    running = false;
}

void Bank::saveState() {
	history.saveState(getCurrentState());
	if (totalSavedStates < MAX_STATES) {
        totalSavedStates++;
    }
}

void Bank::restore(int R, int atmID) {
	applyState(history.getState(R));

	logTransaction(std::to_string(atmID)+": Rollback to " +std::to_string(R)+" bank iterations ago was completed successfully \n");

}

// ATM Implementation
ATM::ATM(int id, const std::string& inputFile, Bank* bank) :
		id(id), stop(false), inputFile(inputFile), bank(bank) , thread(){
	pthread_mutex_init(&stopMutex, nullptr); // Initialize the mutex
}

ATM::~ATM() {
    pthread_mutex_destroy(&stopMutex); // Destroy the mutex
}

void ATM::start() {
	pthread_create(&thread, nullptr, ATM::run, this);
}

void ATM::join() {
	pthread_join(thread, nullptr);
}

void* ATM::run(void* arg) {
	ATM* atm = static_cast<ATM*>(arg);
	std::ifstream file(atm->inputFile);

	if (!file.is_open()) {
		std::cerr << "Error: Could not open file " << atm->inputFile << "\n";
		return nullptr;
	}

	std::string line;
	std::getline(file, line);
	bool isVIP = line.find("VIP") != std::string::npos;

	// Reset the file stream to the beginning
	file.clear();              // Clear EOF flag
	file.seekg(0, std::ios::beg); // Seek to the beginning of the file

	if(!isVIP){
		usleep(100000);
	}
	

	while (std::getline(file, line)) {
		atm->rwLock.acquireWriteLock();
		atm->processCommand(line); // Process the transaction
		pthread_mutex_lock(&atm->stopMutex);
		if (atm->stop) {
			atm->rwLock.releaseWriteLock();
			pthread_mutex_unlock(&atm->stopMutex);
			break;
		}
		atm->rwLock.releaseWriteLock();
		pthread_mutex_unlock(&atm->stopMutex);

		usleep(100000);
	}
	return nullptr;
}

void ATM::processCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string action;


    bool isVIP = command.find("VIP") != std::string::npos;
    bool isPersistent = command.find("PERSISTENT") != std::string::npos;


    // If the command is VIP, submit it to the bank's VIP task queue
    if (isVIP) {

    	size_t vipPos = command.find("VIP=");
    		size_t start = vipPos + 4; // Position of the number after "VIP="
    		size_t end = command.find(' ', start); // Find the next space after the number
    		std::string priorityStr = command.substr(start, end - start); // Extract the numeric part
    		int priority = std::stoi(priorityStr); // Convert to integer
    	

    	// Submit the command as a task to the VIP thread pool
    	bank->submitVIPTask(priority, [this, command, isPersistent]() {
    	    std::istringstream vipIss(command); // Create a stream for parsing
    	    std::string vipAction;

    	    // Execute the VIP command
    	    auto vipExecuteCommand = [&](bool isPersist) -> bool {
				vipIss >> vipAction;
    	        if (vipAction == "O") { // Open account
    	            int accountId, balance;
    	            std::string password;
    	            vipIss >> accountId >> password >> balance;
    	            return bank->createAccount(accountId, password, balance, this->id, isPersist);
    	        } else if (vipAction == "Q") { // Close account
    	            int accountId;
    	            std::string password;
    	            vipIss >> accountId >> password;
    	            return bank->deleteAccount(accountId, password, this->id, isPersist);
    	        } else if (vipAction == "D") { // Deposit
    	            int accountId, amount;
    	            std::string password;
    	            vipIss >> accountId >> password >> amount;
    	            return bank->deposit(accountId, amount, password, this->id, isPersist);
    	        } else if (vipAction == "W") { // Withdraw
    	            int accountId, amount;
    	            std::string password;
    	            vipIss >> accountId >> password >> amount;
    	            return bank->withdraw(accountId, amount, password, this->id, isPersist);
    	        } else if (vipAction == "T") { // Transfer money
    	            int srcId, destId, amount;
    	            std::string password;
    	            vipIss >> srcId >> password >> destId >> amount;
    	            return bank->transfer(srcId, password, destId, amount, this->id, isPersist);
    	        } else if (vipAction == "R") { // Restore Bank
    	            int iterations;
    	            vipIss >> iterations;
    	            return bank->addRestoreRequest(iterations, this->id);
    	        } else if (vipAction == "C") { // Close ATM
    	        	int targetATMID;
    	        	vipIss >> targetATMID;
    	        	return bank->requestATMClosure(targetATMID, this->id, isPersist);
    	        }
    	        return false; // Unknown action
    	    };

    	    // First attempt to execute the command
    	    bool vipSuccess = vipExecuteCommand(isPersistent);

    	    // Retry if the command is persistent and failed
    	    if (isPersistent && !vipSuccess) {
				vipIss.clear();  // Reset the stream
    			vipIss.seekg(0); // Rewind the stream to re-parse input
    	        vipExecuteCommand(false);
    	    }
    	});
    }
	
	else {
    	auto executeCommand = [&](bool isPersist) -> bool {
			iss >> action;
    	        if (action == "O") { // Open account
    	            int accountId, balance;
    	            std::string password;
    	            iss >> accountId >> password >> balance;
    	            return bank->createAccount(accountId, password, balance, this->id, isPersist);
    	        } else if (action == "Q") { // Close account
    	            int accountId;
    	            std::string password;
    	            iss >> accountId >> password;
    	            return bank->deleteAccount(accountId, password, this->id, isPersist);
    	        } else if (action == "D") { // Deposit
    	            int accountId, amount;
    	            std::string password;
    	            iss >> accountId >> password >> amount;
    	            return bank->deposit(accountId, amount, password, this->id, isPersist);
    	        } else if (action == "W") { // Withdraw
    	            int accountId, amount;
    	            std::string password;
    	            iss >> accountId >> password >> amount;
    	            return bank->withdraw(accountId, amount, password, this->id, isPersist);
    	        } else if (action == "B") { // Check balance
    	            int accountId;
    	            std::string password;
    	            iss >> accountId >> password;
    	            return bank->getBalance(accountId, password, this->id, isPersist);
    	        } else if (action == "T") { // Transfer money
    	            int srcId, destId, amount;
    	            std::string password;
    	            iss >> srcId >> password >> destId >> amount;
    	            return bank->transfer(srcId, password, destId, amount, this->id, isPersist);
    	        } else if (action == "R") { // Restore Bank
    	            int iterations;
    	            iss >> iterations;
    	            return bank->addRestoreRequest(iterations, this->id);
    	        } else if (action == "C") { // Close ATM
    	        	int targetATMID;
    	        	iss >> targetATMID;
    	        	return bank->requestATMClosure(targetATMID, this->id,isPersist);
    	        }
    	        return false; // Unknown action
    	    };
    	// Handle non-VIP commands directly
    	    bool success = executeCommand(isPersistent);
    	    sleep(1);

    	    if (isPersistent && !success) {
				iss.clear();  // Reset the stream
    iss.seekg(0); // Rewind the stream to re-parse input
    	        executeCommand(false); // Retry the command
    	        sleep(1);
    	    }
    }
}

void ATM::closeATM() {
	pthread_mutex_lock(&stopMutex);
	stop = true; // Signal the ATM to stop
	pthread_mutex_unlock(&stopMutex);
}


void Bank::logTransaction(const std::string& message) {
	logLock.acquireWriteLock();
	std::ofstream logFile("log.txt", std::ios::app); // Open the log file in append mode
	if (logFile.is_open()) {
		logFile << message;
		logFile.close();
	}
	logLock.releaseWriteLock();
}

ReadWriteLock& ATM::getATMLock(){
	return rwLock;
}