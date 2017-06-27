/**
 * File: mapreduce-server.h
 * ------------------------
 * Models the master node in the entire MapReduce
 * system.
 */

#pragma once
#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <set>
#include <vector>
#include <map>
#include "mapreduce-server-exception.h"

class MapReduceServer {
 public:
  MapReduceServer(int argc, char **argv) throw (MapReduceServerException);
  ~MapReduceServer() throw();
  void run() throw();
  
 private:
  unsigned short computeDefaultPortForUser() const throw ();
  void parseArgumentList(int argc, char *argv[]) throw (MapReduceServerException);
  void initializeFromConfigFile(const std::string& configFileName) throw (MapReduceServerException);
  void confirmRequiredArgumentsArePresent(const std::string& configFilename) const throw (MapReduceServerException);
  void confirmExecutablesAreExecutable() const throw (MapReduceServerException);
  void applyToServer(const std::string& key, const std::string& value) throw (MapReduceServerException);
  void buildIPAddressMap() throw();
  void stageFiles(const std::string& directory, std::list<std::string>& files) throw();
  void startServer() throw (MapReduceServerException);
  void logServerConfiguration(std::ostream& os) throw();
  void orchestrateWorkers() throw();
  void handleRequest(int clientSocket, const std::string& clientIPAddress) throw();
  void spawnMappers() throw();
  void spawnReducers() throw();
  void spawnWorker(const std::string& node, const std::string& command) throw();

  std::string buildMapperCommand(const std::string& remoteHost,
                                 const std::string& executable, 
                                 const std::string& outputPath) throw();
  std::string buildReducerCommand(const std::string& remoteHost,
                                 const std::string& executable, 
                                 const std::string& outputPath) throw();
                                  
  bool surfaceNextFilePattern(std::string& pattern) throw();
  void markFilePatternAsProcessed(const std::string& clientIPAddress, const std::string& pattern) throw();
  void rescheduleFilePattern(const std::string& clientIPAddress, const std::string& pattern) throw();

  void dumpFileHashes(const std::string& dir) throw();
  void dumpFileHash(const std::string& file) throw();
  void bringDownServer() throw();

  std::string user;
  std::string host;
  std::string cwd;
  
  int serverSocket;
  unsigned short serverPort;
  bool verbose, mapOnly;
  size_t numMappers;
  size_t numReducers;
  std::string mapper;
  std::string reducer;
  std::string inputPath;
  std::string intermediatePath;
  std::string outputPath;
  std::string mapperExecutable;
  std::string reducerExecutable;
  
  std::vector<std::string> nodes;
  std::map<std::string, std::string> ipAddressMap;
  bool serverIsRunning; // only manipulated in constructor and in server thread, so no lock needed
  std::thread serverThread;
  
  std::list<std::string> unprocessed;
  std::set<std::string> inflight;
  
  // newly added
  std::mutex fileLock;
  
  MapReduceServer(const MapReduceServer& original) = delete;
  MapReduceServer& operator=(const MapReduceServer& rhs) = delete;
};
