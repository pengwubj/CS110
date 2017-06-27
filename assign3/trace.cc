/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about ever single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system calls return value
 */
#include <climits>
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include "string-utils.h"
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
using namespace std;

static std::map<int, std::string> systemCallNumbers;
static std::map<std::string, int> systemCallNames;
static std::map<std::string, systemCallSignature> systemCallSignatures;
static std::map<int, int> registerNumbers = {{0, RDI}, {1, RSI}, {2, RDX}, {3, R10}, {4, R8}, {5, R9}};
static std::map<int, std::string> errorConstants;

static string readString(pid_t pid, unsigned long addr) { // addr is a char * read from an argument register  
	string str; // start out empty
	size_t numBytesRead = 0;
	bool isEndOfStr = false;
	while (!isEndOfStr) {
		long ret = ptrace(PTRACE_PEEKDATA, pid, addr + numBytesRead);
		const char* word = reinterpret_cast<const char *> (&ret);
		for(unsigned int i = 0; i < sizeof(long); i++) {
			char ch = *(word + i);
			if(ch == '\0') {
				isEndOfStr = true;
				break;
			}
			str.push_back(ch);
		}
		numBytesRead += sizeof(long); 
		// the following conditiong is used for debug
		if(numBytesRead > 100) {
			break;
		}
	}
	return str;
}

void readRegister(pid_t pid, vector<enum scParamType> sysCallSig, map<int, int> registerNumbers) {
	cout << "(";
	if(sysCallSig.size() == 0) cout << "<signature-information-missing>";
	for(unsigned int i = 0; i < sysCallSig.size(); i++) {
		enum scParamType sc = sysCallSig[i];
		switch(sc) {
			case SYSCALL_INTEGER: 
				{
					cout << ptrace(PTRACE_PEEKUSER, pid, registerNumbers[i] * sizeof(long));
					break;
				}
			case SYSCALL_STRING: 
				{
					unsigned long addr = ptrace(PTRACE_PEEKUSER, pid, registerNumbers[i] * sizeof(long));
					string str = readString(pid, addr);
					cout << "\"" << str << "\"";
					break;
				}
			case SYSCALL_POINTER: 
				{
					long addr = ptrace(PTRACE_PEEKUSER, pid, registerNumbers[i] * sizeof(long));
					if(addr == 0) {
						cout << "NULL";
					} else {
						cout << "0x" << std::hex << addr << std::dec;
					}
					break;
				}
			case SYSCALL_UNKNOWN_TYPE: 
				throw TraceException("unknown type");
			default:
				throw TraceException("unknown error");
		}
		if(i != sysCallSig.size() - 1) {
			cout << ", ";
		}
	}	
	cout << ") ";
}

void compileMaps(bool rebuild) {
	compileSystemCallData(systemCallNumbers, systemCallNames, systemCallSignatures, rebuild);
	try {
		compileSystemCallErrorStrings(errorConstants);
	} catch (const MissingFileException& me) {
		cerr << "More details here: " << me.what() << endl;
		exit(1);
	} catch (...) { // ... here means catch everything else
		cerr << "Unknown internal error." << endl;
		exit(2);
	}
}

void enterSysCall(pid_t pid, bool simple, int& exitNumber) {
	long sysCallNum = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));
	if(simple) {
		cout << "syscall(" << sysCallNum << ") ";
	} else {
		string sysCallName = systemCallNumbers[sysCallNum];
		cout << sysCallName;
		vector<scParamType> sysCallSig = systemCallSignatures[sysCallName];
		readRegister(pid, sysCallSig, registerNumbers);
		if(sysCallNum == systemCallNames["exit_group"]) {
			exitNumber = ptrace(PTRACE_PEEKUSER, pid, RDI * sizeof(long));
		}
	}
}

void leaveSysCall(pid_t pid, bool simple) {
	long returnValue = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));
	if(simple) {
		cout << "= " << returnValue; 
	} else {
		cout << "= ";
		if(returnValue < 0) {
			cout << -1;
			cout << " " << errorConstants[abs(returnValue)];
			cout << " (" << strerror(abs(returnValue)) << ")";
		} else {
			if(returnValue <= INT_MAX){
				cout << returnValue;
			} else {
				cout << "0x" << std::hex << returnValue << std::dec;
			}
		}
	}
	cout << endl;
}

void detectSysCall(pid_t pid, bool simple) {
	int exitNumber = 0;
	while(true) {
		// when it enters a system call
		int status1;
		waitpid(pid, &status1, 0); 
		if(WIFEXITED(status1) || WIFSIGNALED(status1)) {
			break;
		}
		if(WIFSTOPPED(status1)) {
			if(WSTOPSIG(status1) == (SIGTRAP|0x80)) {
				enterSysCall(pid, simple, exitNumber);
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				// when it leaves a system call
				int status2;
				waitpid(pid, &status2, WUNTRACED); 
				ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
				if(WIFEXITED(status2) || WIFSIGNALED(status2)) {
					break;
				}
				if(WIFSTOPPED(status2)) {
					if(WSTOPSIG(status2) == (SIGTRAP|0x80)) {
						leaveSysCall(pid, simple);
					} 
					ptrace(PTRACE_SYSCALL, pid, 0, 0);
				}
			} 
			ptrace(PTRACE_SYSCALL, pid, 0, 0);
		}
	}
	// end of output
	cout << "= <no return>" << endl;
	cout << "Program exited normally with status " << exitNumber << endl;
}

int main(int argc, char *argv[]) {
	// pre process
	bool simple = false, rebuild = false;
	int numFlags = processCommandLineFlags(simple, rebuild, argv);
	if (argc - numFlags == 1) {
		cout << "Nothing to trace... exiting." << endl;
		return 0;
	}
	compileMaps(rebuild);
	argv += (1 + numFlags);

	// start child process
	pid_t pid = fork();
	if(pid == 0) {
		ptrace(PTRACE_TRACEME);
		raise(SIGTRAP);
		execvp(argv[0], argv);
	}

	// skip the tgkill system call
	waitpid(pid, NULL, 0);
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
	ptrace(PTRACE_SYSCALL, pid, 0, 0);

	// detect system call
	detectSysCall(pid, simple);

	return 0;
}
