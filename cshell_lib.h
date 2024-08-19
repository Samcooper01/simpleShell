#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define SHELL_PROMPT_TOKEN  "$ : "
#define ARG_DELIM   " "

#define MAX_USER_LINE   1024
#define MAX_COMMANDS    100
#define MAX_ARGS 100
