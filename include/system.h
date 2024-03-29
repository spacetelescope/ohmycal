//
// Created by jhunk on 10/4/23.
//

#ifndef OMC_SYSTEM_H
#define OMC_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>

struct Process {
    // Write stdout stream to file
    char f_stdout[PATH_MAX];
    // Write stderr stream to file
    char f_stderr[PATH_MAX];
    // Combine stderr and stdout (into stdout stream)
    int redirect_stderr;
    // Exit code from program
    int returncode;
};

int shell(struct Process *proc, char *args);
int shell_safe(struct Process *proc, char *args);
char *shell_output(const char *command, int *status);

#endif //OMC_SYSTEM_H
