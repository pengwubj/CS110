/**
 * File: mr-names.cc
 * -----------------
 * Presents the implementation of those functions defined in mr-names.h
 */

#include "mr-names.h"
#include "string-utils.h"
#include <sstream>
#include <iomanip>
using namespace std;

string extractBase(const string& name) {
  size_t pos = name.rfind('/');
  if (pos == string::npos) return name;
  return name.substr(pos + 1);
}

string changeExtension(const string& name, const string& oldext, const string& newext) {
  if (!endsWith(name, "." + oldext)) return name;
  size_t pos = name.rfind(oldext);
  return name.substr(0, pos) + newext;
}

string numberToString(size_t number, size_t width) {
  ostringstream oss;
  oss << setw(width) << setfill('0') << number;
  return oss.str();
}
