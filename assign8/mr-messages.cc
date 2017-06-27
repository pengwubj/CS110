/**
1;2c * File: mr-messages.cc
 * --------------------
 * Implements the trivial collection of messages that need to be sent from 
 * the server to the mappers and reducers and the equally trivial collection of
 * messages sent from the mappers and reducers back to the server.
 */

#include <sstream>
#include <exception>
#include <cstdlib>
#include <map>
#include "string-utils.h"
#include "mr-messages.h"
#include "mr-env.h"
using namespace std;

static map<MRMessage, string> buildForwardMap() {
  map<MRMessage, string> fm;
  fm[kWorkerReady] = "WORKER_READY";
  fm[kJobStart] = "JOB_START";
  fm[kJobInfo] = "JOB_INFO";
  fm[kJobSucceeded] = "JOB_SUCCEEDED";
  fm[kJobFailed] = "JOB_FAILED";
  fm[kServerDone] = "SERVER_DONE";
  return fm;
}

static map<string, MRMessage> buildBackwardMap() {
  map<MRMessage, string> fm = buildForwardMap();
  map<string, MRMessage> bm;
  for (const auto& p: fm) bm[p.second] = p.first;
  return bm;
}

static map<MRMessage, string> messageToStringMap(buildForwardMap());
static map<string, MRMessage> stringToMessageMap(buildBackwardMap());

void sendWorkerReady(ostream& outstream) { 
  outstream << messageToStringMap[kWorkerReady] << endl; 
}

void sendJobStart(ostream& outstream, const string& chunkFile) { 
  outstream << messageToStringMap[kJobStart] << " " << chunkFile << endl; 
}

void sendJobInfo(ostream& outstream, const string& info) {
  outstream << messageToStringMap[kJobInfo] << " Worker at "
            << getHost() << ": " << info << endl;
}

void sendJobSucceeded(ostream& outstream, const string& chunkFile) { 
  outstream << messageToStringMap[kJobSucceeded] << " " << chunkFile << endl;
}

void sendJobFailed(ostream& outstream, const string& chunkFile) { 
  outstream << messageToStringMap[kJobFailed] << " " << chunkFile << endl;
}

void sendServerDone(ostream& outstream) {
  outstream << messageToStringMap[kServerDone] << endl; 
}

bool receiveMessage(istream& instream, MRMessage& message, string& payload) {
  string line;
  getline(instream, line);
  line = trim(line);
  istringstream iss(line);
  string messageString;
  iss >> messageString;
  const auto iter = stringToMessageMap.find(messageString);
  if (iter == stringToMessageMap.cend()) {
    message = kUnknown;
    return false;
  }

  message = iter->second;
  const MRMessage messagesWithPayloads[] = {kJobStart, kJobInfo, kJobSucceeded, kJobFailed};
  const size_t numMessagesWithPayloads = 
    sizeof(messagesWithPayloads)/sizeof(messagesWithPayloads[0]);
  bool hasPayload = 
    (find(messagesWithPayloads, messagesWithPayloads + numMessagesWithPayloads, 
          message) != messagesWithPayloads + numMessagesWithPayloads);
  if (hasPayload) {
    getline(iss, payload);
    payload = trim(payload);
  }
  return hasPayload;
}
