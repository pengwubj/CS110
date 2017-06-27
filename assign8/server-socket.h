/**
 * File: server-socket.h
 * ---------------------
 * Provides a single function that sets up
 * a server socket, binding it to any of the
 * IP addresses associated with the host machine
 * on the specified port.
 */
#pragma once

/**
 * Constant: kServerSocketFailure
 * ------------------------------
 * Constant returned by createServerSocket if the
 * server socket couldn't be created or otherwise
 * bound to listen to the specified port.
 */
const int kServerSocketFailure = -1;

/**
 * Constant: kDefaultBacklog
 * -------------------------
 * Defines the default number of outstanding connections a server
 * socket is allowed to queue up before it claims to be overwhelmed
 * and just ignores connection requests.
 */
const int kDefaultBacklog = 32;

/**
 * Function: createServerSocket
 * ----------------------------
 * createServerSocket creates a server socket to
 * listen for all client connections on the given
 * port with the specified backlog.  The function
 * returns a valid server socket descriptor, or
 * kServerSocketFailure if the function call fails
 * for any reason whatsoever.
 */
int createServerSocket(unsigned short port, int backlog = kDefaultBacklog);
