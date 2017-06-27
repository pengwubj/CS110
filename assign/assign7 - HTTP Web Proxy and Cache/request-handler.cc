/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "response.h"
#include "request.h"
#include <socket++/sockstream.h> // for sockbuf, iosockstream
#include "ostreamlock.h"
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include "client-socket.h"
using namespace std;

HTTPRequestHandler::HTTPRequestHandler() {
	blacklist.addToBlacklist("blocked-domains.txt");
	isUsingProxy = false;
}

void HTTPRequestHandler::setProxy(const std::string& proxyServer, unsigned short proxyPortNumber) {
	this->isUsingProxy = true;
	this->proxyServer = proxyServer;
	this->proxyPortNumber = proxyPortNumber;
}

size_t HTTPRequestHandler::getHashCode(const HTTPRequest& request) const {
	return cache.getHashCode(request);
}

void HTTPRequestHandler::serviceRequest(const std::pair<int, std::string>& connection) throw() {
	// 1. intercept client's HTTP request
	HTTPRequest request;
	sockbuf sb1(connection.first);
	istream is1(&sb1);
	try{
		request.ingestRequestLine(is1);
	} catch (const HTTPBadRequestException& hpe) {
		cerr << oslock << "HTTP Bad Request: " << hpe.what() << endl << osunlock;
		return;
	}
	request.ingestHeader(is1, connection.second);
	// 1.25 check loop
	if(request.containsLoop(connection)) return;
	request.addEntity(connection.second);
	request.ingestPayload(is1); 
	// 1.5 check blacklist and cache
	if(checkBlacklist(request, connection)) return;
	size_t hashCode = getHashCode(request);	// lock
	cacheLock[hashCode].lock();
	try{
		checkCache(request, connection);
	} catch (const HTTPCacheAccessException& hcae) {
		cerr << oslock << "HTTP Cache Access Execption: " << hcae.what() << endl << osunlock;
		return;
	}
	// 1.75 check whether to use another proxy
	int clientSocket = -1;
	if(isUsingProxy) {
		// 2. send request to anther proxy
		std::string forward_request = request.recover_request();
		clientSocket  = createClientSocket(proxyServer, proxyPortNumber);
		if (clientSocket == -1) {
			cerr << "Count not connect to host named \"" << proxyServer << "\"" << endl;
			return;
		}
		sockbuf sbx(clientSocket);
		iosockstream  ssx(&sbx);
		ssx << forward_request << flush;
		// 3. receive response
		sockbuf sb3(clientSocket);
		istream is3(&sb3);
		HTTPResponse response;
		response.ingestResponseHeader(is3);
		response.ingestPayload(is3);
		// 3.5 should cache?
		try{
			shouldCache(request, response);
		} catch (const HTTPCacheAccessException& hcae) {
			cerr << oslock << "HTTP Cache Access Execption: " << hcae.what() << endl << osunlock;
			return;
		}
		// 4. send response back to client
		sockbuf sb4(connection.first);
		iosockstream ss4(&sb4);
		ss4 << response << flush;
	} else {
		// 2. send request to origin server
		clientSocket = createClientSocket(request.getServer(), request.getPort());
		if (clientSocket == -1) {
			cerr << "Count not connect to host named \"" << request.getServer() << "\"" << endl;
			return;
		}
		sockbuf sb2(clientSocket);
		iosockstream ss2(&sb2);
		ss2 << request << flush;
		// 3. receive response
		sockbuf sb3(clientSocket);
		istream is3(&sb3);
		HTTPResponse response;
		response.ingestResponseHeader(is3);
		response.ingestPayload(is3);
		// 3.5 should cache?
		try{
			shouldCache(request, response);
		} catch (const HTTPCacheAccessException& hcae) {
			cerr << oslock << "HTTP Cache Access Execption: " << hcae.what() << endl << osunlock;
			return;
		}
		// 4. send response back to client
		sockbuf sb4(connection.first);
		iosockstream ss4(&sb4);
		ss4 << response << flush;
	}
	cacheLock[hashCode].unlock();
}


bool HTTPRequestHandler::checkBlacklist(const HTTPRequest& request, const std::pair<int, std::string>& connection) {
	// check blacklist
	if(!blacklist.serverIsAllowed(request.getServer())) {
		sockbuf sb403(connection.first);
		iosockstream ss403(&sb403);
		HTTPResponse response403;
		response403.setResponseCode(403);
		response403.setProtocol("HTTP/1.0");
		response403.setPayload("Forbidden Content");
		ss403 << response403 << flush;
		return true;
	} else {
		return false;
	}
}

void HTTPRequestHandler::checkCache(const HTTPRequest& request, const pair<int, string>& connection) {
	// check cache
	HTTPResponse responseCache;
	bool containsCacheEntry = cache.containsCacheEntry(request, responseCache);
	if(containsCacheEntry) {
		sockbuf sbCache(connection.first);
		iosockstream ssCache(&sbCache);
		ssCache << responseCache << flush;
	}
}

void HTTPRequestHandler::shouldCache(const HTTPRequest& request, const HTTPResponse& response) {
	// should cache?
	if(cache.shouldCache(request, response)) {
		cache.cacheEntry(request, response);
	}	
}

// the following two methods needs to be completed 
// once you incorporate your HTTPCache into your HTTPRequestHandler
void HTTPRequestHandler::clearCache() {
	cache.clear();
}

void HTTPRequestHandler::setCacheMaxAge(long maxAge) {
	cache.setMaxAge(maxAge);
}
