/**
 * File: request-handler.h
 * -----------------------
 * Defines the HTTPRequestHandler class, which fully proxies and
 * services a single client request.  
 */

#ifndef _request_handler_
#define _request_handler_

#include <utility>
#include <string>
#include "blacklist.h"
#include "cache.h"

class HTTPRequestHandler {
	public:
		HTTPRequestHandler(); 
		void serviceRequest(const std::pair<int, std::string>& connection) throw();
		void clearCache();
		void setCacheMaxAge(long maxAge);
		void setProxy(const std::string& proxyServer, unsigned short proxyPortNumber);
	private:
		HTTPBlacklist blacklist;
		HTTPCache cache;
		std::mutex cacheLock[997];

		bool isUsingProxy;
		std::string proxyServer;
		unsigned short int proxyPortNumber;

		size_t getHashCode(const HTTPRequest& request) const;
		bool checkBlacklist(const HTTPRequest& request, const std::pair<int, std::string>& connection);
		void checkCache(const HTTPRequest& request, const std::pair<int, std::string>& connection);
		void shouldCache(const HTTPRequest& request, const HTTPResponse& response); 
};

#endif
