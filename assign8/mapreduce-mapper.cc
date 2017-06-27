/**
 * File: mapreduce-mapper.cc
 * -------------------------
 * Presents the implementation of the MapReduceMapper class,
 * which is charged with the responsibility of pressing through
 * a supplied input file through the provided executable and then
 * splaying that output into a large number of intermediate files
 * such that all keys that hash to the same value appear in the same
 * intermediate.
 */

#include "mapreduce-mapper.h"
#include "mr-names.h"
#include "string-utils.h"
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "ostreamlock.h"         // for oslock, osunlock
#include <iostream>
#include <string>
using namespace std;

MapReduceMapper::MapReduceMapper(const string& serverHost, unsigned short serverPort,
		const string& cwd, const string& executable,
		const string& outputPath, unsigned short numHashCodes) :
	MapReduceWorker(serverHost, serverPort, cwd, executable, outputPath) {
		this->numHashCodes = numHashCodes;
	}

void MapReduceMapper::map() const {
	while (true) {
		string name;
		if (!requestInput(name)) break;
		alertServerOfProgress("About to process \"" + name + "\".");
		string base = extractBase(name);
		string output = outputPath + "/" + changeExtension(base, "input", "mapped");
		bool success = processInput(name, output);

		string key;
		int value;
		ifstream inFile(output);
		while(inFile >> key >> value) {
			size_t hashValue = hash<string>()(key) % numHashCodes;
			string newSuffix = numberToString(hashValue)+ ".mapped";
			string out = outputPath + "/" + changeExtension(base, "input", newSuffix);
			ofstream outFile;
			outFile.open(out, std::ios_base::app);
			outFile << key << " " << value << endl;
			outFile.close();
		}
		inFile.close();
		remove(output.c_str()); // remove the intermediate file
		notifyServer(name, success);
	}

	alertServerOfProgress("Server says no more input chunks, so shutting down.");
}
