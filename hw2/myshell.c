#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

// Function to handle SIGINT in the shell
void sigint_handler(int sig) {
    // Do nothing, just return to prevent the shell from exiting on SIGINT
}

// Prepare function for initialization
int prepare(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls

    // Set SIGINT to be ignored by the shell but not by child processes
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    return 0;
}

// Process the command line arguments
int process_arglist(int count, char **arglist) {
    pid_t pid;
    int status;
    int bg_process = 0;
    int pipe_index = -1;
    int input_redirect_index = -1;
    int output_append_index = -1;

    // Check for background process
    if (count > 0 && strcmp(arglist[count - 1], "&") == 0) {
        bg_process = 1;
        arglist[count - 1] = NULL;  // Safely nullify the "&" to prevent passing it to execvp
        count--; // Decrease count to avoid out-of-bounds access
    }

    // Check for special symbols in the arglist
    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_index = i;
        } else if (strcmp(arglist[i], "<") == 0) {
            input_redirect_index = i;
        } else if (strcmp(arglist[i], ">>") == 0) {
            output_append_index = i;
        }
    }

    if (pipe_index != -1) {
        // Handle piping
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return 0;
        }

        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("fork");
            return 0;
        }

        if (pid1 == 0) { // Child process 1
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(pipefd[1]);

            arglist[pipe_index] = NULL;
            if (execvp(arglist[0], arglist) == -1) {
                perror("execvp");
                exit(1);
            }
        }

        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("fork");
            return 0;
        }

        if (pid2 == 0) { // Child process 2
            close(pipefd[1]); // Close unused write end
            dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from pipe
            close(pipefd[0]);

            if (execvp(arglist[pipe_index + 1], &arglist[pipe_index + 1]) == -1) {
                perror("execvp");
                exit(1);
            }
        }

        // Parent process
        close(pipefd[0]);
        close(pipefd[1]);

        // Wait for both children to finish
        waitpid(pid1, &status, 0);
        waitpid(pid2, &status, 0);

    } else if (input_redirect_index != -1) {
        // Handle input redirection
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return 0;
        }

        if (pid == 0) { // Child process
            int fd = open(arglist[input_redirect_index + 1], O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);

            arglist[input_redirect_index] = NULL;
            if (execvp(arglist[0], arglist) == -1) {
                perror("execvp");
                exit(1);
            }
        } else { // Parent process
            waitpid(pid, &status, 0);
        }

    } else if (output_append_index != -1) {
        // Handle output append redirection
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return 0;
        }

        if (pid == 0) { // Child process
            int fd = open(arglist[output_append_index + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);

            arglist[output_append_index] = NULL;
            if (execvp(arglist[0], arglist) == -1) {
                perror("execvp");
                exit(1);
            }
        } else { // Parent process
            waitpid(pid, &status, 0);
        }

    } else {
        // Handle regular commands and background processes
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return 0;
        }

        if (pid == 0) { // Child process
            if (execvp(arglist[0], arglist) == -1) {
                perror("execvp");
                exit(1);
            }
        } else { // Parent process
            if (!bg_process) {
                waitpid(pid, &status, 0);
            } else {
                // Don't wait for background process
                printf("Started background process with PID %d\n", pid);
            }
        }
    }

    return 1;
}

// Finalize function for cleanup
int finalize(void) {
    return 0;
}
