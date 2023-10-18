#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

void run_shell();
bool parse_command(char* buffer, int* status, int* bg_pid_array, int* num_bg_pids);
void change_dir(char* path);
void expand_substr(char* buffer, char* dest_str, char* shell_pid_str);
bool symbol_parse(char** user_string, int num_args, char* input_file, char* output_file);
void redirect_in_out(char* input_file, char* output_file, bool run_bg);

#endif