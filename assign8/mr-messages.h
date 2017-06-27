/**
 * File: mapreduce-messages.h
 * --------------------------
 * Provides common messsage formats to be passed between 
 * the mapreduce-server and mapreduce-workers.  In general,
 * All functions named send* expect an iosockstream (layered
 * over the network connection to another host) to be supplied
 * as the first argument.  The understanding is that messages
 * published to that iosockstream are sent to the other side and
 * ingested in a classic request/response manner.
 */

#pragma once
#include <iostream>
#include <string>

enum MRMessage {
  kWorkerReady,
  kJobStart,
  kJobInfo,
  kJobSucceeded,
  kJobFailed,
  kServerDone,
  kUnknown
};

/**
 * Function: sendWorkerReady
 * -------------------------
 * When a worker is free and can process another input chunk (whether
 * it's for a mapper or a reducer job), it notifies the server by
 * sending it this message.
 */
void sendWorkerReady(std::ostream& outstream);

/**
 * Function: sendJobStart
 * ----------------------
 * When a server hears that a worker is idle and is willing to do more work,
 * it messages that worker by sending it this message (and identifying the
 * absolute path to the file pattern that it should process).
 */
void sendJobStart(std::ostream& outstream, const std::string& filePattern);

/**
 * Function: sendJobInfoMessage
 * ----------------------------
 * When the worker makes enough progress and/or notices something interesting,
 * it messages the server by sending it this message (along with a free-form,
 * single-line string as a payload) so that the server, which is the centralized
 * location where all info messages are published, can print it.
 *
 * This particular function doesn't need to contribute to the final solution, but
 * it may be helpful to send per-worker messages to the server so the
 * server can log messages about worker activity.
 */
void sendJobInfo(std::ostream& outstream, const std::string& info);

/**
 * Function: sendJobSucceeded
 * --------------------------
 * When a worker invokes an executable (be it a mapper or a reducer) on 
 * a particular pattern, and that executable comes back with a zero exit code
 * (which expresses success!), the worker notifies the server that the identified
 * chunk has been fully processed.
 */
void sendJobSucceeded(std::ostream& outstream, const std::string& filePattern);

/**
 * Function: sendJobFailed
 * -----------------------
 * When a worker invokes a mapper or reducer executable on a particular
 * pattern, and that executable returns a nonzero error code, the worker
 * notifies the server of the failure by sending it this message.  This way
 * the server can reschedule the pattern to be processed later on).
 */
void sendJobFailed(std::ostream& outstream, const std::string& filePattern);

/**
 * Function: sendServerDone
 * ------------------------
 * When the server has gotten confirmation that all chunks have been processed,
 * it sends this message to workers when that say they're ready for another job.
 * By sending this message from the server to a worker, the worker knows that
 * it can shut itself down.
 */
void sendServerDone(std::ostream& outstream);

/**
 * Function: receivedMessage
 * -------------------------
 * This is a generic routine that the server and the workers use
 * to surface a message that comes in over a network connection.  Some
 * messages are accompanied by a payload (which, in practice, is always
 * the name of a chunk file), and that chunk file is surfaced via the
 * third argument.  The return value is true if and only if the
 * message is accompanied by a payload.
 */
bool receiveMessage(std::istream& instream, MRMessage& message, std::string& payload);
