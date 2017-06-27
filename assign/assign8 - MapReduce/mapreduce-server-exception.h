/**
 * File: mapreduce-server-exception.h
 * ----------------------------------
 * Defines the class of exception thrown whenever a
 * MapReduceServer can't be configured properly (typically
 * because a command line argument was either missing,
 * malformed, or refers to improperly formatted data).
 */

#pragma once
#include <exception>
#include <string>

class MapReduceServerException: public std::exception {
 public:
  MapReduceServerException(const std::string& message) throw() : message(message) {}
  ~MapReduceServerException() throw() {}
  const char *what() const throw() { return message.c_str(); }
  
 private:
  const std::string message;
};
