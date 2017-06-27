/**
 * File: mapreduce-reducer.h
 * -------------------------
 * Defines the interface for the MapReduceReducer class,
 * which knows how to configure itself (via its constructor) and
 * feed input files to the supplied executable.  Input files are
 * actually input patterns, and all files matching that pattern
 * should be concatenated, sorted, grouped by key, and then
 * placed in a temporary file that is itself fed to the executable
 * capable of reducing all vectors of values into a single value.
 * The input files are expected to end in ".mapped" and the final output
 * files are expected to end in ".output".
 *
 * See mapreduce-worker.h for more documentation on how the constructor
 * should behave.
 */

#pragma once
#include "mapreduce-worker.h"

class MapReduceReducer: protected MapReduceWorker {
	public:
		MapReduceReducer(const std::string& serverHost, 
				unsigned short serverPort,
				const std::string& cwd, 
				const std::string& executable, 
				const std::string& inputPath,
				const std::string& outputPath);
		void reduce() const;
	private:
		std::string inputPath;
};
