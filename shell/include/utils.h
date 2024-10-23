#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

void printcommand(command *, int);
void printpipeline(pipeline *, int);
void printparsedline(pipelineseq *);
command * pickfirstcommand(pipelineseq *);

struct ChildProcessInfo {
    pid_t child_pids[MAX_LINE_LENGTH];
    volatile int num;
};

struct background_processes_info {
    int background_pid[MAX_LINE_LENGTH];
    int background_status[MAX_LINE_LENGTH];
    int background_is_exit[MAX_LINE_LENGTH];
    volatile int num;
};

void add_child_pid(struct ChildProcessInfo *info, pid_t pid);
void remove_child_pid(struct ChildProcessInfo *info, pid_t pid);
void add_background_process(struct background_processes_info *info, pid_t pid, int status,int is_exit_status);
#endif /* !_UTILS_H_ */
