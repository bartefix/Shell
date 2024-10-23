#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "builtins.h"
#include <dirent.h>


int echo(char*[]);
int myexit(char *[]);
int lcd(char *[]);
int lkill(char *[]);
int lls(char *[]);

builtin_pair builtins_table[]={
	{"exit",	&myexit},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

int 
echo( char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int 
myexit(char * argv[])
{
	if(argv[1]!=NULL){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
	}
	exit(0);
}

int  
lcd(char * argv[])
{
	
	if(argv[1]==NULL){chdir(getenv("HOME"));
	return 0;}
	if(argv[2]!= NULL){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
		}
	if(chdir(argv[1])==-1){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
	}
	return 0;
}
int 
lkill(char * argv[])
{
	if(argv[1]==NULL || argv[3]!=NULL){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
	}
	int out;
	
	if(argv[2]==NULL){
		char *endptr;
		long pid_value = strtol(argv[1], &endptr, 10);

		if (*endptr != '\0' || pid_value == 0) {
			fprintf(stderr, "Builtin %s error.\n", argv[0]);
			return BUILTIN_ERROR;
		}
		out = kill(pid_value, SIGTERM);
	}
	else
	{
		char *endptr;
		long pid_value = strtol(argv[2], &endptr, 10);
		char *inputString = argv[1];
    	if (inputString[0] == '-') {
        	size_t length = strlen(inputString);
        	for (size_t i = 0; i < length; ++i) {
            	inputString[i] = inputString[i + 1];
       		}
   		}
		int _sig = strtol(inputString, &endptr,10);
		if (*endptr != '\0' || pid_value == 0) {
			fprintf(stderr, "Builtin %s error.\n", argv[0]);
			return BUILTIN_ERROR;
		}
		out = kill(pid_value, _sig);
	}
	if(out==-1){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
	}
	return 0;
	
}
int 
lls(char * argv[])
{
	DIR* dir = opendir(".");
	struct dirent* entry;
	if(dir==NULL){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
		}
	while((entry = readdir(dir))!= NULL){
		char* name = entry->d_name;
		if(name[0]!='.')printf("%s\n", name);
	}

	if(closedir(dir)==-1){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
		return BUILTIN_ERROR;
	}
		fflush(stdout);
		return 0;
}
