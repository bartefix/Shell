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

struct background_processes_info background_list;
struct ChildProcessInfo child_info;

void handleChildTermination(int signo) {
    pid_t child_pid;
    int status;
	int temp_errno = errno;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {

		int is_foreground=0;
		for(int i=0;i<child_info.num;i++){
			if(child_info.child_pids[i]==child_pid)is_foreground=1;
		}
		if(is_foreground){
			remove_child_pid(&child_info,child_pid);
		}
		else
		{
			if (WIFEXITED(status)) {
				add_background_process(&background_list,child_pid, WEXITSTATUS(status),1);
			}
			if (WIFSIGNALED(status)){
				add_background_process(&background_list,child_pid, WTERMSIG(status),0);
			}
		}
	}
	errno = temp_errno;
}

void block(int SIGNAL){
		sigset_t mask;
    	sigemptyset(&mask);
    	sigaddset(&mask, SIGNAL);
    	sigprocmask(SIG_BLOCK, &mask, NULL);
	}

void unblock(int SIGNAL){
		sigset_t mask;
    	sigemptyset(&mask);
    	sigaddset(&mask, SIGNAL);
    	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	}

void execute_command(command* com, char* args[], int input_fd, int output_fd){
	char* input_path = NULL;
	char* output_path = NULL;
	int append = 0;
	redirseq* start = com->redirs;
	if(start!=NULL){
		redirseq* temp = com->redirs;

		if(IS_RIN(temp->r->flags)){
				input_path=temp->r->filename;
			}
			if(IS_ROUT(temp->r->flags) || IS_RAPPEND(temp->r->flags)){
				output_path=temp->r->filename;
				if(IS_RAPPEND(temp->r->flags))append = 1;
			}
		temp= temp->next;
		while(temp!=start){
			if(IS_RIN(temp->r->flags)){
				input_path=temp->r->filename;
			}
			if(IS_ROUT(temp->r->flags) || IS_RAPPEND(temp->r->flags)){
				output_path=temp->r->filename;
				if(IS_RAPPEND(temp->r->flags))append = 1;
			}
		temp= temp->next;
		}
	}
	if(input_path != NULL){
		close(input_fd);
		input_fd = open(input_path, O_RDONLY);
	}
	if(output_path != NULL){
		close(output_fd);
		int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
		output_fd = open(output_path,flags,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	}
	if(input_fd == -1){
		if(errno == EACCES)fprintf(stderr, "%s: permission denied\n", input_path);
		if(errno == ENOENT)fprintf(stderr, "%s: no such file or directory\n", input_path);
		exit(EXEC_FAILURE);
	}
	if( output_fd == -1){
		if(errno == EACCES)fprintf(stderr, "%s: permission denied\n", output_path);
		exit(EXEC_FAILURE);
	}

	if (input_fd != STDIN_FILENO) {
		dup2(input_fd, STDIN_FILENO);
		close(input_fd);
	}
	if (output_fd != STDOUT_FILENO) {
		dup2(output_fd, STDOUT_FILENO);
		close(output_fd);
	}
	execvp(args[0], args);
	if(errno==ENOENT)fprintf(stderr,"%s: no such file or directory\n",args[0]);
		else if(errno==EACCES)fprintf(stderr,"%s: permission denied\n",args[0]);
			else fprintf(stderr,"exec error\n");
		exit(EXEC_FAILURE);
}

int check_first_command(command * com){
	argseq* temp = com->args->next;
	argseq* start = com->args;
	int k=1;
	char* args[MAX_LINE_LENGTH/2+1];
	args[0]=start->arg;
	while(temp!=start){
		args[k] = temp->arg;
		k++;
		temp = temp->next;
	}
	args[k]=NULL;
	int was_shell=0; 
	for(int i=0;builtins_table[i].name!=NULL;i++){
		if(strcmp(builtins_table[i].name,args[0])==0){
			was_shell=1;
			builtins_table[i].fun(args);
			break;
		}
	}
	return was_shell;
}

void execute_pipeline(pipeline* ppl){
	commandseq* comseq = ppl->commands;
	command* com =NULL;
	comseq->prev->next=NULL;
	int in,fd[2];
	in = STDIN_FILENO;
	while(comseq->next!=NULL){
		com = comseq->com;
		argseq* temp = com->args->next;
		argseq* start = com->args;
		int k=1;
		char* args[MAX_LINE_LENGTH/2+1];
		args[0]=start->arg;
		while(temp!=start){
			args[k] = temp->arg;
			k++;
			temp = temp->next;
		}
		args[k]=NULL;

		if(pipe(fd) == -1){ 
			exit(EXEC_FAILURE);
		}

		block(SIGCHLD);

		pid_t child_pid = fork();
		if (child_pid == -1) {
			exit(EXEC_FAILURE);
		}
			add_child_pid(&child_info,child_pid);
		if (child_pid == 0) {
			close(fd[0]);
			execute_command(com,args,in, fd[1]);
		}
		else {
			unblock(SIGCHLD);
			close(fd[1]);
			if(in!= STDIN_FILENO)close(in);
			in = fd[0];
			comseq = comseq->next;
		}	
	}
	com = comseq->com;
	argseq* temp = com->args->next;
	argseq* start = com->args;
	int k=1;
	char* args[MAX_LINE_LENGTH/2+1];
	args[0]=start->arg;
	while(temp!=start){
		args[k] = temp->arg;
		k++;
		temp = temp->next;
	}
	args[k]=NULL;

	block(SIGCHLD);

	pid_t child_pid = fork();
	if (child_pid == -1) {
		exit(EXEC_FAILURE);
	}
	add_child_pid(&child_info,child_pid);
	if (child_pid == 0) {
		execute_command(com,args,in, STDOUT_FILENO);
	}
	if(in!= STDIN_FILENO)close(in);

	sigset_t empty;
	sigemptyset(&empty);
	while(child_info.num>0)sigsuspend(&empty);
	unblock(SIGCHLD);
}

void execute_pipeline_background(pipeline* ppl){
	commandseq* comseq = ppl->commands;
	command* com =NULL;
	comseq->prev->next=NULL;
	int in,fd[2];
	in = STDIN_FILENO;
	while(comseq->next!=NULL){
		com = comseq->com;
		argseq* temp = com->args->next;
		argseq* start = com->args;
		int k=1;
		char* args[MAX_LINE_LENGTH/2+1];
		args[0]=start->arg;
		while(temp!=start){
			args[k] = temp->arg;
			k++;
			temp = temp->next;
		}
		args[k]=NULL;

		if(pipe(fd) == -1){ 
			exit(EXEC_FAILURE);
		}

		block(SIGCHLD);

		pid_t child_pid = fork();
		if (child_pid == -1) {
			exit(EXEC_FAILURE);
		}
		if (child_pid == 0) {
			if (setsid() == -1) {
           		perror("setsid");
            	exit(EXIT_FAILURE);
        	}
			close(fd[0]);
			execute_command(com,args,in, fd[1]);
		}
		else {
			unblock(SIGCHLD);
			close(fd[1]);
			if(in!= STDIN_FILENO)close(in);
			in = fd[0];
			comseq = comseq->next;
		}	
	}
		com = comseq->com;
		argseq* temp = com->args->next;
		argseq* start = com->args;
		int k=1;
		char* args[MAX_LINE_LENGTH/2+1];
		args[0]=start->arg;
		while(temp!=start){
			args[k] = temp->arg;
			k++;
			temp = temp->next;
		}
		args[k]=NULL;
		block(SIGCHLD);
		pid_t child_pid = fork();
		if (child_pid == -1) {
			exit(EXEC_FAILURE);
		}
		if (child_pid == 0) {
			if (setsid() == -1) {
				perror("setsid");
				exit(EXIT_FAILURE);
        	}
			execute_command(com,args,in, STDOUT_FILENO);
		}

	unblock(SIGCHLD);
	if(in!= STDIN_FILENO)close(in);
}

int main(int argc, char *argv[])
{
	background_list.num=0;
	child_info.num=0;
	struct sigaction sa_parent;
    sa_parent.sa_handler = SIG_IGN;
    sigemptyset(&sa_parent.sa_mask);
    sa_parent.sa_flags = 0;
    sigaction(SIGINT, &sa_parent, NULL);

    struct sigaction sa_child_termination;
    sa_child_termination.sa_handler = handleChildTermination;
    sigemptyset(&sa_child_termination.sa_mask);
    sa_child_termination.sa_flags = SA_RESTART | SA_NOCLDSTOP; 
    sigaction(SIGCHLD, &sa_child_termination, NULL);

	pipelineseq * ln;
	command *com;
	char buf[MAX_LINE_LENGTH*2];
	buf[0]=0;
	size_t bytes_read;
	int j=0;
	struct stat stat_info;
	int _fstat = fstat(0, &stat_info);
	int _s_ischr = S_ISCHR(stat_info.st_mode);

    while (1){
		if ( _fstat == 0 && _s_ischr) {
			block(SIGCHLD);
			for(int i=0;i<background_list.num;i++){
				if(background_list.background_is_exit[i]==1)
					fprintf(stdout,"Background process %d terminated. (exited with status %d)\n", background_list.background_pid[i], background_list.background_status[i]);
				if(background_list.background_is_exit[i]==0)
					fprintf(stdout,"Background process %d terminated.  (killed by signal %d)\n", background_list.background_pid[i], background_list.background_status[i]);
			}
			background_list.num = 0;
			unblock(SIGCHLD);
			fprintf(stdout,"%s",PROMPT_STR);
			fflush(stdout);
		}

		bytes_read = read(STDIN_FILENO, buf+j, MAX_LINE_LENGTH-j);
		j+=bytes_read;
		buf[j]=0;
		if(bytes_read==0 && buf[0]=='\0')break;

		if(strchr(buf,'\n')==NULL){
			if(strlen(buf)>=MAX_LINE_LENGTH){
				j=0;
				while(strchr(buf,'\n')==NULL){
					bytes_read = read(STDIN_FILENO,buf,sizeof(buf));
					j=bytes_read;
				}
				size_t ptr = strchr(buf,'\n')-buf;
				size_t move_size = strlen(buf)-ptr-1; 
				memmove(buf,buf+ptr+1,move_size);
				buf[move_size]=0;
				j-=ptr+1;
				fprintf(stderr,"%s\n",SYNTAX_ERROR_STR);
				continue;
			}
			else
			{
				continue;
			}
		}
		size_t ptr = strchr(buf,'\n')-buf;
		size_t move_size = strlen(buf)-ptr-1;
		buf[ptr]='\0';
		ln=parseline(buf);
		memmove(buf,buf+ptr+1,move_size);
		buf[move_size]=0;
		j-=ptr+1;
		if(ln==NULL){
			fprintf(stderr,"%s \n",SYNTAX_ERROR_STR);
			continue;
		}
		if(pickfirstcommand(ln)==NULL)continue;
		ln->prev->next=NULL;
		while(ln!=NULL){
			com = ln->pipeline->commands->com;
			if(check_first_command(com)){
				ln = ln->next;
				continue;
			}
			else
			{
				if(ln->pipeline->flags==INBACKGROUND)
					execute_pipeline_background(ln->pipeline);
				else	
					execute_pipeline(ln->pipeline);
				ln = ln->next;
			}
		}
	}
	return 0;
}
