/**
 * File: mapreduce-worker.h
 * ------------------------
 * Defines the MapReduceWorker base class, which consolidates
 * state and functionality common to both the MapReduceMapper
 * and MapReduceReducer subclasses.  See each of "mapreduce-mapper.h" and
 * "mapreduce-reducer.h" for insight as to how this base class works on
 * behalf of its two subclasses.
 */

#pragma once
#include <string>
#include <fstream>

class MapReduceWorker {
 protected:
/**
 * Protected Contructor: MapReduceWorker
 * -------------------------------------
 * Constructs the portion of the class that's common to all subclasses.
 * This constructor is protected (instead of public) to prevent objects
 * of type MapReduceWorker from existing.
 */
  MapReduceWorker(const std::string& serverHost, 
                  unsigned short serverPort,
                  const std::string& cwd, 
                  const std::string& executable, 
                  const std::string& outputPath);
  
/**
 * Inherited state value: serverHost
 * ---------------------------------
 * The host where mr--that is, the map-reduce server--is being run.
 * MapReduceWorker subclasses need to know what map-reduce server to connect
 * to and issue requests to.
 */
  std::string serverHost;

/**
 * Inherited state value: serverPort
 * ---------------------------------
 * The port that the server is listening to.
 */
  unsigned short serverPort;

/**
 * Inherited state values: cwd, executable, outputPath
 * ---------------------------------------------------
 * The current working directory where the server executable
 * resides, the name of the worker executable that should be
 * invoked to process all input files, and the directory where
 * all output files should be placed.
 *
 * The current working directory and the outputPath are expected to
 * be absolute paths, whereas the executable may be either relative (to
 * the cwd) or absolute.
 */
  std::string cwd, executable, outputPath;

/**
 * Inherited method: getClientSocket
 * ---------------------------------
 * Establishes a connection to the map-reduce server, which is expected
 * to be running on the server host/port pair supplied at construction
 * time.
 */
  int getClientSocket() const;

/**
 * Inherited method: requestInput
 * ------------------------------
 * Asks the server to supply the name of a file name (or file pattern that
 * matches one or more file names).  If the server supplies a file name/pattern,
 * that name/pattern will be placed in the string referenced by name, and the return
 * value will be true.  If the server reports back that all files have
 * been processed, then the space referenced by name is left alone, and the return
 * value will be false.
 */
  bool requestInput(std::string& name) const;

/**
 * Method: processInput
 * --------------------
 * Launches executable (supplied at construction time) to process
 * all files matching the supplied name/pattern, and places the executable's
 * output in a file named <output>.
 */
  bool processInput(const std::string& name, const std::string& output) const;

/**
 * Inherited method: notifyServer
 * ------------------------------
 * Informs the server whether or not all of the files matching the
 * the supplied name (which might be a pattern, ala "*-00000.mapped")
 * succeeded.
 */
  void notifyServer(const std::string& name, bool success) const;

/**
 * Inherited method: alertServerOfProgress
 * ---------------------------------------
 * Sends an arbitrary info string back to the server so that the
 * server than print it alongside all other progress strings from
 * all other workers in one console window.
 */
  void alertServerOfProgress(const std::string& info) const;

 private:
  MapReduceWorker(const MapReduceWorker& original) = delete;
  void operator=(const MapReduceWorker& rhs) = delete;
};
