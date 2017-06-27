/**
 * File: mr-env.cc
 * ---------------
 * Provides the implementation of the environment-variable-oriented
 * functions exported by mr-env.h.
 */

#include "mr-env.h"
#include <climits>  // for HOST_NAME_MAX, PATH_MAX constants
#include <unistd.h> // for getuid, gethostname, getcwd
#include <pwd.h>    // for getpwuid
using namespace std;

/**
 * Function: getUser
 * -----------------
 * Returns the SUNet ID of the logged in user.
 */
string getUser() throw (MapReduceServerException) {
  struct passwd pw, *result = NULL;
  char buf[100000]; // huge space that will generally be large enough
  getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result); // getuid can't fail, getpwuid_r is reentrant
  if (result == NULL) 
    throw MapReduceServerException("Could not determine your SUNetID.");
  return pw.pw_name;
}

/**
 * Function: getHost
 * -----------------
 * Returns the hostname where the server is running (e.g. "myth15",
 * but without the ".stanford.edu".
 */
string getHost() throw (MapReduceServerException) {
  char name[HOST_NAME_MAX + 1];
  if (gethostname(name, HOST_NAME_MAX + 1) == -1) // function is thread safe
    throw MapReduceServerException("Could not determine the name of the host machine.");
  return name;
}

/**
 * Function: getCurrentWorkingDirectory
 * ------------------------------------
 * Returns the current working directory.
 */
string getCurrentWorkingDirectory() throw (MapReduceServerException) {
  char cwd[PATH_MAX + 1];
  if (getcwd(cwd, PATH_MAX + 1) == NULL) // function is thread safe
    throw MapReduceServerException("Could not determine your current working directory.");
  return cwd;
}
