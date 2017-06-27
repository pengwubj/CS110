/**
 * File: mr-utils.h
 * ----------------
 * Defines a collection of ulility functions to assist mr, mrm, and mrw.
 */

#pragma once
#include <cstddef>
#include <string>
#include "mapreduce-server-exception.h"

/**
 * Function: extractPortNumber
 * ---------------------------
 * If the user wishes to supply a port number on the command line (probably because their
 * default port number is in use for some reason), then extractPortNumber is called.
 * In a nutshell, it relies on strtol to convert a string to a number, and it confirms that
 * the string was purely numeric and that the number is a legitimate port number.
 */
unsigned short extractPortNumber(const char *portArgument) throw (MapReduceServerException);

/**
 * Function: parseNumberInRange
 * ----------------------------
 * Confirms that the supplied value is indeed a nonnegative number withing 
 * the range [low, high].  If so, parseNumberInRange returns the value as a number.  If not,
 * then a MapReduceServerException is thrown around an actionable error message.
 */
size_t parseNumberInRange(const std::string& key, const std::string& value,
                          size_t low, size_t high) throw (MapReduceServerException);

/**
 * Function: ensureDirectoryExists
 * -------------------------------
 * Ensures that the supplied path exists and is visible to the user.
 * Returns the absolute version of the path, even if it was expressed as relative,
 * because these paths needs to be passed across the network between the server and the
 * workers.
 */
std::string ensureDirectoryExists(const std::string& key, const std::string& path, 
                                  const std::string& cwd) throw (MapReduceServerException);
