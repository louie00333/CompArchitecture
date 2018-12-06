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

CacheController::CacheController(ConfigInfo ci, char *tracefile)
{
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
		//myCache.block[k].LeastRecentlyUsed.resize(ci.associativity);
		// Initialize LRU placeholders
		for (unsigned int g = 0; g < ci.associativity; g++)
		{
			//myCache.block[k].LeastRecentlyUsed[g] = g;
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
void CacheController::runTracefile()
{
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
	while (getline(infile, line))
	{
		// these strings will be used in the file output
		string opString, activityString;
		smatch match; // will eventually hold the hexadecimal address string
		unsigned long int address;
		// create a struct to track cache responses
		CacheResponse response;

		// ignore comments
		if (std::regex_match(line, commentPattern) || std::regex_match(line, instructionPattern))
		{
			// skip over comments and CPU instructions
			continue;
		}
		else if (std::regex_match(line, match, loadPattern))
		{
			cout << "Found a load op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, false, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		}
		else if (std::regex_match(line, match, storePattern))
		{
			cout << "Found a store op!" << endl;
			istringstream hexStream(match.str(2));
			hexStream >> std::hex >> address;
			outfile << match.str(1) << match.str(2) << match.str(3);
			cacheAccess(&response, true, address);
			outfile << " " << response.cycles << (response.hit ? " hit" : " miss") << (response.eviction ? " eviction" : "");
		}
		else if (std::regex_match(line, match, modifyPattern))
		{
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
		}
		else
		{
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
CacheController::AddressInfo CacheController::getAddressInfo(unsigned long int address)
{
	AddressInfo ai;
	// Using address, divide by number of bytes per block and take the floor,
	// then mod by number of indexs

	ai.setIndex = (int)floor((address / ci.blockSize));
	if (ai.setIndex >= ci.numberSets)
	{
		ai.setIndex = ai.setIndex % (int)(ci.numberSets);
	}
	ai.tag = address >> (numByteOffsetBits + numSetIndexBits);
	return ai;
}

/*
	This function allows us to read or write to the cache.
	The read or write is indicated by isWrite.
*/
void CacheController::cacheAccess(CacheResponse *response, bool isWrite, unsigned long int address)
{
	// determine the index and tag
	AddressInfo ai = getAddressInfo(address);
	response->hit = 0;
	response->eviction = 0;
	response->cycles = 0;
	cout << "\tSet index: " << ai.setIndex << ", tag: " << std::hex << ai.tag << endl;
	
	cout << "ENTRY CONTENTS: ";
	for (int i = 0; i < (int)myCache.block[ai.setIndex].entry.size(); i++)
		cout << ' ' << myCache.block[ai.setIndex].entry[i].tag;
	cout << '\n';

	int inCacheFlag = 0;
	// CHECK IF IN CACHE
	cout << "INPUTTED TAG COMPARISON: " << ai.tag << '\n';
	for (unsigned int i = 0; i < myCache.block[ai.setIndex].LeastRecentlyUsed.size(); i++)
	{
		cout << "CHECKING: " << myCache.block[ai.setIndex].entry[i].tag << '\n';
		// HIT: Increment hits, Update response,
		if ((myCache.block[ai.setIndex].entry[i].isValid) &&
			(myCache.block[ai.setIndex].entry[i].tag == ai.tag))
		{
			cout << "tag: " << ai.tag << " IN CACHE\n";
			globalHits++;
			response->hit = 1;
			inCacheFlag = 1;
			if ((unsigned int)myCache.block[ai.setIndex].entryNum >= ci.associativity &&
				(unsigned int)myCache.block[ai.setIndex].LeastRecentlyUsed.back() != i)
			{
				cout << "LRU CONTENTS BEFORE: ";
				for (int i = 0; i < (int)myCache.block[ai.setIndex].LeastRecentlyUsed.size(); ++i)
				cout << ' ' << myCache.block[ai.setIndex].LeastRecentlyUsed[i];
				cout << '\n';
				myCache.block[ai.setIndex].LeastRecentlyUsed.erase(myCache.block[ai.setIndex].LeastRecentlyUsed.begin());
				myCache.block[ai.setIndex].LeastRecentlyUsed.push_back(i);
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.back()].tag = ai.tag; 
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.back()].isValid = 1;
				//int x = myCache.block[ai.setIndex].LeastRecentlyUsed[];
				/*
				int x = myCache.block[ai.setIndex].LeastRecentlyUsed.at(i);
				cout << "using at gives: " << x << '\n';
				myCache.block[ai.setIndex].LeastRecentlyUsed.erase(myCache.block[ai.setIndex].LeastRecentlyUsed.begin() + x);
				myCache.block[ai.setIndex].LeastRecentlyUsed.push_back(i);
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.size() - 1].tag = ai.tag;
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.size() - 1].isValid = 1;
				inCacheFlag = 1;
				*/
			}
			else
			{
			}
			break;
		}
	}
	// If NOT IN CACHE
	// MISS
	if (!inCacheFlag)
	{
		cout << "NOT IN CACHE\n";
		globalMisses++;
		response->hit = 0;
		// Check if space in Cache
		// IF No Space left in Cache
		if ((unsigned)myCache.block[ai.setIndex].entryNum >= ci.associativity)
		{
			cout << "EVICTION\n";
			response->eviction = 1;
			globalEvictions++;
			// EVICTION
			// RP: RANDOM
			if (ci.rp == ReplacementPolicy::Random)
			{
				int randNum = rand() % 3;
				myCache.block[ai.setIndex].entry[randNum].tag = ai.tag;
			}
			// RP: LRU
			else
			{
				cout << "LRU CONTENTS BEFORE: ";
				for (int i = 0; i < (int)myCache.block[ai.setIndex].LeastRecentlyUsed.size(); ++i)
				cout << ' ' << myCache.block[ai.setIndex].LeastRecentlyUsed[i];
				cout << '\n';
			//	for (int i = 0; i < (int)myCache.block[ai.setIndex].LeastRecentlyUsed.size(); ++i)
				int tempHolder = myCache.block[ai.setIndex].LeastRecentlyUsed[0];
				cout << "Moving: " << tempHolder << " to end of array\n";
				myCache.block[ai.setIndex].LeastRecentlyUsed.erase(myCache.block[ai.setIndex].LeastRecentlyUsed.begin());
				myCache.block[ai.setIndex].LeastRecentlyUsed.push_back(tempHolder);
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.back()].tag = ai.tag; 
				myCache.block[ai.setIndex].entry[myCache.block[ai.setIndex].LeastRecentlyUsed.back()].isValid = 1;
				// replace tag of LRU
				// Location of LRU[0] is location of LRU in 'entry'
				/*
				int iterator = myCache.block[ai.setIndex].LeastRecentlyUsed.at(0);
				myCache.block[ai.setIndex].LeastRecentlyUsed.erase(myCache.block[ai.setIndex].LeastRecentlyUsed.begin());
				cout << "Iterator is " << iterator << '\n';
				myCache.block[ai.setIndex].LeastRecentlyUsed.push_front(iterator);
				myCache.block[ai.setIndex].entry[iterator].tag = ai.tag;
				*/
				// Update LRU deque
			}
		}
		// IF Space left in cache (spaces that aren't valid)
		// Append to Cache at end
		else
		{
			cout << "SPACE IN CACHE\n";
			for (int i = 0; i < (int)myCache.block[ai.setIndex].entry.size(); i++)
			{

				if (myCache.block[ai.setIndex].entry[i].isValid != 1)
				{
					cout << "ADDING TO ENTRY: " << i << "\n";
					cout << "NEW NUMBER OF ENTRIES: " << myCache.block[ai.setIndex].entryNum + 1 << '\n';
					myCache.block[ai.setIndex].entryNum++;
					cout << "TAG: " << ai.tag << " AT ENTRY " << i << '\n';
					myCache.block[ai.setIndex].entry[i].tag = ai.tag;
					myCache.block[ai.setIndex].entry[i].isValid = 1;
					myCache.block[ai.setIndex].LeastRecentlyUsed.push_back(i);
					break;
				}
			}
		}
	}
	
	cout << "LRU CONTENTS AFTER: ";
	for (int i = 0; i < (int)myCache.block[ai.setIndex].LeastRecentlyUsed.size(); i++)
		cout << ' ' << myCache.block[ai.setIndex].LeastRecentlyUsed[i];
	cout << '\n';

	cout << "ENTRY CONTENTS: ";
	for (int i = 0; i < (int)myCache.block[ai.setIndex].entry.size(); i++)
		cout << ' ' << myCache.block[ai.setIndex].entry[i].tag;
	cout << '\n';
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
void CacheController::updateCycles(CacheResponse *response, bool isWrite)
{
	// your code should calculate the proper number of cycles

	if (isWrite)
	{
		response->cycles += ci.cacheAccessCycles + ci.memoryAccessCycles;
	}
	else if (response->hit)
	{
		response->cycles += ci.cacheAccessCycles;
	}
	else
	{
		response->cycles += ci.memoryAccessCycles + ci.cacheAccessCycles;
	}

	globalCycles += response->cycles;
	//cout << "Added: " << std::dec << globalCycles << " Cycles" << '\n';
}