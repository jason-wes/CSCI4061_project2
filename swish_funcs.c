#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // TODO Task 0: Tokenize string s
    // Assume each token is separated by a single space (" ")
    // Use the strtok() function to accomplish this
    // Add each token to the 'tokens' parameter (a string vector)
    // Return 0 on success, -1 on error
    char *token;
    const char delim[2] = " ";

    token = strtok(s, delim);
    // not sure if this is proper error handling. strok doesn't error
    if(token == NULL) {
        perror("Str not tokenized. Returned null.");
        return -1;
    }

    while(token != NULL) {

        // error checking strvec_add
        if (strvec_add(tokens, token) == -1) {
            perror("Error adding token to strvec object");
            return -1;
        }

        token = strtok(NULL, delim);
    }

    return 0;
}

int run_command(strvec_t *tokens) {
    // TODO(done) Task 2: Execute the specified program (token 0) with the
    // specified command-line arguments
    // THIS FUNCTION SHOULD BE CALLED FROM A CHILD OF THE MAIN SHELL PROCESS
    // Hint: Build a string array from the 'tokens' vector and pass this into execvp()
    // Another Hint: You have a guarantee of the longest possible needed array, so you
    // won't have to use malloc.

    // set child process to correct process group
    pid_t cpid = getpid();
    if(setpgid(cpid, cpid)) {
        perror("setpgid error");
        return 1;
    }

    // child process handling SIGTTIN and SIGTTOU correctly
    struct sigaction sac;
    sac.sa_handler = SIG_DFL;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }


    char* arr[MAX_ARGS];

    int fd_out = -2;
    int fd_in = -2;
    int i; 
    for(i = 0; i < tokens->length; i++) {

        arr[i] = strvec_get(tokens, i);
        
        // error checking strvec_get
        if (arr[i] == NULL) {
            perror("strvec_get error");
            return -1;
        } 
        
        else if (!strcmp(arr[i], "<")) {
            char* filename = strvec_get(tokens, i+1);

            // error checking strvec_get
            if (filename == NULL) {
                perror("strvec_get error");
                return -1;
            }

            // Open file for reading only if it exists
            fd_in = open(filename, O_RDONLY, S_IRUSR|S_IWUSR);

            // error checking opening input file
            if (fd_in == -1) {
                perror("Failed to open input file");
                return -1;
            }

            // redirect input to file and error check it
            if (dup2(fd_in, STDIN_FILENO) == -1) {
                perror("dup2");
                if(close(fd_in)) {
                    perror("error closing input file");
                }
                return -1;
            }

            // set ith element to NULL, array will already be filled with what we want to pass it execvp
            arr[i] = (char *) NULL;
        } 
        
        else if (!strcmp(arr[i], ">") || !strcmp(arr[i], ">>")) {
            char* filename = strvec_get(tokens, i+1);

            // error checking strvec_get
            if (filename == NULL) {
                perror("strvec_get error");
                return -1;
            }

            // Checking if output redirection should be in write or append mode
            if (!strcmp(arr[i], ">")) {
                fd_out = open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
            } else {
                fd_out = open(filename, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR);
            }

            // error checking open
            if (fd_out == -1) {
                perror("Failed to open output file");
                return -1;
            }

            // redirection and dup2 error checking
            if (dup2(fd_out, STDOUT_FILENO) == -1) {
                perror("dup2");
                if(close(fd_out)) {
                    perror("error closing output file");
                }
                return -1;
            }

            // set ith element to NULL, array will already be filled with what we want to pass it execvp
            arr[i] = (char *) NULL;
        } 

    }
    // set last element of array to NULL for execvp, may be redundant in some cases with redirection
    arr[i] = (char *) NULL;
    
    // error checking exec, only returns on exec failure
    // may give more arguments than necessary in arr but will terminate at the correct spot with a NULL char*
    if (execvp(arr[0], arr)) {
        perror("exec");

        // need to close open files since process didn't automatically end

        // fd_out open if not equal to -2
        if (fd_out != -2) {
            if (close(fd_out)) {
                perror("Failed to close output file");
            }
        }

        // fd_in open if not equal to -2
        if (fd_in != -2) {
            if (close(fd_in)) {
                perror("Failed to close output file");
            }
        }
        
        return -1;
    }

    // TODO(done) Task 3: Extend this function to perform output redirection before exec()'ing
    // Check for '<' (redirect input), '>' (redirect output), '>>' (redirect and append output)
    // entries inside of 'tokens' (the strvec_find() function will do this for you)
    // Open the necessary file for reading (<), writing (>), or appending (>>)
    // Use dup2() to redirect stdin (<), stdout (> or >>)
    // DO NOT pass redirection operators and file names to exec()'d program
    // E.g., "ls -l > out.txt" should be exec()'d with strings "ls", "-l", NULL

    // TODO(done) Task 4: You need to do two items of setup before exec()'ing
    // 1. Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults.
    // The code in main() within swish.c sets these handlers to the SIG_IGN value.
    // Adapt this code to use sigaction() to set the handlers to the SIG_DFL value.
    // 2. Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID

    // Not reachable after a successful exec(), but retain here to keep compiler happy
    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // TODO Task 5: Implement the ability to resume stopped jobs in the foreground
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    //    Feel free to use sscanf() or atoi() to convert this string to an int
    char* job_index_string = strvec_get(tokens, 1);
    if (job_index_string == NULL) {
        perror("strvec_get error");
        return -1;
    }

    int job_index = atoi(job_index_string);
    int jobs_length = jobs->length;

    // check that jobs index is within bounds
    if (job_index < 0 || job_index > jobs_length - 1) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }

    job_t *job = job_list_get(jobs, job_index);

    // error with jobs_list_get
    if (job == NULL) {
        return -1;
    }

    // 2. Call tcsetpgrp(STDIN_FILENO, <job_pid>) where job_pid is the job's process ID
    if(tcsetpgrp(STDIN_FILENO, job->pid)) {
        perror("tcsetpgrp error");
        return -1;
    }
    // 3. Send the process the SIGCONT signal with the kill() system call
    kill(job->pid, SIGCONT);
    // 4. Use the same waitpid() logic as in main -- dont' forget WUNTRACED
    int status = 0;
    waitpid(job->pid, &status, WUNTRACED);
    // 5. If the job has terminated (not stopped), remove it from the 'jobs' list
    if (!WIFSTOPPED(status)) {
        job_list_remove(jobs, job_index);
    }
    // 6. Call tcsetpgrp(STDIN_FILENO, <shell_pid>). shell_pid is the *current*
    //    process's pid, since we call this function from the main shell process
    pid_t ppid = getpid();
    if(tcsetpgrp(STDIN_FILENO, ppid)) {
        perror("tcsetpgrp error");
        return -1;
    }

    // TODO Task 6: Implement the ability to resume stopped jobs in the background.
    // This really just means omitting some of the steps used to resume a job in the foreground:
    // 1. DO NOT call tcsetpgrp() to manipulate foreground/background terminal process group
    // 2. DO NOT call waitpid() to wait on the job
    // 3. Make sure to modify the 'status' field of the relevant job list entry to JOB_BACKGROUND
    //    (as it was JOB_STOPPED before this)

    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // TODO Task 6: Wait for a specific job to stop or terminate
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Make sure the job's status is JOB_BACKGROUND (no sense waiting for a stopped job)
    // 3. Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    // 4. If the process terminates (is not stopped by a signal) remove it from the jobs list

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    // TODO Task 6: Wait for all background jobs to stop or terminate
    // 1. Iterate through the jobs list, ignoring any stopped jobs
    // 2. For a background job, call waitpid() with WUNTRACED.
    // 3. If the job has stopped (check with WIFSTOPPED), change its
    //    status to JOB_STOPPED. If the job has terminated, do nothing until the
    //    next step (don't attempt to remove it while iterating through the list).
    // 4. Remove all background jobs (which have all just terminated) from jobs list.
    //    Use the job_list_remove_by_status() function.

    return 0;
}
