#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main() {
    int pipeFd[2]; // Array to hold the read and write file descriptors for the pipe - pipeFd[0] read, pipeFd[1] write
    pid_t c1pid, c2pid; // Process IDs for the child processes
    if (pipe(pipeFd) == -1) { // Create the pipe and store the file descriptors in pipeFd
        perror("ERROR: pipe() failed");
        exit(1);
    }
    fprintf(stderr, "(parent_process>forking...)\n");
    if ((c1pid = fork()) == -1) { // Fork the first child process
        perror("ERROR: fork() failed for child1");
        exit(1);
    }
    if (c1pid > 0) {
        fprintf(stderr, "(parent_process>created process with id: %d)\n", c1pid);
    }
    if (c1pid == 0) { // Child1 process
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO); // Close standard output
        dup(pipeFd[1]); // Duplicate the write end of the pipe to standard output
        close(pipeFd[1]); // Close the original write end of the pipe
        close(pipeFd[0]); // Close the read end of the pipe
        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        execlp("ls", "ls", "-l", NULL); // Execute "ls -l"
        perror("ERROR: execlp() failed for ls -l"); // If execlp returns, an error occurred
        exit(1);
    }
    close(pipeFd[1]); // Close the write end of the pipe
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    fprintf(stderr, "(parent_process>forking...)\n");
    if ((c2pid = fork()) == -1) { // Fork the second child process
        perror("ERROR: fork() failed for child2");
        exit(1);
    }
    if (c2pid == 0) { // Child2 process
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO); // Close standard input
        dup(pipeFd[0]); // Duplicate the read end of the pipe to standard input
        close(pipeFd[0]); // Close the original read end of the pipe
        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        execlp("tail", "tail", "-n", "2", NULL); // Execute "tail -n 2"
        perror("ERROR: execlp() failed for tail -n 2"); // If execlp returns, an error occurred
        exit(1);
    }
    close(pipeFd[0]); // Close the read end of the pipe
    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(c1pid, NULL, 0);
    waitpid(c2pid, NULL, 0);
    fprintf(stderr, "(parent_process>exiting...)\n");
    return 0;
}
