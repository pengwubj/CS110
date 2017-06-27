/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
using namespace std;
int Pipe(int pipefd[2]) {
	int p = pipe(pipefd);
	if(p == -1) {
		if(errno == EFAULT) throw("pipefd is not valid");
		if(errno == EMFILE) throw("Too many file descriptors are in use by the process");
		if(errno == ENFILE) throw("The system limit on the total number of open files has been reached");
		throw("unknown error");
	}
	return p;
}

int Dup2(int oldfd, int newfd) {
	int d = dup2(oldfd, newfd);
	if(d == -1) {
		if(errno == EBADF) throw("oldfd isn't an open fd, or newfd is out of the range for fd");
		if(errno == EBUSY) throw("race condition occured");
		if(errno == EINTR) throw("The dup2() call was interrupted by a siganl");
		if(errno == EMFILE) throw("Too many file descriptors are in use by the process");
		throw("unknown error");
	}
	return d;
}

int Close(int fd) {
	int c = close(fd);
	if(c == -1) {
		if(errno == EBADF) throw("fd isn't an valid open fd");
		if(errno == EINTR) throw("The close() call was interrupted by a signal");
		if(errno == EIO) throw("An I/O error occured");
		throw("unknown error");
	}
	return c;
}

void Execvp(const char* file, char* const argv[]) {
	int	e = execvp(file, argv);
	if(e == -1) {
		throw("execvp error");
	}
}

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
	int supply_fds[2];
	int ingest_fds[2];
	Pipe(supply_fds);
	Pipe(ingest_fds);
	struct subprocess_t sp = {fork(), kNotInUse, kNotInUse};
	if(sp.pid == 0) {
		if(supplyChildInput) {
			Close(supply_fds[1]);
			Dup2(supply_fds[0], STDIN_FILENO);
			Close(supply_fds[0]);
		} else {
			Close(supply_fds[1]);
			Close(supply_fds[0]);
		}
		if(ingestChildOutput) {
			Close(ingest_fds[0]);
			Dup2(ingest_fds[1], STDOUT_FILENO);
			Close(ingest_fds[1]);
		} else {
			Close(ingest_fds[0]);
			Close(ingest_fds[1]);
		} 
		Execvp(argv[0], argv);
	} else {
		Close(supply_fds[0]);
		Close(ingest_fds[1]);
		if(supplyChildInput) {
			sp.supplyfd = supply_fds[1];
		}
		if(ingestChildOutput) {
			sp.ingestfd = ingest_fds[0];
		}
	}
	return sp;
}
