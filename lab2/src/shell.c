#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
	if (p->in_file){
		int fd = open(p->in_file, O_RDONLY);
		if (fd == -1){
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
	if (p->out_file){
		int fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd == -1){
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
	pid_t pid = fork();
	if (pid == 0) {
		// Child process
		redirection(p);

		if (execvp(p->args[0], p->args) == -1) {
			perror("execvp");
			exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		// Error forking
		perror("fork");
		return -1;
	} else {
		// Parent process
		int status;
		waitpid(pid, &status, 0);
		return 1;
	}
	return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
    struct cmd_node *current = cmd->head;
    int in_fd = STDIN_FILENO; // Initially, the input comes from standard input

    while (current != NULL) {
        int pipefd[2];
        
        // Only create a pipe if there's another command following
        if (current->next != NULL) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (in_fd != STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO); // Redirect input
                close(in_fd);
            }
            
            if (current->next != NULL) {
                dup2(pipefd[1], STDOUT_FILENO); // Redirect output to the next command
                close(pipefd[1]);
                close(pipefd[0]);
            }

            // Execute the current command
            spawn_proc(current);
            exit(EXIT_FAILURE); // Exit if spawn_proc fails
        } 
        else if (pid < 0) {
            // Fork failed
            perror("fork");
            exit(EXIT_FAILURE);
        } 
        else {
            // Parent process
            wait(NULL); // Optionally wait for each child process

            if (in_fd != STDIN_FILENO) {
                close(in_fd); // Close the previous input file descriptor
            }
            
            if (current->next != NULL) {
                close(pipefd[1]); // Close the write end of the current pipe
            }

            in_fd = pipefd[0]; // Save the read end for the next command
            current = current->next; // Move to the next command
        }
    }

    return 1;
}

// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
