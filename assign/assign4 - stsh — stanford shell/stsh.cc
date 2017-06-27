/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-process.h"
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sys/wait.h>
#include <assert.h>
#include <iomanip>
using namespace std;

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it

static void handle_fg(const pipeline& pipeline) {
	sigset_t additions, existingmask;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigaddset(&additions, SIGINT);
	sigaddset(&additions, SIGTSTP);
	sigaddset(&additions, SIGCONT);
	sigprocmask(SIG_BLOCK, &additions, &existingmask);

	char* token = pipeline.commands[0].tokens[0];
	if(token == NULL) throw STSHException("Usage: fg <jobid>.");
	if(strcmp(token, "0") == 0) throw STSHException("fg 0: No such job.");	
	int jobId = atoi(token);
	if(jobId == 0) throw STSHException("Usage: fg <jobid>.");
	if(!joblist.containsJob(jobId)) throw STSHException("fg " + to_string(jobId) + ": No such job.");	
	STSHJob& job = joblist.getJob(jobId);
	// job.setState(kForeground);
	// pid_t pgid = job.getGroupID();
	// kill(-pgid, SIGCONT);
	vector<STSHProcess>& processes = job.getProcesses();
	for(unsigned int i = 0; i < processes.size(); i++) {
		STSHProcess &process = processes[i];
		int err = kill(process.getID(), SIGCONT);
		if(err == 0) {
			job.setState(kForeground);
		}
	}

	while(joblist.hasForegroundJob()) {
		sigsuspend(&existingmask);
	}
	sigprocmask(SIG_UNBLOCK, &additions, NULL);
}

static void handle_bg(const pipeline& p) {
	char* one = p.commands[0].tokens[0];
	if(one == nullptr) throw STSHException("Usage: bg <jobid>.");
	char* two = p.commands[0].tokens[1];
	int iOne = atoi(one);
	if(two == nullptr) { 	// one parameter: only pid
		if(!joblist.containsProcess(iOne)) throw STSHException("No process with pid " + to_string(iOne));
		kill(iOne, SIGCONT);
	} else {				// two parameters: job id and pid
		int iTwo = atoi(two);
		if(!joblist.containsJob(iOne)) throw STSHException("No job with id of " + to_string(iOne));
		STSHJob& job = joblist.getJob(iOne);
		vector<STSHProcess>& processes = job.getProcesses();
		STSHProcess& process = processes[iTwo];
		if(!job.containsProcess(process.getID())) throw STSHException("Job " + to_string(iOne) + " doesn't have a pid at index " + to_string(iTwo));
		kill(process.getID(), SIGCONT);
	}
}

static void handle_slay(const pipeline& p) {
	char* one = p.commands[0].tokens[0];
	if(one == nullptr) throw STSHException("Usage: slay <jobid> <index> | <pid>.");
	char* two = p.commands[0].tokens[1];
	int iOne = atoi(one);
	if(two == nullptr) { 	// one parameter: only pid
		if(!joblist.containsProcess(iOne)) throw STSHException("No process with pid " + to_string(iOne));
		kill(iOne, SIGINT);
	} else {				// two parameters: job id and pid
		int iTwo = atoi(two);
		if(!joblist.containsJob(iOne)) throw STSHException("No job with id of " + to_string(iOne));
		STSHJob& job = joblist.getJob(iOne);
		vector<STSHProcess>& processes = job.getProcesses();
		STSHProcess& process = processes[iTwo];
		if(!job.containsProcess(process.getID())) throw STSHException("Job " + to_string(iOne) + " doesn't have a pid at index " + to_string(iTwo));
		kill(process.getID(), SIGINT);
	}
}

static void handle_halt(const pipeline& p) {
	char* one = p.commands[0].tokens[0];
	if(one == nullptr) throw STSHException("Usage: halt <jobid> <index> | <pid>.");
	char* two = p.commands[0].tokens[1];
	int iOne = atoi(one);
	if(two == nullptr) { 	// one parameter: only pid
		if(!joblist.containsProcess(iOne)) throw STSHException("No process with pid " + to_string(iOne));
		kill(iOne, SIGTSTP);
	} else {				// two parameters: job id and pid
		int iTwo = atoi(two);
		if(!joblist.containsJob(iOne)) throw STSHException("No job with id of " + to_string(iOne));
		STSHJob& job = joblist.getJob(iOne);
		vector<STSHProcess>& processes = job.getProcesses();
		STSHProcess& process = processes[iTwo];
		if(!job.containsProcess(process.getID())) throw STSHException("Job " + to_string(iOne) + " doesn't have a pid at index " + to_string(iTwo));
		kill(process.getID(), SIGTSTP);
	}
}

static void handle_cont(const pipeline& p) {
	char* one = p.commands[0].tokens[0];
	if(one == nullptr) throw STSHException("Usage: cont <jobid> <index> | <pid>.");
	char* two = p.commands[0].tokens[1];
	int iOne = atoi(one);
	if(two == nullptr) { 	// one parameter: only pid
		if(!joblist.containsProcess(iOne)) throw STSHException("No process with pid " + to_string(iOne));
		kill(iOne, SIGCONT);
	} else {				// two parameters: job id and pid
		int iTwo = atoi(two);
		if(!joblist.containsJob(iOne)) throw STSHException("No job with id of " + to_string(iOne));
		STSHJob& job = joblist.getJob(iOne);
		vector<STSHProcess>& processes = job.getProcesses();
		STSHProcess& process = processes[iTwo];
		if(!job.containsProcess(process.getID())) throw STSHException("Job " + to_string(iOne) + " doesn't have a pid at index " + to_string(iTwo));
		kill(process.getID(), SIGCONT);
	}
}
/**
 * Function: handleBuiltin
 * -----------------------
 * Examines the leading command of the provided pipeline to see if
 * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 * returns true if the command is a builtin, and false otherwise.
 */
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins)/sizeof(kSupportedBuiltins[0]);
static bool handleBuiltin(const pipeline& pipeline) {
	const string& command = pipeline.commands[0].command;
	auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
	if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
	size_t index = iter - kSupportedBuiltins;

	switch (index) {
		case 0:
		case 1: exit(0);
		case 2: 
				handle_fg(pipeline); break;
		case 3:
				handle_bg(pipeline); break;
		case 4:
				handle_slay(pipeline); break;
		case 5:
				handle_halt(pipeline); break;
		case 6:
				handle_cont(pipeline); break;
		case 7: 
				cout << joblist; break;
		default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
	}

	return true;
}


static void sigchld_handler(int sig) {
	while(true) {
		int status;
		// pid_t pid = waitpid(-1, &status, WNOHANG|WUNTRACED);
		pid_t pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED);

		if(pid <= 0) break;

		if(WIFEXITED(status)) {
			STSHJob& job = joblist.getJobWithProcess(pid); 
			assert(job.containsProcess(pid));
			STSHProcess& process = job.getProcess(pid);
			process.setState(kTerminated);
			joblist.synchronize(job);
		}
		if(WIFSIGNALED(status)) {
			STSHJob& job = joblist.getJobWithProcess(pid); 
			assert(job.containsProcess(pid));
			STSHProcess& process = job.getProcess(pid);
			process.setState(kTerminated);
			joblist.synchronize(job);
		}	
		if(WIFSTOPPED(status)) {
			STSHJob& job = joblist.getJobWithProcess(pid); 
			assert(job.containsProcess(pid));
			STSHProcess& process = job.getProcess(pid);
			process.setState(kStopped);
			joblist.synchronize(job);
		}	
		if(WIFCONTINUED(status)) {
			STSHJob& job = joblist.getJobWithProcess(pid); 
			assert(job.containsProcess(pid));
			STSHProcess& process = job.getProcess(pid);
			// job.setState(kForeground);	// go crazy! go nuts! why doesn't sync() set this state?!
			process.setState(kRunning);
			joblist.synchronize(job);
		}	
	}
}

static void sigint_handler(int sig) {
	if(joblist.hasForegroundJob()) {
		STSHJob& job = joblist.getForegroundJob();
		// pid_t pgid = job.getGroupID();
		// kill(-pgid, SIGINT);
		vector<STSHProcess>& processes = job.getProcesses();
		for(unsigned int i = 0; i < processes.size(); i++) {
			STSHProcess &process = processes[i];
			kill(process.getID(), SIGINT);
		}
	}	
}

static void sigtstp_handler(int sig) {
	if(joblist.hasForegroundJob()) {
		STSHJob& job = joblist.getForegroundJob();
		// pid_t pgid = job.getGroupID();
		// kill(-pgid, SIGTSTP);
		vector<STSHProcess>& processes = job.getProcesses();
		for(unsigned int i = 0; i < processes.size(); i++) {
			STSHProcess &process = processes[i];
			kill(process.getID(), SIGTSTP);
		}
	}	
}
/**
 * Function: installSignalHandlers
 * -------------------------------
 * Installs user-defined signals handlers for four signals
 * (once you've implemented signal handlers for SIGCHLD, 
 * SIGINT, and SIGTSTP, you'll add more installSignalHandler calls) and 
 * ignores two others.
 */
static void installSignalHandlers() {
	installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
	installSignalHandler(SIGTTIN, SIG_IGN);
	installSignalHandler(SIGTTOU, SIG_IGN);
	installSignalHandler(SIGCHLD, sigchld_handler);
	installSignalHandler(SIGINT, sigint_handler);
	installSignalHandler(SIGTSTP, sigtstp_handler);
}

static void showPipeline(const pipeline& p) {
	for(unsigned int i = 0; i < p.commands.size(); i++) {
		for(unsigned int j = 0; j <= kMaxArguments && p.commands[i].tokens[j] != NULL; j++) {
		}
	}
}

/**
 * Function: createJob
 * -------------------
 * Creates a new job on behalf of the provided pipeline.
 */
static void createJob(const pipeline& p) {
	// showPipeline(p);
	STSHJob& job = joblist.addJob(kForeground);
	pid_t pgid = 0;
	if(p.background) {
		job.setState(kBackground);
	}

	int fds[p.commands.size() - 1][2];
	for(unsigned int i = 0; i < p.commands.size() - 1; i++) {
		pipe(fds[i]);
	}
	
	int infd = 0;
	if(!p.input.empty()) {
		infd = open(p.input.c_str(), O_RDONLY);
	}

	int outfd = 0;
	if(!p.output.empty()) {
		outfd = open(p.output.c_str(), O_WRONLY|O_TRUNC);
		if(outfd == -1 && errno == ENOENT) {
			outfd = open(p.output.c_str(), O_WRONLY|O_CREAT, 0644);
		}
	}

	for(unsigned int i = 0; i < p.commands.size(); i++) {
		pid_t pid = fork();
		if(pid == 0) {
			// for fds
			if(i == 0) {
				if(!p.input.empty()) {
					dup2(infd, STDIN_FILENO);
					close(infd);
				}
				close(fds[i][0]);
				dup2(fds[i][1], STDOUT_FILENO);
				close(fds[i][1]);
				for(unsigned int j = 1; j < p.commands.size() - 1; j++) {
					close(fds[j][0]);
					close(fds[j][1]);
				}
			} else if(i == p.commands.size() - 1) {
				if(!p.output.empty()) {
					dup2(outfd, STDOUT_FILENO);
					close(outfd);
				}
				close(fds[i - 1][1]);
				dup2(fds[i - 1][0], STDIN_FILENO);
				close(fds[i - 1][0]);
				for(unsigned int j = 0; j < p.commands.size() - 2; j++) {
					close(fds[j][0]);
					close(fds[j][1]);
				}
			} else  {
				close(fds[i - 1][1]);
				dup2(fds[i - 1][0], STDIN_FILENO);
				close(fds[i - 1][0]);
				close(fds[i][0]);
				dup2(fds[i][1], STDOUT_FILENO);
				close(fds[i][1]);
				for(unsigned int j = 0; j < p.commands.size() - 1; j++) {
					if(j != i - 1 && j != i) {
						close(fds[j][0]);
						close(fds[j][1]);
					}
				}
			}



			char* argv[kMaxArguments + 2] = {NULL};
			argv[0] = const_cast<char*>(p.commands[i].command);
			for(unsigned int j = 0; j <= kMaxArguments && p.commands[i].tokens[j] != NULL; j++) {
				argv[j + 1] = p.commands[i].tokens[j];
			}
			if(i == 0) {
				pgid = getpid();
				setpgid(getpid(), pid);
			} else {
				if(pgid != 0) {
					setpgid(pid, pgid);
				}
			}
			int err = execvp(argv[0], argv);
			if(err < 0) throw STSHException("Command not found");
		} else {

			job.addProcess(STSHProcess(pid, p.commands[i]));
			if(i == 0) {
				pgid = pid;
				setpgid(pid, pgid);

			} else {
				if(pgid != 0) {
					setpgid(pid, pgid);
				}
			}
		}
	}

	// close fds
	for(unsigned int i = 0; i < p.commands.size() - 1; i++) {
		close(fds[i][0]);
		close(fds[i][1]);
	}

	sigset_t additions, existingmask;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigaddset(&additions, SIGINT);
	sigaddset(&additions, SIGTSTP);
	sigaddset(&additions, SIGCONT);
	sigprocmask(SIG_BLOCK, &additions, &existingmask);

	if(p.background) {
		string str = "";
		str += "[" + to_string(job.getNum()) + "]";
		cout << setw(str.size()) << str << " ";
		vector<STSHProcess>& processes = job.getProcesses();
		for(unsigned int i = 0; i < processes.size(); i++) {
			cout << processes[i].getID() << "  ";
		}
		cout << endl;
	}

	if(joblist.hasForegroundJob()) {
		int err = tcsetpgrp(STDIN_FILENO, pgid);
		if(err == -1) {
			if(errno != ENOTTY) {
				throw STSHException("A more serious problem happens.");
			}
		}
	}

	while(joblist.hasForegroundJob()) {
		sigsuspend(&existingmask);
	}
	sigprocmask(SIG_UNBLOCK, &additions, NULL);

	pid_t parent = getpgid(getpid());
	int err = tcsetpgrp(STDIN_FILENO, parent);
	if(err == -1) {
		if(errno != ENOTTY) {
			throw STSHException("A more serious problem happens.");
		}
	}
}

/**
 * Function: main
 * --------------
 * Defines the entry point for a process running stsh.
 * The main function is little more than a read-eval-print
 * loop (i.e. a repl).  
 */
int main(int argc, char *argv[]) {
	pid_t stshpid = getpid();
	installSignalHandlers();
	rlinit(argc, argv);
	while (true) {
		string line;
		if (!readline(line)) break;
		if (line.empty()) continue;
		try {
			pipeline p(line);
			bool builtin = handleBuiltin(p);
			if (!builtin) createJob(p);
		} catch (const STSHException& e) {
			cerr << e.what() << endl;
			if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
		}
	}

	return 0;
}
