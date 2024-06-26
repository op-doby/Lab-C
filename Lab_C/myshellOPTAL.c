#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include "LineParser.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/wait.h>

#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0

int debug = 0;

typedef struct process{
    cmdLine* cmd;            /* the parsed command line*/
    pid_t pid; 		         /* the process id that is running the command*/
    int status;              /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;	 /* next process in chain */
} process;


process *processList = NULL; 

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process *new_process = (process*)malloc(sizeof(process));
    if (!new_process) {
        perror("ERROR: malloc() failed");
        exit(1);
    }
    
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = *process_list;
    *process_list = new_process;
}

void printProcess(process *Process)
{
    if ((Process) && ((Process->cmd->argCount) > 0))
    {
        char command[200] = "";

        
        for (int i = 0; i < (Process->cmd->argCount); ++i)
        {
            if (i > 0)
            {
                strcat(command, " ");
            }
            strcat(command, Process->cmd->arguments[i]);
        }

        const char *statusString;
        if (Process->status == TERMINATED)
        {
            statusString = "Terminated";
        }
        else if (Process->status == RUNNING)
        {
            statusString = "Running";
        }
        else
        {
            statusString = "Suspended";
        }

        printf("%d\t\t%s\t\t%s\n", Process->pid, command, statusString);
    }
}

void freeProcess(process *process)
{
    if (process)
    {
        if (process->cmd)
        {
            process->cmd->next = NULL;
            freeCmdLines(process->cmd);
        }
        free(process);
        process = NULL;
    }
}

void deleteTerminatedProcesses(process **process_list)
{
    process **indirect = process_list; 
    process *curr_process;

    while ((curr_process = *indirect) != NULL)
    {
        if (curr_process->status == TERMINATED)
        {
            *indirect = curr_process->next;
            freeProcess(curr_process);
        }
        else
        {
            indirect = &curr_process->next;
        }
    }
}
void updateProcessStatus(process *process_list, int pid, int status)
{
    process *currProcess = process_list;
    while (currProcess != NULL)
    {
        if (currProcess->pid == pid)
        {
            currProcess->status = status;
            break;
        }
        currProcess = currProcess->next;
    }
}

void updateProcessList(process **process_list) {
    int updateThis = 0;
    process *curr;
    for (curr = *process_list; curr != NULL; curr = curr->next) {
        int res = waitpid(curr->pid, &updateThis, WCONTINUED | WNOHANG | WUNTRACED );
        if (res == 0) {
            updateProcessStatus(curr, curr->pid, RUNNING);
        } else if (res == -1) {
            updateProcessStatus(curr, curr->pid, TERMINATED);
        } else {
            if (WIFEXITED(updateThis) || WIFSIGNALED(updateThis)) {
                updateProcessStatus(curr, curr->pid, TERMINATED);
            } 
            else if (WIFCONTINUED(updateThis)) {
                updateProcessStatus(curr, curr->pid, RUNNING);
            }
            else if (WIFSTOPPED(updateThis)) {
                updateProcessStatus(curr, curr->pid, SUSPENDED);
            } 
        }
    }
}
void deleteTerminatedProcesses(process **process_list)
{
    process **indirect = process_list; 
    process *curr_process;
    while ((curr_process = *indirect) != NULL)
    {
        if (curr_process->status == TERMINATED)
        {
            *indirect = curr_process->next;
            freeProcess(curr_process);
        }
        else
        {
            indirect = &curr_process->next;
        }
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);
    printf("PID\t\tCommand\t\tSTATUS\n");
    for (process *currProcess = *process_list; currProcess != NULL; currProcess = currProcess->next)
    {
        printProcess(currProcess);
    }

    deleteTerminatedProcesses(process_list);
}

/* Free all memory allocated for the process list */
void freeProcessList(process** process_list) {
     process *curr_process = process_list;
    process *next_process;

    // Iterate through the process list using a while loop
    while (curr_process != NULL)
    {
        next_process = curr_process->next; // Save the next process in the list
        curr_process->cmd->next = NULL;    // Detach the command from the list
        freeCmdLines(curr_process->cmd);   // Free the command line
        free(curr_process);                // Free the current process
        curr_process = next_process;       // Move on to the next process in the list
    }
}


void executeCommand(cmdLine *pCmdLine) {
    // Handle input redirection
    if (pCmdLine->inputRedirect != NULL) {
        int input_fd = open(pCmdLine->inputRedirect, O_RDONLY);
        if (input_fd == -1) {
            perror("ERROR: open() failed for input redirection");
            exit(1);
        }
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("ERROR: dup2() failed for input redirection");
            exit(1);
        }
        close(input_fd);
    }
    
    // Handle output redirection
    if (pCmdLine->outputRedirect != NULL) {
        int output_fd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd == -1) {
            perror("ERROR: open() failed for output redirection");
            exit(1);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("ERROR: dup2() failed for output redirection");
            exit(1);
        }
        close(output_fd);
    }
    
    // Execute the command
    if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
        processList->status = TERMINATED;
        perror("ERROR: execvp() failed");
        exit(1);
    }
}

void execute(cmdLine *pCmdLine) {
    if (strcmp(pCmdLine->arguments[0], "cd") == 0) { 
        if (chdir(pCmdLine->arguments[1]) == -1) {  // arguments[1] is the path to the directory
            fprintf(stderr, "cd: No such file or directory\n");
        }
    }
    // Arguments[0] is the command name
    else if (strcmp(pCmdLine->arguments[0], "alarm") == 0) { // Check if the command is "alarm"
        if(pCmdLine->arguments[1] == NULL) {
            fprintf(stderr, "ERROR: No PID provided\n");
        }
        else {
            pid_t pid = atoi(pCmdLine->arguments[1]); 
            if (kill(pid, SIGCONT) == 0) { // Send SIGCONT signal to wake up the process
                printf("Process %d woke up\n", pid);
            } else {
                perror("ERROR: kill()"); 
            }
        }
    } else if (strcmp(pCmdLine->arguments[0], "blast") == 0) { // Check if the command is "blast"
        if(pCmdLine->arguments[1] == NULL) {
            fprintf(stderr, "ERROR: No PID provided\n");
        }
        else {
            pid_t pid = atoi(pCmdLine->arguments[1]); 
            if (kill(pid, SIGKILL) == 0) { // Send SIGKILL signal to terminate the process
                printf("Process %d terminated\n", pid);
            } else {
                perror("ERROR: kill() failed\n"); 
            } 
        }
    }
    else if (pCmdLine->next != NULL) { // Check if the command is a pipe 
        int pipeFd[2]; // Array to hold the read and write file descriptors for the pipe - pipeFd[0] read, pipeFd[1] write
        pid_t c1pid, c2pid; // Process IDs for the child processes
        if (pipe(pipeFd) == -1) { // Create the pipe and store the file descriptors in pipeFd
            perror("ERROR: pipe() failed");
            exit(1);
        }
        
        // Check for invalid redirections
        if (pCmdLine->outputRedirect != NULL) {
            fprintf(stderr, "ERROR: Output redirection for the first command is invalid\n");
            return;
        }
        if (pCmdLine->next->inputRedirect != NULL) {
            fprintf(stderr, "ERROR: Input redirection for the second command is invalid\n");
            return;
        }
        fprintf(stderr, "(parent_process>forking...)\n");
        
        // Fork the first child process
        if ((c1pid = fork()) == -1) { 
            perror("ERROR: fork() failed for child1");
            exit(1);
        }
        if (c1pid == 0) { // Child1 process
            fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
            close(STDOUT_FILENO); // Close standard output
            if (dup2(pipeFd[1], STDOUT_FILENO) == -1) { // Duplicate the write end of the pipe to standard output
                perror("ERROR: dup2() failed for child1");
                exit(1);
            }
            close(pipeFd[1]); // Close the original write end of the pipe
            close(pipeFd[0]); // Close the read end of the pipe
            fprintf(stderr, "(child1>going to execute the first cmd)\n");
            executeCommand(pCmdLine);
            exit(1);
        }
        addProcess(&processList, pCmdLine, c1pid);
        // Parent process
        close(pipeFd[1]); // Close the write end of the pipe
        fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
        fprintf(stderr, "(parent_process>forking...)\n");
        // Fork the second child process
        if ((c2pid = fork()) == -1) { 
            perror("ERROR: fork() failed for child2");
            exit(1);
        }
        if (c2pid == 0) { // Child2 process
            fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
            close(STDIN_FILENO); // Close standard input
            if (dup2(pipeFd[0], STDIN_FILENO) == -1) { // Duplicate the read end of the pipe to standard input
                perror("ERROR: dup2() failed for child2");
                exit(1);
            }
            close(pipeFd[0]); // Close the original read end of the pipe
            fprintf(stderr, "(child2>going to execute cmd: )\n");
            executeCommand(pCmdLine->next);
            exit(1);
        }
        addProcess(&processList, pCmdLine, c2pid);
        // Parent process
        close(pipeFd[0]); // Close the read end of the pipe
        fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
        fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
        waitpid(c1pid, NULL, 0);
        waitpid(c2pid, NULL, 0);
        fprintf(stderr, "(parent_process>exiting...)\n");
    }
    // Check for the "procs" command
    else if (strcmp(pCmdLine->arguments[0], "procs") == 0) {
        freeCmdLines(pCmdLine); /////// ???????????
        printProcessList(&processList);
    }
    else {
        pid_t p = fork();
        if (p == -1) {
            perror("ERROR: fork() failed\n");
            exit(1);
        }
        if (p == 0) {
            executeCommand(pCmdLine);
        } else {
            addProcess(&processList, pCmdLine, p);
            if ((*pCmdLine).blocking) { // if blocking, wait for the child process to finish
                int status; // stores the exit status of the child process
                waitpid(p, &status, 0); // If blocking is false, the shell will not wait for the child process to finish.
                process *current = processList;
                while (current != NULL) {
                    if (current->pid == p) {
                        current->status = TERMINATED;
                        break;
                    }
                    current = current->next;
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    char cwd[PATH_MAX]; 
    char input[2048]; 
    cmdLine *parsedLine;
    //process *processList = NULL; 
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
        }
    }

    while (1) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) { // get current working directory
            printf("%s\n", cwd); 
            fflush(stdout);
        } else {
            perror("ERROR: getcwd() failed\n"); 
            return 1; 
        }
        if (fgets(input, 2048, stdin) != NULL) { // read input from the user
            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0'; // remove newline character
            }
            if (strcmp(input, "quit") == 0) {
                break;
            }
            parsedLine = parseCmdLines(input);
            if (parsedLine == NULL) {
                perror("ERROR: parseCmdLines() failed\n");
                continue;
            }
            execute(parsedLine);
            //freeCmdLines(parsedLine); /////////////////to check
        } else {
            perror("ERROR: fgets() failed\n");
            printf("\n");
            return 1; 
        }    
    }
    freeCmdLines(parsedLine);
    freeProcessList(processList);
    return 0;
}


