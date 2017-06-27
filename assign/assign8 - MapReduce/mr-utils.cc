/**
 * File: mr-utils.cc
 * -----------------
 */
 
#include "mr-utils.h"
#include <cstdlib>               // for size_t, strtol
#include <climits>               // for USHRT_MAX
#include <sstream>               // for ostringstream
#include <sys/types.h>           // for struct stat
#include <sys/stat.h>            // for stat
#include <unistd.h>              // for stat
using namespace std;

unsigned short extractPortNumber(const char *portArgument) throw (MapReduceServerException) {
  char *endptr;
  long rawPort = strtol(portArgument, &endptr, 0);
  if (*endptr != '\0') {
    ostringstream oss;
    oss << "The supplied port number of \"" << portArgument << "\" is malformed.";
    throw MapReduceServerException(oss.str());
  }
  
  if (rawPort < 1 || rawPort > USHRT_MAX) {
    ostringstream oss;
    oss << "Port number must be between 1 and " << USHRT_MAX << ".";
    throw MapReduceServerException(oss.str());
  }
  
  return static_cast<unsigned short>(rawPort);
}

size_t parseNumberInRange(const string& key, const string& value, size_t low, size_t high) throw (MapReduceServerException) {
  char *endptr;
  long number = strtol(value.c_str(), &endptr, 0);
  if (*endptr != '\0') {
    ostringstream oss;
    oss << "The configuration file's \"" << key << "\" entry is bound to a value" << endl
        << "of \"" << value << "\", but that value needs to be a positive integer.";
    throw MapReduceServerException(oss.str());
  }
  
  size_t positiveNumber = number;
  if (number < 0 || positiveNumber < low || positiveNumber > high) {
    ostringstream oss;
    oss << "The configuration file's \"" << key << "\" entry is bound to the" << endl
        << "number " << positiveNumber << ", but that number needs be within the range [" << low << ", " << high << "].";
    throw MapReduceServerException(oss.str());
  }

  return positiveNumber;
}

/**
 * Method: ensureDirectoryExists
 * -----------------------------
 * Ensures that the supplied path exists and is visible to the user.
 * Returns the absolute version of the path, even if it was expressed as relative,
 * because these paths needs to be passed across the network between the server and the
 * workers.
 */
string ensureDirectoryExists(const string& key, const string& path, const string& cwd) throw (MapReduceServerException) {
  string absolutePath = path;
  if (!path.empty() && path[0] != '/') {
    ostringstream oss;
    oss << cwd << "/" << path;
    absolutePath = oss.str();
  }

  struct stat st;
  int status = stat(path.c_str(), &st);
  if (status == -1) {
    ostringstream oss;
    oss << "The configuration file's \"" << key << "\" entry identifies a path" << endl
        << "of \"" << path << "\" that either doesn't exist" << endl
        << "or is unreadable.";
    throw MapReduceServerException(oss.str());
  }

  if (!(st.st_mode & S_IFDIR)) {
    ostringstream oss;
    oss << "The configuration file's \"" << key << "\" entry identifies a path" << endl
        << "of \"" << path << "\" that isn't a directory.";
    throw MapReduceServerException(oss.str());
  }

  return absolutePath;
}