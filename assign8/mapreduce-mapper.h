/**
 * File: mapreduce-mapper.h
 * ------------------------
 * Defines the interface for the MapReduceMapper class,
 * which knows how to configure itself (via its constructor) and
 * feed input files to the supplied executable.  Input files received
 * from the map-reduce server should be .input files (that is, the file
 * extension is expected to be ".input"), and the output files are of
 * the form "<name>-00000.mapped", "<name>-00001.mapped","<name>-00002.mapped",
 * and so forth.  The 00000, 00001, etc, are five-digit numbers ranging from
 * 0 up through but not including the split value.
 *
 * You'll need to extend the MapReduceMapper constructor to accept one more parameter
 * (the size_t split value), add state to the MapReduceMapper, and extend the implementation
 * of map to distribute the mapper executable's output across many, many files.
 * 
 * See mapreduce-worker.h for more documentation on how the constructor
 * should behave.
 */

#pragma once
#include "mapreduce-worker.h"

class MapReduceMapper: protected MapReduceWorker {
	public:
		MapReduceMapper(const std::string& serverHost, unsigned short serverPort,
				const std::string& cwd, const std::string& executable,
				const std::string& outputPath, unsigned short numHashCodes);
		void map() const;
	private:
		unsigned short numHashCodes;
};
