/*
	Cache Simulator Implementation by Justin Goins
	Oregon State University
	Fall Term 2018
*/

#include "CacheController.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <cmath>

using namespace std;

CacheController::CacheController(ConfigInfo ci, char* tracefile) {
	// store the configuration info
	this->ci = ci;
	this->inputFile = string(tracefile);
	this->outputFile = this->inputFile + ".out";

	// compute the other cache parameters
	this->numByteOffsetBits = log2(ci.blockSize);
	this->numSetIndexBits = log2(ci.numberSets);

	// initialize the counters
	this->globalCycles = 0;
	this->globalHits = 0;
	this->globalMisses = 0;
	this->globalEvictions = 0;


	// create your cache structure
	// ...

	myCache.block.resize(ci.numberSets);
	for (unsigned int k = 0; k < ci.numberSets; k++)
	{
		myCache.block[k].entry.resize(ci.associativity);
		myCache.block[k].LeastRecentlyUsed.resize(ci.associativity);
		// Initialize LRU placeholders
		for(unsigned int g = 0; g < ci.associativity; g++)
		{
			myCache.block[k].LeastRecentlyUsed[g] = g;
		}
	}

/*	int x = myCache.block[0].LeastRecentlyUsed.at(1);	
	myCache.block[0].LeastRecentlyUsed.erase(myCache.block[0].LeastRecentlyUsed.begin() + x);
	myCache.block[0].LeastRecentlyUsed.push_back(1);	
	for(int i = 0; i < (int)myCache.block[0].LeastRecentlyUsed.size(); i++) {cout << myCache.block[0].LeastRecentlyUsed[i] << ", ";}
	*/
	// TODO: Add data structure that represents cache from class

	// manual test code to see if the cache is behaving properly
	// will need to be changed slightly to match the function prototype
	/*
	cacheAccess(false, 0);
	cacheAccess(false, 128);
	cacheAccess(false, 256);

	cacheAccess(false, 0);
	cacheAccess(false, 128);
	cacheAccess(false, 256);
	*/
}

/*
	Starts reading the tracefile and processing memory operations.
*/
void CacheController::runTracefile() {
	cout << "Input tracefile: " << inputFile << endl;
	cout << "Output file name: " << outputFile << endl;
	
	// process each input line
	string line;
	// define regular expressions that are used to locate commands
	regex commentPattern("==.*");
	regex instructionPattern("I .*");
	regex loadPattern(" (L )(.*)(,[[:digit:]]+)$");
	regex storePattern(" (S )(.*)(,[[:digit:]]+)$");
	regex modifyPattern(" (M )(.*)(,[[:digit:]]+)$");

	// open the output file
	ofstream outfile(outputFile);
	// open the output file
	ifstream infile(inputFile);
	// parse each line of the file and look for commands
	while (getline(infile, line)) {
		// these strings will be used in the file output
		string opString, activityString;
		smatch match; // will eventually hold the hexadecimal address string
		unsigned long int address;
		// create a struct to track cache responses
		CacheResponse response;

		// ignore comments
		if (std::regex_match(line, commentPattern) || std::regex_match(line, instructionPattern)) {
			// skip over comments and CPU instructions
			continue;
		} else if (std::regex_match(line, match, loadPattern)) {
			cout << "Found a load op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, false, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} else if (std::regex_match(line, match, storePattern)) {
			cout << "Found a store op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, true, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		} else if (std::regex_match(line, match, modifyPattern)) {
			cout << "Found a modify op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			// first process the read operation
			cacheAccess(&response, false, address);
			string tmpString; // will be used during the file output
			tmpString.append(response.hit ? " hit" : " miss");
			tmpString.append(response.eviction ? " eviction" : "");
			unsigned long int totalCycles = response.cycles; // track the number of cycles used for both stages of the modify operation
			// now process the write operation
			cacheAccess(&response, true, address);
			tmpString.append(response.hit ? " hit" : " miss");
			tmpString.append(response.eviction ? " eviction" : "");
			totalCycles += response.cycles;
			outfile << " " << totalCycles << tmpString;
		} else {
			throw runtime_error("Encountered unknown line format in tracefile.");
		}
		outfile << endl;
	}
	// add the final cache statistics
	outfile << "Hits: " << globalHits << " Misses: " << globalMisses << " Evictions: " << globalEvictions << endl;
	outfile << "Cycles: " << globalCycles << endl;

	infile.close();
	outfile.close();
}

/*
	Calculate the block index and tag for a specified address.
*/
CacheController::AddressInfo CacheController::getAddressInfo(unsigned long int address) {
	AddressInfo ai;
	// Using address, divide by number of bytes per block and take the floor,
	// then mod by number of indexs
	ai.setIndex = (int)floor((address / ci.blockSize)) % (int)(ci.numberSets);
	ai.tag 		= ai.tag >> (numByteOffsetBits + numSetIndexBits); 
	return ai;
}

/*
	This function allows us to read or write to the cache.
	The read or write is indicated by isWrite.
*/
void CacheController::cacheAccess(CacheResponse* response, bool isWrite, unsigned long int address) {
	// determine the index and tag
	AddressInfo ai = getAddressInfo(address);

	cout << "\tSet index: " << ai.setIndex << ", tag: " << ai.tag << endl;

	// your code needs to update the global counters that track the number of hits, misses, and evictions

	// READ
	if (!isWrite) 
	{
		for (unsigned int i = 0; i < myCache.block[ai.setIndex].entry.size(); i++)
		{
			// HIT: Increment hits, Update response, 
			if ((myCache.block[ai.setIndex].entry[i].isValid) &&
			    (myCache.block[ai.setIndex].entry[i].tag == ai.tag))
			{
				globalHits++;
				response->hit = 1;
				int x = myCache.block[ai.setIndex].LeastRecentlyUsed.at(i);	
				myCache.block[ai.setIndex].LeastRecentlyUsed.erase(myCache.block[ai.setIndex].LeastRecentlyUsed.begin() + x);
				myCache.block[ai.setIndex].LeastRecentlyUsed.push_back(i);	
				break;
			}
		}
		// MISS: Increment misses
		if (!(response->hit))
		{
			globalMisses++;
			response->hit = 0;
		}
	}
	// WRITE
	else
	{
		response->hit = 0;
		// WRITE - EVICTION 
		if (myCache.block[ai.setIndex].entryNum == (int)myCache.block[ai.setIndex].entry.size())
		{
			// Increment globalEvictions, Update response, Update globalCycles
			// REPLACEMENT POLICY == RANDOM
			if(ci.rp == ReplacementPolicy::Random)
			{
				int randNum = rand() % 3;
				myCache.block[ai.setIndex].entry[randNum].tag = ai.tag; 
			}
			// REPLACEMENT POLICY == LEAST RECENTLY USED
			else
			{
				//int temp = myCache.block[ai.setIndex].LeastRecentlyUsed.begin();
				myCache.block[ai.setIndex].entry[0].tag = ai.tag;
			}
			globalEvictions++;
		}
		// Update globalCycles
		// WRITE - FREE ENTRY
		else
		{
			int iterator = 0;
			while (myCache.block[ai.setIndex].entry[iterator].isValid == 1){iterator++;}
			if (myCache.block[ai.setIndex].entry[iterator].tag != ai.tag || 
				myCache.block[ai.setIndex].entryNum == 0)
			{
				myCache.block[ai.setIndex].entry[iterator].tag     = ai.tag;
				myCache.block[ai.setIndex].entry[iterator].isValid = 1;
				myCache.block[ai.setIndex].entryNum++;
			}else{
				cout << "Already In Cache!\n";
				isWrite = 0;
				response->hit = 1;
			}
		}
	}


	if (response->hit)
		cout << "Address " << std::hex << address << " was a hit." << endl;
	else
		cout << "Address " << std::hex << address << " was a miss." << endl;

	cout << "-----------------------------------------" << endl;
	
	updateCycles(response, isWrite);
	return;
}

/*
	Compute the number of cycles used by a particular memory operation.
	This will depend on the cache write policy.
*/
void CacheController::updateCycles(CacheResponse* response, bool isWrite) {
	// your code should calculate the proper number of cycles
	
	if(isWrite)
	{
		response->cycles += ci.cacheAccessCycles + ci.memoryAccessCycles;
		
	}else if(response->hit)
	{
		response->cycles += ci.cacheAccessCycles;
	}else
	{
		response->cycles += ci.memoryAccessCycles + ci.cacheAccessCycles;
	}

	globalCycles = response->cycles;
	//cout << "Added: " << std::dec << globalCycles << " Cycles" << '\n'; 
}