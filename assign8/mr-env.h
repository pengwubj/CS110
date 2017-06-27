/**
 * File: mr-env.h
 * --------------
 * Provides a collection of obvious functions that surface
 * various values about the user environment.  All functions
 * exported by mr-env are thread safe.
 */

#pragma once
#include <string>
#include "mapreduce-server-exception.h"

/**
 * Function: getUser
 * -----------------
 * Returns the SUNet ID of the person running the server.  A
 * MapReduceServerException is thrown in the rare case where the
 * SUNet ID can't be determined.
 */
std::string getUser() throw (MapReduceServerException);

/**
 * Function: getHost
 * -----------------
 * Returns the name of the machine where the server is running. A
 * MapReduceServerException is thrown in the rare case where the
 * hostname can't be determined.
 */
std::string getHost() throw (MapReduceServerException);

/**
 * Function: getCurrentWorkingDirectory
 * ------------------------------------
 * Returns the absolute pathname of the current working directory. A
 * MapReduceServerException is thrown in the rare case where the
 * current working directory can't be determined.
 */
std::string getCurrentWorkingDirectory() throw (MapReduceServerException);
