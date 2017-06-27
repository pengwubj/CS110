/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include <utility>
#include <string>

using namespace std;
HTTPProxyScheduler::HTTPProxyScheduler() {isUsingProxy = false;}
void HTTPProxyScheduler::scheduleRequest(int clientfd, const string& clientIPAddress) throw () {
	if(isUsingProxy) {
		requestHandler.setProxy(getProxyServer(), getProxyPortNumber());
	}
	requestHandler.serviceRequest(make_pair(clientfd, clientIPAddress));
}

std::string HTTPProxyScheduler::getProxyServer() {return proxyServer;}
unsigned short HTTPProxyScheduler::getProxyPortNumber() {return proxyPortNumber;}

void HTTPProxyScheduler::setProxy(const std::string& proxyServer, unsigned short proxyPortNumber) {
	isUsingProxy = true;
	this->proxyServer = proxyServer;
	this->proxyPortNumber = proxyPortNumber;
}
