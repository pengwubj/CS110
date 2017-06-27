#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"

using namespace std;

struct worker {
	worker() {}
	worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
	subprocess_t sp;
	bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static void markWorkersAsAvailable(int sig) {
	while(true) {
		pid_t pid = waitpid(-1, NULL, WNOHANG|WUNTRACED);
		if(pid <= 0) {
			break;
		}
		// reap a halted worker, mark him available
		for(size_t w = 0; w < kNumCPUs; w++) {
			if(workers[w].sp.pid == pid) {
				workers[w].available = true;
				numWorkersAvailable++;
				break;
			}
		}
	}
}

static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};
static void spawnAllWorkers() {
	cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
	for (size_t i = 0; i < kNumCPUs; i++) {
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(i, &set);
		workers[i] = worker((char**)kWorkerArguments);
		sched_setaffinity(workers[i].sp.pid, sizeof(cpu_set_t), &set);
		cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
	}
}

static size_t getAvailableWorker() {
	// set block mask for SIGCHLD
	sigset_t additions, existingmask;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigprocmask(SIG_BLOCK, &additions, &existingmask);
	// lift SIGCHLD and wait for intriguing handler
	while(numWorkersAvailable == 0) {
		sigsuspend(&existingmask);
	}
	int idx = kNumCPUs;
	for(size_t w = 0; w < kNumCPUs; w++) {
		if(workers[w].available) {
			idx = w;
			break;
		}
	}
	// unblcok SIGCHLD
	sigprocmask(SIG_UNBLOCK, &additions, NULL);
	return idx;
}

static void broadcastNumbersToWorkers() {
	while (true) {
		string line;
		getline(cin, line);
		if (cin.fail()) break;
		size_t endpos;
		long long num = stoll(line, &endpos);
		if (endpos != line.size()) break;
		// get available worker index
		size_t idx = getAvailableWorker();
		if(idx < kNumCPUs) {
			workers[idx].available = false;
			numWorkersAvailable--;
			// feed num to supplyfd and tell him to continue
			dprintf(workers[idx].sp.supplyfd, "%lld\n", num);
			kill(workers[idx].sp.pid, SIGCONT);
		}
	}
}

static void waitForAllWorkers() {
	// blcok SIGCHLD
	sigset_t additions, existingmask;
	sigemptyset(&additions);
	sigaddset(&additions, SIGCHLD);
	sigprocmask(SIG_BLOCK, &additions, &existingmask);
	// check all workers finish their work
	while(numWorkersAvailable != kNumCPUs) {
		sigsuspend(&existingmask);
	}
	// unblock SIGCHLD
	sigprocmask(SIG_UNBLOCK, &additions, NULL);
}

static void closeAllWorkers() {
	signal(SIGCHLD, SIG_DFL);
	for(size_t w = 0; w < kNumCPUs; w++) {
		// close supplyfds to indicate an EOF
		close(workers[w].sp.supplyfd);
		// py ends when detecting an EOF, but should restart first
		kill(workers[w].sp.pid, SIGCONT);
	}
	// check all workers come back
	for(size_t i = 0; i < kNumCPUs; i++) {
		while(true) {
			int status;
			waitpid(workers[i].sp.pid, &status, 0);
			if(WIFEXITED(status)) {
				break;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	signal(SIGCHLD, markWorkersAsAvailable);
	spawnAllWorkers();
	broadcastNumbersToWorkers();
	waitForAllWorkers();
	closeAllWorkers();
	return 0;
}
