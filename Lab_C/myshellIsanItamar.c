#include "LineParser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <linux/wait.h>
#include <asm-generic/signal.h>

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process
{
    cmdLine *cmd;         /* the parsed command line*/
    pid_t pid;            /* the process id that is running the command*/
    int status;           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;

int debugmode = 0; // o for off, 1 for on
cmdLine *commandLine;
#define HISTLEN 20
char *history[HISTLEN];
int oldest = 0;
int newest = 0;
process *proclist = NULL;


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

void printProcessList(process **process_list)
{
    updateProcessList(process_list);
    printf("PID\t\tCommand\t\tSTATUS\n");
    for (process *currProcess = *process_list; currProcess != NULL; currProcess = currProcess->next)
    {
        printProcess(currProcess);
    }

    deleteTerminatedProcesses(process_list);
}


void freeProcessList(process *process_list)
{
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

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    process *newProcess = (process *)malloc(sizeof(process));
    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = RUNNING;
    newProcess->next = *process_list;
    proclist = newProcess;
}

char *findCommandHistory(int ind)
{
    if (ind < 0 || ind >= HISTLEN)
    {
        fprintf(stderr, "history command - Error");
        return NULL;
    }
    int currInd = oldest;
    while (currInd != ind && currInd < newest)
    { // find the index
        currInd = (currInd + 1) % HISTLEN;
    }
    if (currInd == newest)
    {
        fprintf(stderr, "history command - Error");
        return NULL;
    }
    printf("%s\n", history[currInd]);
    return history[currInd];
}


void pipeC(cmdLine* pCmdLine){
    int fileDescriptor[2];
    pid_t child1;
    pid_t child2;
    int p = pipe(fileDescriptor);
    if (p == -1){
        perror("pipe Error");
        exit(1);
    }
    child1 = fork();
    if (child1 == -1){
        perror("fork Error");
        exit(1);
    }
    
    if (child1 == 0){ 
        int inFd = -1;
        if (pCmdLine->inputRedirect != NULL){
            inFd = open(pCmdLine->inputRedirect, O_RDONLY);
            if (inFd == -1) {
                perror("Error Open");
                exit(1);
            }
            if (dup2(inFd, 0) == -1) {
                perror("Error dup2");
                exit(1);
            }
        }         
        close(STDOUT_FILENO);
        dup(fileDescriptor[1]);
        close(fileDescriptor[0]);
        close(fileDescriptor[1]);
        int execRes = execvp(pCmdLine->arguments[0] ,pCmdLine->arguments);
        if (inFd != -1){
            close(inFd);
        }
        if (execRes < 0){
            perror("Error execvp");
            exit(1);
        }
    }
    else {
        addProcess(&proclist, pCmdLine, child1);
        close(fileDescriptor[1]);
        child2 = fork();
        if (child2 == -1){
            perror("fork error");
            exit(1);
        }
         
        if (child2 != 0){ 
            int outFd = -1;
            cmdLine* nextCommand = pCmdLine->next;
            if (nextCommand->outputRedirect != NULL) {
                outFd = open(nextCommand->outputRedirect, O_CREAT|  O_WRONLY | O_TRUNC, 0777); 
                if (outFd == -1) {
                    perror("Error Open");
                    exit(1);
                }
                if (dup2(outFd, 1) == -1) {
                    perror("Error dup2");
                    exit(1);
                }
            }          
            close(STDIN_FILENO);
            dup(fileDescriptor[0]);
            close(fileDescriptor[0]);
            close(fileDescriptor[1]);
            int execRes = execvp(pCmdLine->next->arguments[0] ,pCmdLine->next->arguments);
            if (outFd != -1)
                close(outFd);
            if (execRes < 0){
                perror("Error execvp");
                exit(1);
            }
        }
        else {
            if(debugmode == 1){
                fprintf(stderr,"pid: %d, Executing cmd: %s\n", child2, pCmdLine->next->arguments[0]);
            }
            addProcess(&proclist, pCmdLine->next, child2);
            close(fileDescriptor[0]);

            waitpid (child1,NULL,0);
            waitpid (child2,NULL,0);
        }
    }
}

void freeHistoryArray()
{
    for (int i = oldest; i != newest; i = (i + 1) % HISTLEN)
    {
        free(history[i]);
    }
}

void quitCommand()
{
    freeCmdLines(commandLine);
    freeProcessList(proclist);
    freeHistoryArray();
    exit(0);
}

void cdCommand(cmdLine *pCmdLine)
{

    int cd = chdir(pCmdLine->arguments[1]);
    freeCmdLines(pCmdLine);
    if (cd == -1)
    {
        perror("Error: cd failed");
        return;
    }
}

void suspendCommand(cmdLine *pCmdLine)
{
    if (pCmdLine->arguments[1] != NULL)
    {

        pid_t pid = atoi(pCmdLine->arguments[1]);
        freeCmdLines(pCmdLine);
        if (kill(pid, SIGTSTP) == -1)
        {
            perror("kill");
            return;
        }
        printf("Looper handling SIGSTP\n");
        return;
    }
}

void wakeCommand(cmdLine *pCmdLine)
{
    if (pCmdLine->arguments[1] != NULL)
    {
        pid_t pid = atoi(pCmdLine->arguments[1]);
        freeCmdLines(pCmdLine);
        if (kill(pid, SIGCONT) == -1)
        {
            perror("kill");
            return;
        }
        printf("Looper handling SIGCONT\n");
        return;
    }
}

void killCommand(cmdLine *pCmdLine)
{
    if (pCmdLine->arguments[1] != NULL)
    {

        pid_t pid = atoi(pCmdLine->arguments[1]);
        freeCmdLines(pCmdLine);
        if (kill(pid, SIGINT) == -1)
        {
            perror("kill");
            return;
        }
        printf("Looper handling SIGINT\n");
    }
}

void procsCommand(cmdLine *pCmdLine)
{
    freeCmdLines(pCmdLine);
    printProcessList(&proclist);
}


void historyCommand(cmdLine *pCmdLine)
{
    int ind = 0;
    while (ind != newest)
    {
        printf("%d: %s\n", ind + 1, history[ind]);
        ind = (ind + 1) % HISTLEN;
    }
    freeCmdLines(pCmdLine);
}

void printCurrentDirectory()
{
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    fprintf(stdout, "the current directory is: %s\n", cwd);
}

void execute(cmdLine *pCmdLine)
{

    if (debugmode == 1)
    {
        fprintf(stderr, "command: %s\n", pCmdLine->arguments[0]);
        fprintf(stderr, "PID: %d\n", getpid());
    }

    if (pCmdLine->next != NULL)
    {
        if (pCmdLine->outputRedirect != NULL)
        {
            perror("error - redirection");
            exit(1);
        }
        if (pCmdLine->next->inputRedirect != NULL)
        {
            perror("error - redirection");
            exit(1);
        }

        pipeC(pCmdLine);
        return;
    }

    if (strcmp(pCmdLine->arguments[0], "quit") == 0)
    {
        quitCommand();
    }
    else if (strcmp(pCmdLine->arguments[0], "cd") == 0)
    {
        cdCommand(pCmdLine);
    }

    else if (strcmp(pCmdLine->arguments[0], "suspend") == 0)
    {

        suspendCommand(pCmdLine);
    }

    else if (strcmp(pCmdLine->arguments[0], "wake") == 0)
    {
        wakeCommand(pCmdLine);
    }
    else if (strcmp(pCmdLine->arguments[0], "kill") == 0)
    {
        killCommand(pCmdLine);
    }
    else if (strcmp(pCmdLine->arguments[0], "history") == 0)
    {
        historyCommand(pCmdLine);
    }
    else if (strcmp(pCmdLine->arguments[0], "procs") == 0)
    {
        procsCommand(pCmdLine);
    }
    else if (strcmp(pCmdLine->arguments[0], "!!") == 0)
    {
        int ind = 0;
        if (newest != 0)
        {
            ind = newest - 1;
        }
        else if (history[HISTLEN] != NULL)
        { 
            ind = HISTLEN;
        }
        freeCmdLines(pCmdLine);
        char *in = findCommandHistory(ind);
        if (in != NULL)
        {
            if (strncmp(in, "!", 1) != 0)
            {
                
                if (history[newest] != NULL)
                {
                    oldest = (oldest + 1) % HISTLEN;
                    free(history[newest]);
                }
                history[newest] = malloc(1 + strlen(in));
                if (history[newest] != NULL)
                {
                    strncpy(history[newest], in, 1 + strlen(in));
                    newest = (newest + 1) % HISTLEN;
                }
            }
            cmdLine *command = parseCmdLines(in);
            execute(command);
        }
    }
    else if (strncmp(pCmdLine->arguments[0], "!", 1) == 0)
    {
        char n = pCmdLine->arguments[0][1];
        int ind = n - '0' - 1;
        char *in = findCommandHistory(ind);
        freeCmdLines(pCmdLine);
        if (in != NULL)
        {
            if (strncmp(in, "!", 1) != 0)
            {
                
                if (history[newest] != NULL)
                {
                    oldest = (oldest + 1) % HISTLEN;
                    free(history[newest]);
                }
                history[newest] = malloc(1 + strlen(in));
                if (history[newest] != NULL)
                {
                    strncpy(history[newest], in, 1 + strlen(in));
                    newest = (newest + 1) % HISTLEN;
                }
            }
            cmdLine *command = parseCmdLines(in);
            execute(command);
        }
    }
    else
    {
        pid_t child = fork();
        if (child == 0)
        {
            int inputFileDescriptor = -1;
            int outputFileDescriptor = -1;
            if (pCmdLine->inputRedirect != NULL)
            {

                inputFileDescriptor = open(pCmdLine->inputRedirect, O_RDONLY);
                if (inputFileDescriptor == -1)
                {
                    perror("open failed - Error");
                    close(inputFileDescriptor);
                    exit(1);
                }
                if (dup2(inputFileDescriptor, 0) == -1)
                {
                    perror("dup2 failed - Error");
                    close(inputFileDescriptor);
                    exit(1);
                }
            }

            if (pCmdLine->outputRedirect != NULL)
            {
                outputFileDescriptor = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (outputFileDescriptor == -1)
                {
                    perror("open failed - Error");
                    close(outputFileDescriptor);
                    exit(1);
                }
                if (dup2(outputFileDescriptor, 1) == -1)
                {
                    perror("dup2 failed - Error");
                    close(outputFileDescriptor);
                    exit(1);
                }
            }

            int exe = execvp(pCmdLine->arguments[0], pCmdLine->arguments);
            if (inputFileDescriptor >= 1)
            {
                if (close(inputFileDescriptor) == -1)
                {
                    perror("close failed - Error");
                    exit(1);
                }
                if (close(outputFileDescriptor) == -1)
                {
                    perror("close failed - Error");
                    exit(1);
                }
            }

            if (exe == -1)
            {
                perror("execution failed - Error");

                _exit(1);
            }
        }
        else if (child == -1)
        {
            perror("fork failed - Error");
            exit(1);
        }
        else
        {
            addProcess(&proclist, pCmdLine, child);
        }

        if (pCmdLine->blocking == 1)
        {
            waitpid(child, NULL, 0);
        }
    }
}

int main(int argc, char const *argv[])
{
    char input[2048];
    FILE *inFile = stdin;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            debugmode = 1;
        }
    }

    while (1)
    {
        printCurrentDirectory();
        fgets(input, 2048, inFile);
        if (strncmp(input, "!", 1) != 0)
        { 
            
            if (history[newest] != NULL)
            { // queue is full
                oldest = (oldest + 1) % HISTLEN;
                free(history[newest]);
            }
            history[newest] = malloc(1 + strlen(input)); 
            if (history[newest] != NULL)
            {
                strncpy(history[newest], input, 1 + strlen(input));
                newest = (newest + 1) % HISTLEN;
            }
        }
        commandLine = parseCmdLines(input);
        execute(commandLine);
    }
}

