/**
 * File: mapreduce-reducer.cc
 * --------------------------
 * Presents the implementation of the MapReduceReducer class,
 * which is charged with the responsibility of collating all of the
 * intermediate files for a given hash number, sorting that collation,
 * grouping the sorted collation by key, and then pressing that result
 * through the reducer executable.
 *
 * See the documentation in mapreduce-reducer.h for more information.
 */

#include "mapreduce-reducer.h"
#include "ostreamlock.h"         // for oslock, osunlock
#include "mr-names.h"
#include <iostream>
using namespace std;

MapReduceReducer::MapReduceReducer(const string& serverHost, unsigned short serverPort,
		const string& cwd, const string& executable, const string& inputPath, const string& outputPath) : 
  MapReduceWorker(serverHost, serverPort, cwd, executable, outputPath) {
	  this->inputPath = inputPath;
  }

void MapReduceReducer::reduce() const {
	
	while (true) {
		string name;
		if (!requestInput(name)) break;
		string base = extractBase(name).substr(2);

		string command = "cat " + name;
		command += " | sort | python " + cwd + "/group-by-key.py > ";
		command += outputPath + "/" + changeExtension(base, "mapped", "out");

		int status = system(command.c_str());
		if(status != -1) status = WEXITSTATUS(status);

		alertServerOfProgress("About to process \"" + name + "\".");
		string before = outputPath + "/" + changeExtension(base, "mapped", "out");
		string after = outputPath + "/" + changeExtension(base, "mapped", "output");

		bool success = processInput(before, after);
		remove(before.c_str()); // remove the intermediate file

		notifyServer(name, success);
	}

	alertServerOfProgress("Server says no more input chunks, so shutting down.");
}
