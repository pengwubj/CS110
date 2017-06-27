/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
	int fds[2];
	pipe(fds);
	pid_t pid1 = fork();
	if(pid1 == 0) {
		pids[0] = pid1;
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		close(fds[1]);
		execvp(argv1[0], argv1);
	}
	pid_t pid2 = fork();
	if(pid2 == 0) {
		pids[1] = pid2;
		close(fds[1]);
		dup2(fds[0], STDIN_FILENO);
		close(fds[0]);
		execvp(argv2[0], argv2);
	}
	close(fds[0]);
	close(fds[1]);
}
