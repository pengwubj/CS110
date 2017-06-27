/**
 * File: word-count-mapper.cc
 * --------------------------
 * Implements a sample mapper to be used in your MapReduce
 * system.  This program reads in a text file identified
 * via argv[1] and outputs a file whose name is supplied via
 * argv[2].
 *
 * All MapReduce mappers, of course, need to output a flat
 * text file of key/value pairs.  Because this particular mapper
 * counts words, its output format is the following:
 *
 *  <word-1> 1
 *  <word-2> 1
 *  <word-3> 1
 *
 * That's as straightforward a list of key-value pair as you can get, really.
 *
 * You shouldn't need to change this file.
 */

#include <cstdlib>   // for srand, random
#include <cctype>    // for isalnum
#include <iostream>  // for cout
#include <fstream>   // for ifstream
#include <string>    // for string
#include <algorithm> // for find_if, transform
#include "mr-random.h"

using namespace std;

static void publishWordCountPairs(ifstream& infile, ofstream& outfile) {
  while (true) { // read individual words up to end-of-file
    string word;
    infile >> word;
    if (infile.fail()) break;
    word.erase(remove_if(word.begin(), word.end(), [](int ch){ return !isalnum(ch);}), word.end());
    transform(word.begin(), word.end(), word.begin(), ::tolower); 
    if (!word.empty()) outfile << word << " 1" << endl;
  }
}

static const double kChanceOfMapFailure = 0.2;
static const int kMapSuccess = 0;
static const int kMapFailure = 1;
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
  publishWordCountPairs(infile, outfile);
  int result = randomChance(kChanceOfMapFailure) ? kMapFailure : kMapSuccess;
  if (result == kMapFailure) {
    outfile.close();
    remove(outfileName);
  }
  
  return result;
}
