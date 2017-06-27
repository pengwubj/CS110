/**
 * File: mapreduce-server.cc
 * -------------------------
 * Presents the implementation of the MapReduceServer class.
 */

#include "mapreduce-server.h"
#include "thread-pool.h"
#include <iostream>              // for cout
#include <getopt.h>              // for getopt_long
#include <sstream>               // for ostringstream
#include <functional>            // for hash<string>
#include <climits>               // for USHRT_MAX
#include <cstring>               // for memset
#include <vector>                // for vector
#include <set>                   // for set
#include <algorithm>             // for random_shuffle
#include <functional>            // for hash
#include <fstream>               // for ifstream
#include <unistd.h>              // for close
#include <netdb.h>               // for gethostbyname, struct hostent *
#include <sys/socket.h>          // for socket, bind, accept, listen, etc.
#include <arpa/inet.h>           // for htonl, htons, etc.  
#include <dirent.h>              // for DIR, dirent
#include <socket++/sockstream.h>
#include "mr-nodes.h"            // for getting list of nodes in myth cluster that actually work!
#include "mr-messages.h"         // for messages between client and server
#include "mr-utils.h"            // 
#include "mr-env.h"              // for getUser, getHost, etc
#include "mr-hash.h"             // for specialized hash<ifstream>
#include "mr-names.h"            // for numberToString
#include "string-utils.h"        // for trim
#include "server-socket.h"       // for createServerSocket, kServerSocketFailure
#include "ostreamlock.h"         // for oslock, osunlock
#include "mapreduce-server-exception.h"
using namespace std;

/**
 * Constructor: MapReduceServer
 * ----------------------------
 * Configures the server using the well-formed contents of the configuration file supplied
 * on the command line, and then launches the server to get ready for the flash mob
 * of worker requests to map and reduce data input files.
 */
static const size_t kNumWorkerThreads = 32;
MapReduceServer::MapReduceServer(int argc, char *argv[]) throw (MapReduceServerException) 
	: user(getUser()), host(getHost()), cwd(getCurrentWorkingDirectory()),
	serverPort(computeDefaultPortForUser()), verbose(true), mapOnly(false),
	serverIsRunning(false) {
		parseArgumentList(argc, argv);
		if (verbose) cout << "Determining which machines in the myth cluster can be used... " << flush;
		nodes = loadMapReduceNodes();
		if (verbose) cout << "[done!!]" << endl;
		buildIPAddressMap();
		startServer();
		logServerConfiguration(cout);

	}

/**
 * Method: run
 * -----------
 * Presents the high-level scripts of what the server must accomplish in order to ensure that
 * an entire MapReduce job runs to completion.  See the documentation for each of the
 * methods called by run to gain a better sense of how everything works.
 */
void MapReduceServer::run() throw() {
	spawnMappers();
	if(!mapOnly) {
		spawnReducers();
	}
}

/**
 * Method: parseArgumentList
 * -------------------------
 * Self-explanatory.  The --config/-c flag is required, the --port/-p flag.
 */
static const string kUsageString = "./mr --mapper <mapper-name> --reducer <reducer-name> --config <config-file> [--quiet] [--port <port>] [--map-only]";
static const unsigned short kDefaultServerPort = 12345;
void MapReduceServer::parseArgumentList(int argc, char *argv[]) throw (MapReduceServerException) {
	struct option options[] = {
		{"mapper", required_argument, NULL, 'm'},
		{"reducer", required_argument, NULL, 'r'},
		{"quiet", no_argument, NULL, 'q'},
		{"port", required_argument, NULL, 'p'},
		{"config", required_argument, NULL, 'c'},
		{"map-only", no_argument, NULL, 'o'},
		{NULL, 0, NULL, 0},
	};

	ostringstream oss;
	string configFilename;
	while (true) {
		int ch = getopt_long(argc, argv, "m:r:qp:c:o", options, NULL);
		if (ch == -1) break;
		switch (ch) {
			case 'm':
				if (!mapper.empty()) {
					oss << "The mapper should be specified exactly once." << endl;
					oss << kUsageString;
					throw MapReduceServerException(oss.str());
				}
				mapper = optarg;
				break;
			case 'r':
				if (!reducer.empty()) {
					oss << "The reducer should be specified exactly once." << endl;
					oss << kUsageString;
					throw MapReduceServerException(oss.str());
				}
				reducer = optarg;
				break;
			case 'q':
				verbose = false;
				break;
			case 'p':
				serverPort = extractPortNumber(optarg);
				break;
			case 'c':
				configFilename = optarg;
				break;
			case 'o':
				mapOnly = true;
				break;
			default:
				oss << "Unrecognized or improperly supplied flag." << endl;
				oss << kUsageString;
				throw MapReduceServerException(oss.str());
		}
	}

	argc -= optind;
	if (argc > 0) {
		oss << "Too many arguments." << endl;
		oss << kUsageString;
		throw MapReduceServerException(oss.str());
	}

	confirmRequiredArgumentsArePresent(configFilename);
	initializeFromConfigFile(configFilename);
	confirmExecutablesAreExecutable();
}

/**
 * Method: computeDefaultPortForUser
 * ---------------------------------
 * Uses the username and hostname to compute a default server
 * port number (to be used in the event one isn't supplied
 * on the command line).
 */
static const unsigned short kLowestOpenPortNumber = 1024;
unsigned short MapReduceServer::computeDefaultPortForUser() const throw () {
	size_t hashValue = hash<string>()(user + "@" + host); // ensure different than http-proxy default port
	return hashValue % (USHRT_MAX - kLowestOpenPortNumber) + kLowestOpenPortNumber;
}

/**
 * Method: confirmRequiredArgumentsArePresent
 * ------------------------------------------
 * Confirms that certain flags and arguments were supplied on the command line.
 */
void MapReduceServer::confirmRequiredArgumentsArePresent(const string& configFilename) const throw (MapReduceServerException) {
	ostringstream oss;
	if (mapper.empty()) {
		oss << "The mapper must be specified." << endl;
		oss << kUsageString;
		throw MapReduceServerException(oss.str());
	}
	if (reducer.empty()) {
		oss << "The reducer must be specified." << endl;
		oss << kUsageString;
		throw MapReduceServerException(oss.str());
	}
	if (configFilename.empty()) {
		oss << "You must supply a configuration file as an argument." << endl;
		oss << kUsageString;
		throw MapReduceServerException(oss.str());
	}
}

/**
 * Method: confirmExecutablesAreExecutable
 * ---------------------------------------
 * Does the best job it can to confirm that the supplied executable names
 * identify executables that can be run by the user driving the map reduce job.
 */
void MapReduceServer::confirmExecutablesAreExecutable() const throw (MapReduceServerException) {
	struct executable {
		string executableName;
		string errorMessage; 
	} executables[] = {
		{mapper, "The mapper client named \"" + mapper + "\" either doesn't exist or isn't executable."},
		{reducer, "The reducer client named \"" + reducer + "\" either doesn't exist or isn't executable."},
		{mapperExecutable, "The mapper executable named \"" + mapperExecutable + "\" (to be invoked by \"" + mapper + ") either doesn't exist or isn't executable."},
		{reducerExecutable, "The reducer executable named \"" + reducerExecutable + "\" (to be invoked by \"" + reducer + ") either doesn't exist or isn't executable."},
	};

	for (const executable& e: executables) {
		if (access(e.executableName.c_str(), X_OK) != 0) {
			ostringstream oss;
			oss << e.errorMessage;
			throw MapReduceServerException(oss.str());
		}
	}
}

/**
 * Method: initializeFromConfigFile
 * --------------------------------
 * Opens the supplied configuation file, confirms that it exists, confirms
 * that the configuation file is well-formed and supplies all of the necessary
 * information, then updates the server object with the data found within the
 * file, confirms all directories identified within the configuration file (be they absolute or
 * relative to the executable directory) exist.  Oodles of error checking is done here to minimize
 * the possibility that the server crashes in some more oblique way later on.
 */
static const string kConfigFileKeys[] = {
	"mapper", "reducer", "num-mappers", "num-reducers",
	"input-path", "intermediate-path", "output-path"
};
static const size_t kNumConfigFileKeys = sizeof(kConfigFileKeys)/sizeof(kConfigFileKeys[0]);
void MapReduceServer::initializeFromConfigFile(const string& configFileName) throw (MapReduceServerException) {
	ifstream infile(configFileName);
	if (!infile) {
		ostringstream oss;
		oss << "Configuration file named \"" << configFileName << "\" could not be opened.";
		throw MapReduceServerException(oss.str());
	}

	set<string> requiredKeys(kConfigFileKeys, kConfigFileKeys + kNumConfigFileKeys);
	set<string> suppliedKeys;
	while (true) {
		string key;
		getline(infile, key, /* stopchar = */ ' ');
		if (infile.fail()) break;
		key = trim(key);
		if (key.empty()) continue;
		if (requiredKeys.find(key) == requiredKeys.cend()) {
			ostringstream oss;
			oss << "Configuration file key of \"" << key << "\" not recognized.";
			throw MapReduceServerException(oss.str());
		}
		if (suppliedKeys.find(key) != suppliedKeys.cend()) {
			ostringstream oss;
			oss << "Configuration file key of \"" << key << "\" supplied multiple times.";
			throw MapReduceServerException(oss.str());
		}
		suppliedKeys.insert(key);
		string value;
		getline(infile, value); // read rest of line
		applyToServer(key, trim(value));
	}

	if (suppliedKeys != requiredKeys) {
		ostringstream oss;
		oss << "One or more required keys missing from configuration file." << endl;
		throw MapReduceServerException(oss.str());
	}
}

/**
 * Method: applyToServer
 * ---------------------
 * Dispatches on each of the various key values and assigns the corresponding
 * server data member to the supplied value.  Relative directories are canonicalized
 * to be absolute directories, and the number of mappers and number of reducers are
 * forced to be a small, positive number.
 */
static const size_t kMinWorkers = 1;
static const size_t kMaxWorkers = kNumWorkerThreads;
void MapReduceServer::applyToServer(const string& key, const string& value) throw (MapReduceServerException) {
	if (key == "mapper") {
		mapperExecutable = value;
	} else if (key == "reducer") {
		reducerExecutable = value;
	} else if (key == "num-mappers") {
		numMappers = parseNumberInRange(key, value, kMinWorkers, kMaxWorkers);
	} else if (key == "num-reducers") {
		numReducers = parseNumberInRange(key, value, kMinWorkers, kMaxWorkers);
	} else if (key == "input-path") {
		inputPath = ensureDirectoryExists(key, value, cwd);
	} else if (key == "intermediate-path") {
		intermediatePath = ensureDirectoryExists(key, value, cwd);
	} else if (key == "output-path") {
		outputPath = ensureDirectoryExists(key, value, cwd);
	}
}

/**
 * Method: logServerConfiguration
 * ------------------------------
 * Publishes the state of the server object (assuming it's been fully
 * initialized) just so we can sanity check all of the values that will certainly
 * influence execution.
 */
void MapReduceServer::logServerConfiguration(ostream& os) throw() {
	if (!verbose) return;
	os << "Mapper executable: " << mapperExecutable << endl;
	os << "Reducer executable: " << reducerExecutable << endl;
	os << "Number of Mapping Workers: " << numMappers << endl;
	os << "Number of Reducing Workers: " << numReducers << endl;
	os << "Input Path: " << inputPath << endl;
	os << "Intermediate Path: " << intermediatePath << endl;
	os << "Output Path: " << outputPath << endl;
	os << "Server running on port " << serverPort << endl;
	os << endl;
}

/**
 * Method: buildIPAddressMap
 * -------------------------
 * Compiles a map of IP addresses (e.g. "171.64.64.123") to hostname (e.g. "myth21.stanford.edu").
 * This is done so that logging messages can print hostnames instead of IP addresses, even though
 * IP addresses are more readily surfaced by all of the socket API functions.
 */
void MapReduceServer::buildIPAddressMap() throw() {
	ipAddressMap["127.0.0.1"] = host + ".stanford.edu";
	for (const string& node: nodes) {
		struct hostent *he = gethostbyname(node.c_str());
		if (he == NULL) {
			cerr << node << ".stanford.edu is unreachable." << endl;
			continue;
		}
		for (size_t i = 0; he->h_addr_list[i] != NULL; i++) {
			string ipAddress = inet_ntoa(*(struct in_addr *)(he->h_addr_list[i]));
			ipAddressMap[ipAddress] = node + ".stanford.edu";
		}
	}
}

/**
 * Method: stageFiles
 * ------------------
 * Examines the named directory for its legit entries and populates the provided
 * list with the absolute path names of all these files.
 */
void MapReduceServer::stageFiles(const std::string& directory, list<string>& files) throw() {
	DIR *dir = opendir(directory.c_str());
	if (dir == NULL) {
		cerr << "Directory named \"" << directory << "\" could not be opened." << endl;
		exit(1); // this is serious enough that we should just end the program
	} 

	while (true) {
		struct dirent *ent = readdir(dir);
		if (ent == NULL) break;
		if (ent->d_name[0] == '.') continue; // ".", "..", or some hidden file should be ignored
		string file(directory);
		file += "/";
		file += ent->d_name;
		files.push_back(file);
	}

	closedir(dir); // ignore error, since we can still proceed without having closed the directory
}

/**
 * Method: startServer
 * -------------------
 * Creates a server socket to listen in on the (possibly command-line supplied)
 * server port, and then launches the server itself in a separate thread.
 * (The server needs to run off the main thread, because the main thread needs to move on
 * to stage input files, spawn workers to apply mappers to those input files, wait
 * until all input files have been processed, run groupByKey, and so forth.
 */
void MapReduceServer::startServer() throw (MapReduceServerException) {
	serverSocket = createServerSocket(serverPort);
	if (serverSocket == kServerSocketFailure) {
		ostringstream oss;
		oss << "Port " << serverPort << " is already in use, so server could not be launched.";
		throw MapReduceServerException(oss.str());
	}

	serverThread = thread([this] { orchestrateWorkers(); });
}

/**
 * Method: orchstrateWorkers
 * -------------------------
 * Implements the canonical server, which loops interminably, handling incoming
 * network requests which, in this case, will be requests from the farm of previously
 * spawned workers.
 */
void MapReduceServer::orchestrateWorkers() throw () {
	serverIsRunning = true;
	while (true) {
		struct sockaddr_in clientAddress;
		socklen_t clientAddressSize = sizeof(clientAddress);
		memset(&clientAddress, 0, clientAddressSize);
		int clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressSize);
		if (!serverIsRunning) { close(clientSocket); close(serverSocket); break; }
		string clientIPAddress(inet_ntoa(clientAddress.sin_addr));
		if (verbose) 
			cout << oslock << "Received a connection request from " 
				<< ipAddressMap[clientIPAddress] << "." << endl << osunlock;
		handleRequest(clientSocket, clientIPAddress);
	}
}

/**
 * Method: handleRequest
 * ---------------------
 * Manages the incoming message from some worker running on the supplied IP address and,
 * in some cases, responds to the same worker with some form of an acknowledgement that the message was
 * received.
 */
void MapReduceServer::handleRequest(int clientSocket, const string& clientIPAddress) throw() {
	if (verbose) 
		cout << oslock << "Incoming communication from " << ipAddressMap[clientIPAddress]
			<< " on descriptor " << clientSocket << "." << endl << osunlock;
	sockbuf sb(clientSocket);
	iostream ss(&sb);
	MRMessage message;
	string payload;
	try {
		receiveMessage(ss, message, payload);
	} catch (exception& e) {
		if (verbose)
			cout << oslock << "Spurious connection received from " << ipAddressMap[clientIPAddress]
				<< " on descriptor " << clientSocket << "." << endl << osunlock;
		return;
	}

	if (message == kWorkerReady) {
		string pattern;
		fileLock.lock();
		bool success = surfaceNextFilePattern(pattern);
		fileLock.unlock();
		if (success) {
			if (verbose)
				cout << oslock << "Instructing worker at " << ipAddressMap[clientIPAddress] << " to process "
					<< "this pattern: \"" << pattern << "\"" << endl << osunlock;
			sendJobStart(ss, pattern);
		} else { // no more patterns to process, so kill the worker
			if (verbose) 
				cout << oslock << "Informing worker at " << ipAddressMap[clientIPAddress] << " that all "
					<< "file patterns have been processed." << endl << osunlock;
			sendServerDone(ss); // informs worker to exit, server side system call returns, surrounding thread exits 
		}
	} else if (message == kJobSucceeded) {
		string pattern = trim(payload);
		markFilePatternAsProcessed(clientIPAddress, pattern);
	} else if (message == kJobFailed) {
		string pattern = trim(payload);
		rescheduleFilePattern(clientIPAddress, pattern);
	} else if (message == kJobInfo) {
		string workerInfo = trim(payload);
		if (verbose) cout << oslock << workerInfo << endl << osunlock;
	} else {
		if (verbose) 
			cout << oslock << "Ignoring unrecognized message type of \"" 
				<< message << "\"." << endl << osunlock;
	}

	if (verbose) 
		cout << oslock << "Conversation with " << ipAddressMap[clientIPAddress] 
			<< " complete." << endl << osunlock;
}

/**
 * Method: surfaceNextFilePattern
 * ------------------------------
 * Surfaces the next input file pattern that should be processed by some worker eager to do some work.
 * The method returns false if there are no more files to be processed, but it returns
 * true if there was at least one (and it surfaces the name of that input file pattern by
 * populated the space referenced by pattern with it).
 */
bool MapReduceServer::surfaceNextFilePattern(std::string& pattern) throw() {
	if (unprocessed.empty()) return false;
	pattern = unprocessed.front();
	unprocessed.pop_front();
	inflight.insert(pattern);
	return true;
}

/**
 * Method: markFilePatternAsProcessed
 * ----------------------------------
 * Acknowledges that the worker spinning on the remote machine (as identified by
 * the supplied IP address) managed to fully process the supplied file pattern.
 */
void MapReduceServer::markFilePatternAsProcessed(const std::string& clientIPAddress, 
		const std::string& pattern) throw() {
	inflight.erase(pattern);
	if (verbose) 
		cout << oslock << "File pattern \"" << pattern << "\" " 
			<< "fully processed by worker at " << ipAddressMap[clientIPAddress] << "." 
			<< endl << osunlock;
}

/**
 * Method: rescheduleFilePattern
 * -----------------------------
 * Acknowledges that a worker spinning on some remote machine (identified via its IP address) failed 
 * to fully process the supplied input file pattern (probably because a mapper or reducer executable
 * returned a non-zero exit status), and rescheduled to same pattern to be processed
 * later on.  Rather than reissuing the job for the same pattern to the same machine right away,
 * we instead append it to the end of the queue of unprocessed patterns and schedule
 * it later, when it bubbles to the front.
 */
void MapReduceServer::rescheduleFilePattern(const std::string& clientIPAddress, 
		const std::string& pattern) throw() {
	inflight.erase(pattern);
	unprocessed.push_back(pattern);
	if (verbose) 
		cout << oslock << "File pattern \"" << pattern << "\" " 
			<< "not properly processed by worker at " << ipAddressMap[clientIPAddress] << ", so rescheduling." 
			<< endl << osunlock;
}

void MapReduceServer::spawnReducers() throw() {
	ThreadPool pool(kNumWorkerThreads);
	// stageFiles(inputPath, unprocessed);
	unprocessed.clear();
	for(unsigned int i = 0; i < numMappers * numReducers; i++) {
		unprocessed.push_back(intermediatePath + "/*." + numberToString(i) + ".mapped");
	}
	vector<string> reducerNodes(nodes);

	for(size_t w = 0; w < kNumWorkerThreads; w++) {
		random_shuffle(reducerNodes.begin(), reducerNodes.end());
		const string& reducerNode = reducerNodes[0];
		string command = buildReducerCommand(reducerNode, reducerExecutable, outputPath);
		// cout << oslock << "==== command: " << command << endl << osunlock;
		pool.schedule([this, reducerNode, command]{spawnWorker(reducerNode, command);});
	}
	pool.wait();

	if (verbose) cout << "Reducing of all input chunks now complete." << endl;
	// cout << oslock << "==== checkpoint" << endl << osunlock;
	dumpFileHashes(outputPath);
}

/**
 * Method: spawnMappers
 * --------------------
 * Launches this->numMappers workers, one per remote machine, across this->numMappers
 * remote hosts.  
 * 
 * (The initial code only spawns one worker, regardless of the
 * value of this->numMappers, but Task 2 of your Assignment 8 handout outlines
 * what changes need to be made to upgrade it to spawn the full set of mapper workers
 * so that map jobs can be done in parallel across multiple machines instead of
 * in sequence on just one).
 */
void MapReduceServer::spawnMappers() throw() {
	ThreadPool pool(kNumWorkerThreads);
	stageFiles(inputPath, unprocessed);
	vector<string> mapperNodes(nodes);

	for(size_t w = 0; w < kNumWorkerThreads; w++) {
		random_shuffle(mapperNodes.begin(), mapperNodes.end());
		const string& mapperNode = mapperNodes[0];
		string command = buildMapperCommand(mapperNode, mapperExecutable, intermediatePath);
		pool.schedule([this, mapperNode, command]{spawnWorker(mapperNode, command);});
	}
	pool.wait();

	if (verbose) cout << "Mapping of all input chunks now complete." << endl;
	if (!mapOnly) return;
	dumpFileHashes(intermediatePath);
}

string MapReduceServer::buildReducerCommand(const string& remoteHost, 
		const string& executable, 
		const string& outputPath) throw() {
	ostringstream oss;
	string pathToReducer = reducer[0] == '/' ? reducer : cwd + "/" + reducer;
	oss << "ssh -o ConnectTimeout=15 " << user << "@" << remoteHost
		<< " '" << pathToReducer
		<< " " << host
		<< " " << serverPort
		<< " " << cwd
		<< " " << executable
		<< " " << intermediatePath
		<< " " << outputPath 
		<< "'";
	return oss.str();
}

/**
 * Method: buildMapperCommand
 * --------------------------
 * Constructs the command needed to invoke a mapper on a remote machine using
 * the system (see "man system") function.  See spawnWorker below for even more information.
 */
string MapReduceServer::buildMapperCommand(const string& remoteHost, 
		const string& executable, 
		const string& outputPath) throw() {
	ostringstream oss;
	string pathToMapper = mapper[0] == '/' ? mapper : cwd + "/" + mapper;
	oss << "ssh -o ConnectTimeout=15 " << user << "@" << remoteHost
		<< " '" << pathToMapper
		<< " " << host
		<< " " << serverPort
		<< " " << cwd
		<< " " << executable
		<< " " << outputPath 
		<< " " << numMappers * numReducers
		<< "'";
	return oss.str();
}

/**
 * Method: spawnWorker
 * -------------------
 * Assumes the incoming command is of the form 
 * 
 *  'ssh poohbear@myth8.stanford.edu '/usr/class/cs110/<other-directories>/<worker> <arg1> ... <argn>',
 * 
 * which is precisely the command that can be used to launch a worker--either a mapper or a reducer--
 * on a remote machine (in this example, myth8).
 * 
 * The remote command is invoked via the system function, which blocked until the remotely
 * invoked command--in this case, a command to run a remote worker--terminates.
 */
void MapReduceServer::spawnWorker(const string& node, const string& command) throw() {
	if (verbose)
		cout << oslock << "Spawning worker on " << node << " with ssh command: " << endl 
			<< "\t\"" << command << "\"" << endl << osunlock;
	int status = system(command.c_str());
	if (status != -1) status = WEXITSTATUS(status);
	if (verbose) 
		cout << oslock << "Remote ssh command on " << node << " executed and returned a status " 
			<< status << "." << endl << osunlock;
}

/**
 * Method: dumpFileHashes
 * ----------------------
 * Lists the file hashes of all of the files in specified directory.
 */
void MapReduceServer::dumpFileHashes(const std::string& dir) throw() {
	list<string> files;
	stageFiles(dir, files);
	files.sort();
	for (const string& file: files) dumpFileHash(file);
}

/**
 * Method: dumpFileHash
 * --------------------
 * Prints the name of the file and a hash summary of its contents (as per
 * the recipe defined within the hash<ifstream>.
 */

void MapReduceServer::dumpFileHash(const string& file) throw() {
	ifstream infile(file);
	cout << file << " hashes to " << hash<ifstream>()(infile) << endl;
}

/**
 * Destructor: ~MapReduceServer
 * ----------------------------
 * Instructs the server to shut itself down, waits for that
 * to happen, and then destroys all of the directly embedded objects
 * that contribute to the MapReduce system.
 */
MapReduceServer::~MapReduceServer() throw() {
	// assumption is that thread pool is empty and that server is asleep in an accept call
	bringDownServer();
	serverThread.join();
	if (verbose) cout << "Server has shut down." << endl;
}

/**
 * Method: bringDownServer
 * -----------------------
 * Pings the serverThread, which is assumed to be blocked within an accept call,
 * so that it wakes up and shuts itself down.  If you look at the implementation of
 * orchestrateWorkers above, you'll see the first thing it does is check to see
 * it the server is no longer running, and if so, closes down all of its server-side
 * resources and exits.
 */
void MapReduceServer::bringDownServer() throw() {
	// assumption is that thread pool is empty and that server is asleep in an accept call
	serverIsRunning = false;
	shutdown(serverSocket, SHUT_RD);
}
