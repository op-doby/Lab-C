#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main() {
    int pipe_fd[2]; // Array to hold the read and write file descriptors for the pipe
    pid_t child1_pid, child2_pid;

    if (pipe(pipe_fd) == -1) { // Create a pipe
        perror("ERROR: pipe() failed");
        exit(1);
    }

    fprintf(stderr, "(parent_process>forking...)\n");

    if ((child1_pid = fork()) == -1) { // Fork the first child process
        perror("ERROR: fork() failed for child1");
        exit(1);
    }

    if (child1_pid == 0) { // Child1 process
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO); // Close standard output
        dup(pipe_fd[1]); // Duplicate the write end of the pipe to standard output
        close(pipe_fd[1]); // Close the original write end of the pipe
        close(pipe_fd[0]); // Close the read end of the pipe

        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        execlp("ls", "ls", "-l", NULL); // Execute "ls -l"
        perror("ERROR: execlp() failed for ls -l"); // If execlp returns, an error occurred
        exit(1);
    }

    // Parent process
    close(pipe_fd[1]); // Close the write end of the pipe
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");

    fprintf(stderr, "(parent_process>forking...)\n");

    if ((child2_pid = fork()) == -1) { // Fork the second child process
        perror("ERROR: fork() failed for child2");
        exit(1);
    }

    if (child2_pid == 0) { // Child2 process
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO); // Close standard input
        dup(pipe_fd[0]); // Duplicate the read end of the pipe to standard input
        close(pipe_fd[0]); // Close the original read end of the pipe

        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        execlp("tail", "tail", "-n", "2", NULL); // Execute "tail -n 2"
        perror("ERROR: execlp() failed for tail -n 2"); // If execlp returns, an error occurred
        exit(1);
    }

    // Parent process
    close(pipe_fd[0]); // Close the read end of the pipe
    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");

    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");

    // Wait for both child processes to terminate
    waitpid(child1_pid, NULL, 0);
    waitpid(child2_pid, NULL, 0);

    fprintf(stderr, "(parent_process>exiting...)\n");

    return 0;
}
