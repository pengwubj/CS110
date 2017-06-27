/**
 * File: mr-hash.h
 * ---------------
 * Defines a specialized template implementation of hash<ifstream>.
 */

#pragma once
#include <string>
#include <fstream>
#include <functional>

/**
 * Type: hash<ifstream>
 * --------------------
 * Specializes the std::hash template class to work on ifstreams
 * so that we can generate lightweight hashes of text files to
 * more easily see if two files match.
 *
 * If you want, you can just ignore this and pretend that
 * hash<ifstream> works as a built-in.
 */

namespace std {
  template <>
  struct hash<std::ifstream> {
    size_t operator()(std::ifstream &infile) const {
      std::hash<std::string> stringHasher;
      size_t runningHash = 0;
      while (true) {
        std::string line;
        std::getline(infile, line);
        if (infile.fail()) return runningHash;
        line += '\n';
        runningHash += stringHasher(line);
      }
    }
  };
}

