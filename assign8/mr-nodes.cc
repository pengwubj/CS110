/**
 * File: mr-nodes.cc
 * -----------------
 * Presents the (very straightforward) implementation of
 * loadMapReduceNodes.  In an ideal world this would actually
 * build a list of myth hostnames that were reachable and ssh'able,
 * but right now it just loads the list of hostnames from a file. #weak
 */

#include "mr-nodes.h"
#include <cstdlib>
#include <set>
#include <thread>
#include <fstream>
#include <mutex>
#include <iostream>
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

static void pollMyth(size_t num, const set<size_t>& exclude, vector<string>& nodes, mutex& m) {
  if (exclude.find(num) != exclude.cend()) return;
  string command = "timeout 5 ssh -o ConnectTimeout=5 myth" + to_string(num) + " date >& /dev/null";
  int ret = system(command.c_str());
  if (ret != 0) return;
  lock_guard<mutex> lg(m);
  nodes.push_back("myth" + to_string(num));
}

static void loadMythsToExclude(set<size_t>& exclude) {
  ifstream infile("/usr/class/cs110/WWW/map-reduce/dead-myths.txt");
  if (!infile) {
    cerr << oslock << "Missing configuration file needed for mr-nodes module... Inform CS110 staff pronto!!!" << endl << osunlock;
    return;
  }

  while (true) {
    string line;
    getline(infile, line);
    if (infile.fail()) break;
    try {
      exclude.insert(stoi(trim(line)));
    } catch (...) {
      cerr << oslock << "Configuration file needed for mr-nodes module is malformed... Inform CS110 staff pronto!!!" << endl << osunlock;
    }
  }
}

static const size_t kNumMyths = 32;
vector<string> loadMapReduceNodes() {
  set<size_t> exclude;
  loadMythsToExclude(exclude);
  vector<string> nodes;
  mutex m;
  thread threads[kNumMyths];
  for (size_t i = 0; i < kNumMyths; i++) threads[i] = thread(pollMyth, i + 1, ref(exclude), ref(nodes), ref(m));
  for (thread& t: threads) t.join();
  return nodes;
}
