#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include "LineParser.h"
#include <asm-generic/fcntl.h>
#include <linux/limits.h>



int debug = 0;

void execute(cmdLine *pCmdLine) {

    if (strcmp((*pCmdLine).arguments[0], "cd") == 0) { 
        if (chdir((*pCmdLine).arguments[1]) == -1) {  //arguments[1] is the path to the directory
            fprintf(stderr, "cd: No such file or directory\n", (*pCmdLine).arguments[1]); 
        }
    }
    //Arguments[0] is the command name
     else if (strcmp((*pCmdLine).arguments[0], "alarm") == 0) { // Check if the command is "alarm"
        pid_t pid = atoi((*pCmdLine).arguments[1]); 
        if (kill(pid, SIGCONT) == 0) { // Send SIGCONT signal to wake up the process
            printf("Process %d woke up\n", pid);
        } else {
            perror("ERROR: kill()"); 
        }
    } else if (strcmp((*pCmdLine).arguments[0], "blast") == 0) { // Check if the command is "blast"
        pid_t pid = atoi((*pCmdLine).arguments[1]); 
        if (kill(pid, SIGKILL) == 0) { // Send SIGKILL signal to terminate the process
            printf("Process %d terminated\n", pid);
        } else {
            perror("ERROR: kill() failed\n"); 
        } 
    }
    else {
        pid_t p = fork();
        if(p ==-1){
            perror("ERROR: fork() failed\n");
            exit(1);
        }
        if (p == 0) {
            if((*pCmdLine).inputRedirect != NULL){
                int input_file = open((*pCmdLine).inputRedirect, O_RDONLY);
            if (input_file == -1) {
                perror("ERROR: couldn't open the input file for reading");
                _exit(1);
            }
            // Duplicate the file descriptor to stdin (file descriptor 0)
            int result = dup2(input_file, STDIN_FILENO);
            if (result == -1) {
                perror("ERROR: couldn't accomplish dup2() for input redirection");
                _exit(1);
            }
            close(input_file);
            }
            if((*pCmdLine).outputRedirect != NULL){
                int output_file = open((*pCmdLine).outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_file == -1) {
                    perror("ERROR: couldn't open the output file for writing");
                    _exit(1);
                }
                // Duplicate the file descriptor to stdout (file descriptor 1)
                int result = dup2(output_file, STDOUT_FILENO);
                if (result == -1) {
                    perror("ERROR: couldn't accomplish dup2() for output redirection");
                    _exit(1);
                }
                result = dup2(output_file, STDERR_FILENO); // Redirect stderr to the same file descriptor as stdout
                if (result == -1) {
                    perror("ERROR: couldn't accomplish dup2() for error redirection");
                    _exit(1);
                }
                close(output_file);
            }
            if(debug){
                fprintf(stderr, "PID: %d\n", getpid());
                fprintf(stderr, "Executing command: %s\n", (*pCmdLine).arguments[0]);
            }
            execvp((*pCmdLine).arguments[0], (*pCmdLine).arguments);
            perror("ERROR: execvp() failed\n"); // If execvp returns, it means an error occurred, and the current process was not replaced. 
            _exit(1); //Used in situations where the process needs to terminate immediately without 
            //performing any cleanup, such as in the child process after a fork where performing such cleanup could be 
            //problematic or redundant.
        }
        else{
            if ((*pCmdLine).blocking) { // if blocking, wait for the child process to finish
                int status; //stores the exit status of the child process
                waitpid(p, &status, 0); // If blocking is false, the shell will not wait for the child process to finish.
            }
        }
    }

}

int main(int argc, char **argv){
    char cwd[PATH_MAX]; 
    char input[2048]; 
    cmdLine *parsedLine;
    for(int i=0; i<argc; i++){
        if(strcmp(argv[i], "-d") == 0){
            debug = 1;
        }
    }

    while(1){
        if(getcwd(cwd, sizeof(cwd))!=NULL){ // get current working directory
            printf("%s\n", cwd); 
            fflush(stdout);
        }
        else{
            perror("ERROR: getcwd() failed\n"); 
            return 1; 
        }
        if(fgets(input, 2048, stdin)!=NULL){ // read input from the user
            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0'; // remove newline character
            }
            if (strcmp(input, "quit") == 0) {
                    break;
            }
            parsedLine = parseCmdLines(input);
            if(parsedLine == NULL){
                perror("ERROR: parseCmdLines() failed\n");
                continue;
            }
            execute(parsedLine);
            freeCmdLines(parsedLine);
        }
        else{
            perror("ERROR: fgets() failed\n");
            printf("\n");
            return 1; 
        }    
    }
    return 0;
}