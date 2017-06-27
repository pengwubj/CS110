/**
 * File: http-request.cc
 * ---------------------
 * Presents the implementation of the HTTPRequest class and
 * its friend functions as exported by request.h.
 */

#include <sstream>
#include "request.h"
#include "string-utils.h"
using namespace std;

static const string kWhiteSpaceDelimiters = " \r\n\t";
static const string kProtocolPrefix = "http://";
static const unsigned short kDefaultPort = 80;

bool HTTPRequest::containsLoop(const std::pair<int, std::string>& connection) {
	if(!requestHeader.containsName("x-forwarded-for")) return false; // no previous client/proxy
	string clientIPAddressList = requestHeader.getValueAsString("x-forwarded-for");
	size_t found = clientIPAddressList.find(connection.second);
	if(found != std::string::npos) {
		sockbuf sb504(connection.first);
		iosockstream ss504(&sb504);
		HTTPResponse response504;
		response504.setResponseCode(504);
		response504.setProtocol("HTTP/1.0");
		response504.setPayload("Loop Detected!");
		ss504 << response504 << flush;
		return true;
	} else {
		return false;
	}
}

void HTTPRequest::ingestRequestLine(istream& instream) throw (HTTPBadRequestException) {
  getline(instream, requestLine);
  if (instream.fail()) {
    throw HTTPBadRequestException("First line of request could not be read.");
  }

  requestLine = trim(requestLine);
  istringstream iss(requestLine);
  iss >> method >> url >> protocol;
  server = url;
  size_t pos = server.find(kProtocolPrefix);
  server.erase(0, kProtocolPrefix.size());
  pos = server.find('/');
  if (pos == string::npos) {
    // url came in as something like http://www.google.com, without the trailing /
    // in that case, least server as is (it'd be www.google.com), and manually set
    // path to be "/"
    path = "/";
  } else {
    path = server.substr(pos);
    server.erase(pos);
  }
  port = kDefaultPort;
  pos = server.find(':');
  if (pos == string::npos) return;
  port = strtol(server.c_str() + pos + 1, NULL, 0); // assume port is well-formed
  server.erase(pos);
}

void HTTPRequest::ingestHeader(istream& instream, const string& clientIPAddress) {
  requestHeader.ingestHeader(instream);
}

void HTTPRequest::addEntity(const string& clientIPAddress) {
	requestHeader.addHeader("x-forwarded-proto", "http");
	if(requestHeader.containsName("x-forwarded-for")) {
		string str = requestHeader.getValueAsString("x-forwarded-for");
		str += "," + clientIPAddress;
		requestHeader.addHeader("x-forwarded-for", str);
	} else {
		requestHeader.addHeader("x-forwarded-for", clientIPAddress);
	}
}

bool HTTPRequest::containsName(const string& name) const {
  return requestHeader.containsName(name);
}

void HTTPRequest::ingestPayload(istream& instream) {
  if (getMethod() != "POST") return;
  payload.ingestPayload(requestHeader, instream);
}

std::string HTTPRequest::recover_request() {
	ostringstream oss;
	oss << requestLine << "\r\n";
	oss << requestHeader;
	oss << "\r\n";
	oss << payload;
	return oss.str();
}

ostream& operator<<(ostream& os, const HTTPRequest& rh) {
  const string& path = rh.path;
  os << rh.method << " " << path << " " << rh.protocol << "\r\n";
  os << rh.requestHeader;
  os << "\r\n"; // blank line not printed by request header
  os << rh.payload;
  return os;
}
