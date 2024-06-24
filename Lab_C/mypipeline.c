#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

void create_pipeline() {
    int pipefd[2]; // Array to hold the pipe file descriptors

    if (pipe(pipefd) == -1) { // Create the pipe
        perror("ERROR: pipe() failed");
        exit(EXIT_FAILURE);
    }

    pid_t child1 = fork(); // Fork the first child process
    if (child1 == -1) {
        perror("ERROR: fork() failed");
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) { // First child process
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO); // Close the standard output
        dup(pipefd[1]); // Duplicate the write end of the pipe to standard output
        close(pipefd[1]); // Close the duplicated write end
        close(pipefd[0]); // Close the read end of the pipe (not used)

        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        char *args[] = {"ls", "-l", NULL};
        execvp(args[0], args); // Execute "ls -l"
        perror("ERROR: execvp() failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1);
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]); // Close the write end of the pipe in the parent process

    pid_t child2 = fork(); // Fork the second child process
    if (child2 == -1) {
        perror("ERROR: fork() failed");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) { // Second child process
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO); // Close the standard input
        dup(pipefd[0]); // Duplicate the read end of the pipe to standard input
        close(pipefd[0]); // Close the duplicated read end

        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        char *args[] = {"tail", "-n", "2", NULL};
        execvp(args[0], args); // Execute "tail -n 2"
        perror("ERROR: execvp() failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(pipefd[0]); // Close the read end of the pipe in the parent process

    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0); // Wait for the first child process to finish
    waitpid(child2, NULL, 0); // Wait for the second child process to finish
    fprintf(stderr, "(parent_process>exiting...)\n");
}

int main() {
    fprintf(stderr, "(parent_process>forking...)\n");
    create_pipeline();
    return 0;
}
