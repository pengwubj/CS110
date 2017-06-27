/**
 * File: word-count-reducer.cc
 * ---------------------------
 * Presents the implementation of a simple reducer that
 * can be used in your MapReduce system.  This program
 * reads a text of key/value-vector pairs via argv[1] and outputs
 * a file whose name is supplied via arg[2].
 *
 * All MapReduce mappers, expect a sorted, flat-text file of key/value-vector pairs and
 * output another sorted, flat-text file of key/value pairs, where the values are the
 * result of some reduction being applied to the incoming vector of values.
 * Because this particular reducer helps to build sorted word-frequency lists, 
 * the file input format should look like this:
 *
 *  <word-1> 1 1 1
 *  <word-2> 1
 *  <word-3> 1 1 1 1 1 1
 *
 * and the output file format should be 
 *
 *  <word-1> 3
 *  <word-2> 1
 *  <word-3> 6
 *
 * Note that each of the two files are lexicographically sorted by word.
 */

#include <iostream> 
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include "mr-random.h"
using namespace std;

static void aggregateAndPublishWordCount(ifstream& infile, ofstream& outfile) {
  while (true) {
    string line;
    getline(infile, line);
    if (infile.fail()) break;   
    istringstream iss(line); 
    string word;
    iss >> word;
    if (iss.fail()) continue;
    int count = 0;
    while (true) {
      string inc;
      iss >> inc;
      if (iss.fail()) break;
      count++;
    }    
    outfile << word << " " << count << endl; 
  }
}

static const double kChanceOfReduceFailure = 0.4;
static const int kReduceSuccess = 0;
static const int kReduceFailure = 1;
static const int kBadArgumentCount = 2;
int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << "wrong number of arguments." << endl;
    return kBadArgumentCount;
  }

  sleepRandomAmount();
  const char *infileName = argv[1];
  ifstream infile(infileName);
  const char *outfileName = argv[2];
  ofstream outfile(outfileName);
  aggregateAndPublishWordCount(infile, outfile);
  int result = randomChance(kChanceOfReduceFailure) ? kReduceFailure : kReduceSuccess;
  if (result == kReduceFailure) {
    outfile.close();
    remove(outfileName);
  }
  return result;
}
