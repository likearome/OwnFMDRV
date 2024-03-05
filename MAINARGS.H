// MAINARGS.H
#ifndef _MAINARGS_H_
#define _MAINARGS_H_

// main에서 parseargv에 넘겨줄 구조체
struct argstruct {
	int argc;
	char** argv;
	char* gametype;
	char targetcddrive;
	int volume;
	unsigned char flags; // ARGFL_TARGETCDDRIVE, ARGFL_UNLOAD, ARGFL_LOADHIGH, ARGFL_VERBOSE, ARGFL_DIGITALVOLUME
};

#define ARGFL_TARGETCDDRIVE 1
#define ARGFL_VOLUME 2
#define ARGFL_UNLOAD 4
#define ARGFL_LOADHIGH 8
#define ARGFL_VERBOSE 16
#define ARGFL_DIGITALVOLUME 32

extern struct argstruct args;

#define ErrorLog(...)           fprintf(stderr, __VA_ARGS__)
#define InfoLog(...)            if( (args.flags & ARGFL_VERBOSE) != 0 ) printf(__VA_ARGS__)

#endif
