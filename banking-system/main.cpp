#include "banking_system.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <unistd.h>


int main(int argc, char* argv[]) {
	// Check if there are not enough arguments
	if (argc < 3) { // At least 1 VIP thread and 1 ATM input file are required
		return 1;
	} 
	// Parse the number of VIP threads
	size_t numVIPThreads = std::stoi(argv[1]);
	
	// Initialize the Bank system with VIP threads
	Bank bank(numVIPThreads);
	
	// Number of ATM input files
	int numATMs = argc - 2;


	// Check if all input files are valid before proceeding
	for (int i = 2; i < argc; ++i) {
		std::ifstream file(argv[i]);
		if (!file.is_open()) {
			std::cerr << "Bank error: illegal arguments\n";
			return 1;
		}
	}

	// Create and start ATMs
	std::vector<ATM*> atms;
	for (int i = 0; i < numATMs; ++i) {
		ATM* atm = new ATM(i+1, argv[i + 2], &bank);
		atms.push_back(atm);
		bank.registerATM(atm); // Register the ATM with the bank
		atm->start();
	}

	// Wait for ATM threads to finish
	for (ATM* atm : atms) {
		
		atm->join();
		atm->closeATM();
		delete atm;
	}
bank.stop();
	return 0;
}