#include "shell.h"
    
/***********************************************************************************
*                           run_shell()
*
*   Run smallsh. Repeatedly prompt for user input and handle children processes.
*   
*   
************************************************************************************/
void run_shell(){

    size_t buf_size = 2048;
    char* buffer = calloc(buf_size, sizeof(char));

    size_t pid_array_size = 256;
    int* bg_pid_array = calloc(pid_array_size, sizeof(int));
    int num_bg_pids = 0;
    int child_status = 0;
    int child_pid = 0;

    char user_input[2048];
    bool exit_shell = false;
    int status = 0;
    char shell_pid_str[10];
    sprintf(shell_pid_str, "%i", getpid());

    // ignore SIGINT for parent and background children
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // ignore SIGTSTP (to make p3testscript function)
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = SIG_IGN;
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    printf("\nStarting terminal.");
    fflush(stdout);

    while(!exit_shell){

        // check for any zombie children
        child_pid = waitpid(-1, &child_status, WNOHANG);

        // while any zombies exist
        while(child_pid > 0){

            // if child process executed successfully, get status
            if(WIFEXITED(child_status)){
                status = WEXITSTATUS(child_status);
                printf("\n\nBackground pid %i is done: exit value %i", child_pid, status);
                fflush(stdout);
            // if child process terminated abnormally, get status
            } else if(WIFSIGNALED(child_status)){
                status = WTERMSIG(child_status);
                printf("\nBackground pid %i is done: terminated by signal %i", child_pid, status);
                fflush(stdout);
            }

            // kill the zombie child
            kill(child_pid, 0);

            // if there are multiple bg pids
            if(num_bg_pids > 1){

                // iterate through the pid array to find where this pid is
                for(int i=0; i < num_bg_pids; i++){
                    // fill that index with the pid at the end of the array
                    if(bg_pid_array[i] == child_pid){
                        bg_pid_array[i] = bg_pid_array[num_bg_pids - 1];
                    }
                }
            }

            // remove from list of bg pids and check for remaining zombies
            num_bg_pids--;
            child_pid = waitpid(-1, &child_status, WNOHANG);

        }

        // prompt
        printf("\n: ");
        fflush(stdout);

        // get user input + expand "$$"
        getline(&buffer, &buf_size, stdin);
        expand_substr(buffer, user_input, shell_pid_str);
        
        exit_shell = parse_command(user_input, &status, bg_pid_array, &num_bg_pids);

    }
    
    free(buffer);
    exit(1);

}

/***********************************************************************************
*                           run_shell()
*
*   Run smallsh. Repeatedly prompt for user input and handle children processes.
*   
*   Returns:
*       - bool:
*           true if user wants to exit smallsh (via exit command)
*           false if user enters anything else
************************************************************************************/
bool parse_command(char* buffer, int* status, int* bg_pid_array, int* num_bg_pids){

    char* exit_str = "exit";
    char* status_str = "status";
    char* cd_str = "cd";
    char* comment = "#";

    char* user_cmd;
    char* save_ptr;
    int num_args = 0;

    char* user_args[30];
    char input_file[30];
    char output_file[30];
    bool run_bg = false;

    // get command
    user_cmd = strtok_r(buffer, " \n", &save_ptr);
    if(user_cmd != NULL){
        user_args[0] = user_cmd;
        num_args = 1;
    }

    // parse into individual arguments
    while((num_args < 513) && (user_cmd != NULL)){
        user_cmd = strtok_r(NULL, " \n", &save_ptr);
        if(user_cmd != NULL){
            user_args[num_args] = user_cmd;
            num_args += 1;
        }
    }

    // if the user entered something
    if(num_args > 0){
        
        // check symbols
        run_bg = symbol_parse(user_args, num_args, input_file, output_file);

        // exit; exit regardless of following arguments
        if(strcmp(user_args[0], exit_str) == 0){
            printf("Exiting terminal.\n");
            fflush(stdout);

            // kill any remaining children
            for(int i=0; i < *num_bg_pids; i++){
                kill(bg_pid_array[i], SIGKILL);
            }

            return true;

        // status
        } else if (strcmp(user_args[0], status_str) == 0) {
            printf("exit value %i", *status);
            fflush(stdout);
            return false;

        // cd
        } else if (strcmp(user_args[0], cd_str) == 0){
            change_dir(user_args[1]);
            return false;

        // if it's a comment, ignore it
        } else if(strncmp(user_args[0], comment, 1) == 0) {
            return false;

        // otherwise, try to execute it
        } else{

            int child_status;

            // fork child process
            pid_t child_pid = fork();

            // fork failed
            if(child_pid < 0){
                perror("\nfork() failed.");
                fflush(stdout);
                *status = 1;

            // child process
            } else if (child_pid == 0){

                // redirect input/output per user args
                redirect_in_out(input_file, output_file, run_bg);    

                // allow SIGINT to terminate foreground child process
                if(!run_bg){
                    struct sigaction SIGINT_action = {0};
                    SIGINT_action.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &SIGINT_action, NULL);
                }

                // ignore SIGTSTP
                struct sigaction SIGTSTP_action = {0};
                SIGTSTP_action.sa_handler = SIG_IGN;
                sigaction(SIGTSTP, &SIGTSTP_action, NULL);

                // execute program
                execvp(user_args[0], user_args);

                // return an error if the execution failed
                perror("\nExecution failed");
                exit(1);

            // parent process
            } else {

                // if child is foreground process
                if(!run_bg){
                    child_pid = waitpid(child_pid, &child_status, 0);
                    // if child process executed successfully, get status
                    if(WIFEXITED(child_status)){
                        *status = WEXITSTATUS(child_status);
                    // if child process terminated abnormally, get status
                    } else if(WIFSIGNALED(child_status)){
                        *status = WTERMSIG(child_status);
                        printf("Terminated with signal: %i", *status);
                    }

                // if child is background process
                } else {
                    // keep track of this process's pid
                    printf("\nStarting process %i", child_pid);
                    bg_pid_array[*num_bg_pids] = child_pid;
                    *num_bg_pids = *num_bg_pids + 1;
                }
            }

            return false;
        }
    }

    // if nothing was entered
    return false;
}

/***********************************************************************************
*                           change_dir()
*
*   Change the current directory.
*   
*   
************************************************************************************/
void change_dir(char* path){
    char curr_dir[PATH_MAX];
    int success = 0;
    getcwd(curr_dir, PATH_MAX);

    char* home_path = getenv("HOME");

    // if no path, go to HOME directory
    if(path == NULL){
                    
        // change to HOME
        success = chdir(home_path);

        if(success == 0){
            getcwd(curr_dir, PATH_MAX);
        } else {
            perror("\nFailed to change to home directory");
        }

    } else {
        success = chdir(path);

        if(success == 0){
            getcwd(curr_dir, PATH_MAX);
        } else {
            perror("\nFailed to change to directory");
        }
    }
}

/***********************************************************************************
*                           expand_substr()
*
*   Expands any instances of $$ into the current process's PID.
*   
*   
************************************************************************************/
void expand_substr(char* buffer, char* dest_str, char* shell_pid_str){
    char* last_occurence = buffer;
    char* this_occurence = NULL;

    // initialize destination string
    strncpy(dest_str, "\0", 1);

    // check for any occurences of "$$"
    this_occurence = strstr(buffer, "$$");

    // if an occurence is found
    while(this_occurence != NULL){
        // add any characters between this and the last occurence to the expanded string
        strncat(dest_str, last_occurence, ((this_occurence - last_occurence) / sizeof(char)));
        // add the expanded pid num
        strncat(dest_str, shell_pid_str, strlen(shell_pid_str));
        // move last occurence to this occurence, skipping over $$ substring
        last_occurence = this_occurence + 2 * sizeof(char);
        // get next occurence
        this_occurence = strstr(last_occurence, "$$");
    }

    // copy remaining characters over
    strncat(dest_str, last_occurence, strlen(buffer));
    fflush(stdout);

}

/***********************************************************************************
*                           symbol_parse()
*
*   Check for any symbols (<, >, &) and set input/output/background as needed.
*   
*   
************************************************************************************/
bool symbol_parse(char** user_string, int num_args, char* input_file, char* output_file){
    bool seen_first_symbol = false;
    bool background = false;
    char* input_symbol = "<";
    char* output_symbol = ">";
    char* bg_symbol = "&";

    // initialize the strings for comparison later in parse_command
    strncpy(input_file, "\0", 1);
    strncpy(output_file, "\0", 1);

    for(int i=0; i < num_args; i++){
        // check for input file
        if(strcmp(user_string[i], input_symbol) == 0){
            // save input filename, max length 29 chars
            strncpy(input_file, user_string[i+1], 29);
            // don't pass any symbols to later functions
            if(!seen_first_symbol){
                seen_first_symbol = true;
                user_string[i] = NULL;
            }
        // check for output file
        } else if(strcmp(user_string[i], output_symbol) == 0){
            // save output filename, max length 29 chars
            strncpy(output_file, user_string[i+1], 29);
            // don't pass any symbols to later functions
            if(!seen_first_symbol){
                seen_first_symbol = true;
                user_string[i] = NULL;
            }
        }
    }

    // check for background symbol
    if(strcmp(user_string[num_args - 1], bg_symbol) == 0){
        background = true;
        // don't pass any symbols to later functions
        if(!seen_first_symbol){
            seen_first_symbol = true;
            user_string[num_args - 1] = NULL;
        }
    }

    // if there are no symbols, end with NULL argument
    if(!seen_first_symbol){
        user_string[num_args] = NULL;
    }

    // determine whether to execute in the background
    return background;
}

/***********************************************************************************
*                           redirect_in_out()
*
*   Redirects input and output for children based on provided user arguments.
*   If input and/or output is not redirected for a background process,
*   input/output is automatically redirected to /dev/null.
*   
*   
************************************************************************************/
void redirect_in_out(char* input_file, char* output_file, bool run_bg){
    int in_desc;
    int out_desc;

    // if user specified an input file
    if(strlen(input_file) > 0){

        // open the input file for reading
        in_desc = open(input_file, O_RDONLY);
        if(in_desc == -1){
            perror("\nError opening input file");
            exit(1);
        }
        // redirect stdin to input file
        int result = dup2(in_desc, 0);
        if(result == -1){
            perror("\nError dup2() for input");
            exit(1);
        }
    
    // if user did not specify an input file
    } else {
        // if this is a background process
        if(run_bg){
            // open /dev/null for reading
            in_desc = open("/dev/null", O_RDONLY);
            if(in_desc == -1){
                perror("\nError opening /dev/null for reading");
                exit(1);
            }
            // redirect stdin to input file
            int result = dup2(in_desc, 0);
            if(result == -1){
                perror("\nError dup2() /dev/null for input");
                exit(1);
            }
        }
    }

    // if user submitted an output file
    if(strlen(output_file) > 0){

        // open the output file for writing
        out_desc = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(out_desc == -1){
            perror("\nError opening output file");
            exit(1);
        }
        // redirect stdout to output file
        int result = dup2(out_desc, 1);
        if(result == -1){
            perror("\nError dup2() for output");
            exit(1);
        }
    
    // if user did not specify an output file
    } else {
        // if this is a background process
        if(run_bg){
            // open /dev/null for writing
            out_desc = open("/dev/null", O_WRONLY);
            if(out_desc == -1){
                perror("\nError opening /dev/null for writing");
                exit(1);
            }
            // redirect stdout to /dev/null
            int result = dup2(out_desc, 1);
            if(result == -1){
                perror("\nError dup2() /dev/null for writing");
                exit(1);
            }
        }
    }
}
