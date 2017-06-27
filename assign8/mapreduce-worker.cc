/**
 * File: mapreduce-worker.cc
 * -------------------------
 * Presents the implementation of the MapReduceWorker class.
 */

#include "mapreduce-worker.h"
#include <cassert>
#include <sstream>
#include "mr-messages.h"
#include "string-utils.h"
#include "client-socket.h"
#include "socket++/sockstream.h"
using namespace std;

MapReduceWorker::MapReduceWorker(const string& serverHost, unsigned short serverPort,
                                 const string& cwd, const string& executable,
                                 const string& outputPath) :
  serverHost(serverHost), serverPort(serverPort), cwd(cwd), 
  executable(executable), outputPath(outputPath) {}

bool MapReduceWorker::requestInput(string& name) const {
  int clientSocket = getClientSocket();
  sockbuf sb(clientSocket);
  iosockstream ss(&sb);
  sendWorkerReady(ss);
  MRMessage message;
  string payload;
  receiveMessage(ss, message, payload);
  if (message == kServerDone) return false;
  name = trim(payload);
  return true;
}

bool MapReduceWorker::processInput(const string& name, const string& output) const {
  string executable = cwd + "/" + this->executable;
  ostringstream oss;
  oss << executable << " " << name << " " << output;
  string command = oss.str();
  return system(command.c_str()) == 0;
}

void MapReduceWorker::notifyServer(const string& name, bool success) const {
  int clientSocket = getClientSocket();
  sockbuf sb(clientSocket);
  iosockstream ss(&sb);
  if (success) {
    sendJobSucceeded(ss, name);
  } else {
    sendJobFailed(ss, name);
  }
}

void MapReduceWorker::alertServerOfProgress(const string& info) const {
  int clientSocket = getClientSocket();
  sockbuf sb(clientSocket);
  iosockstream ss(&sb);
  sendJobInfo(ss, info);
}

static const int kServerInaccessible = 2;
int MapReduceWorker::getClientSocket() const {
  int clientSocket = createClientSocket(serverHost, serverPort);
  if (clientSocket == kClientSocketError) exit(kServerInaccessible);
  return clientSocket;
}
