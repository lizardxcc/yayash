#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


enum token_type {
	WORD,
	PIPE,
	TWO_GREATER,
	LESS,
	GRETER,
};
struct token {
	enum token_type type;
	char *string;
};
struct token tokens[512];
size_t tokens_size = 0;

bool consume(const char *expected, const char *s)
{
	if (strncmp(expected, s, strlen(expected)) == 0)
			return true;
	return false;
}

void mylex(const char *s)
{
	size_t len = strlen(s);
	size_t i = 0;
	for (i = 0; i < len; ) {
		if (s[i] == ' ' ||
				s[i] == '\t' ||
				s[i] == '\n') {
			i++;
			continue;
		}

		if (consume("|", &s[i])) {
			i++;
			tokens[tokens_size].type = PIPE;
			tokens_size++;
			continue;
		} else if (consume(">", &s[i])) {
			i+=1;
			tokens[tokens_size].type = GRETER;
			tokens_size++;
			continue;
		} else if (consume("2>", &s[i])) {
			i+=2;
			tokens[tokens_size].type = TWO_GREATER;
			tokens_size++;
			continue;
		} else if (consume("<", &s[i])) {
			i+=1;
			tokens[tokens_size].type = LESS;
			tokens_size++;
			continue;
		}
		if (i == len-1) {
			tokens[tokens_size].type = WORD;
			tokens[tokens_size].string = strndup(&s[i], 1);
			tokens_size++;
			i+=(1);
			return;
		}
		size_t j;
		for (j = 1; i+j < len; j++) {
			if (s[i+j] == ' ') {
				tokens[tokens_size].string = strndup(&s[i], j);
				tokens[tokens_size].type = WORD;
				tokens_size++;
				i+=j;
				break;
			} else if (i+j == len-1) {
				tokens[tokens_size].type = WORD;
				tokens[tokens_size].string = strndup(&s[i], j+1);
				tokens_size++;
				i+=(j+1);
				return;
			}
		}
	}
}


void print_error(const char *s)
{
	fprintf(stderr, "An error occured: %s\n", s);
}


struct command {
	char **args;
	size_t args_size;
	char *stdin_redirect;
	char *stdout_redirect;
	char *stderr_redirect;
};


void init_command(struct command *command)
{
	command->args = NULL;
	command->args_size = 0;
	command->stdin_redirect = NULL;
	command->stdout_redirect = NULL;
	command->stderr_redirect = NULL;
}

int create_subprocess(struct command *command, int stdin_pipe, bool last_command_flag, pid_t *child_pid);


struct command_process_info {
	pid_t *pids;
	size_t process_size;
	char *command;
};

void init_process_info(struct command_process_info *info)
{
	info->pids = NULL;
	info->process_size = 0;
	info->command = NULL;
}
void add_process(struct command_process_info *info, pid_t pid)
{
	info->process_size++;
	info->pids = realloc(info->pids, sizeof(pid_t) * info->process_size);
	info->pids[info->process_size-1] = pid;
}
struct command_process_info *cur_processes = NULL;
struct command_process_info **processes_stack = NULL; // jobs
size_t processes_stack_size = 0;
void add_bg_jobs(struct command_process_info *info)
{
	processes_stack_size++;
	processes_stack = realloc(processes_stack, sizeof(struct command_process_info *) * processes_stack_size);
	processes_stack[processes_stack_size-1] = info;
}
void signal_processes(int signal, const struct command_process_info *info)
{
	for (size_t i = 0; i < info->process_size; i++) {
		kill(info->pids[i], signal);
	}
}



int main(void)
{
	printf("Welcome to yayash\n");

	char buf[256];
	while (1) {
		if (cur_processes != NULL) {
			free(cur_processes);
			cur_processes = NULL;
		}
		cur_processes = malloc(sizeof(struct command_process_info));
		init_process_info(cur_processes);
		printf("yayash: ");
		if (fgets(buf, 256, stdin) == NULL) {
			if (feof(stdin) != 0) {
				break;
			}
			if (ferror(stdin) != 0) {
			}
			putchar('\n');
			continue;
		}
		if (strlen(buf) != 0)
			buf[strlen(buf)-1] = '\0'; // delete newline

		// parse line
		tokens_size = 0;
		mylex(buf);

		struct command *commands = malloc(sizeof(struct command));
		init_command(&commands[0]);
		size_t commands_size = 1;
		for (size_t i = 0; i < tokens_size; i++) {
			switch (tokens[i].type) {
				case PIPE:
					commands_size++;
					commands = realloc(commands, sizeof(struct command) * commands_size);
					init_command(&commands[commands_size-1]);
					break;
				case GRETER:
					if (i == tokens_size-1) {
						print_error("Expected a file name after \'>\'");
						goto shell_error_end;
					}
					if (tokens[i+1].type != WORD) {
						print_error("Expected a file name after \'>\'");
						goto shell_error_end;
					}
					commands[commands_size-1].stdout_redirect = strdup(tokens[i+1].string);
					//printf("debug: stdout_redirect: %s\n", stdout_redirect);
					i++;
					break;
				case TWO_GREATER:
					if (i == tokens_size-1) {
						print_error("Expected a file name after \'2>\'");
						goto shell_error_end;
					}
					if (tokens[i+1].type != WORD) {
						print_error("Expected a file name after \'2>\'");
						goto shell_error_end;
					}
					commands[commands_size-1].stderr_redirect = strdup(tokens[i+1].string);
					i++;
					break;
				case LESS:
					if (i == tokens_size-1) {
						print_error("Expected a file name after \'<\'");
						goto shell_error_end;
					}
					if (tokens[i+1].type != WORD) {
						print_error("Expected a file name after \'<\'");
						goto shell_error_end;
					}
					commands[commands_size-1].stdin_redirect = strdup(tokens[i+1].string);
					i++;
					break;
				case WORD:
					commands[commands_size-1].args_size++;
					commands[commands_size-1].args = realloc(commands[commands_size-1].args, sizeof(char *) * commands[commands_size-1].args_size);
					commands[commands_size-1].args[commands[commands_size-1].args_size-1] = tokens[i].string;
					break;
			}
		}

		if (commands_size == 1 && commands[0].args_size == 0)
			continue;

		for (size_t i = 0; i < commands_size; i++) {
			commands[i].args_size++;
			commands[i].args = realloc(commands[i].args, sizeof(char *) * commands[i].args_size);
			commands[i].args[commands[i].args_size-1] = NULL;
		}

		int fd;
		cur_processes->command = strdup(buf);
		for (size_t i = 0; i < commands_size; i++) {
			pid_t cur_pid;

			if (i == 0) {
				if (commands_size == 1) {
					create_subprocess(&commands[0], -1, true, &cur_pid);
				} else {
					fd = create_subprocess(&commands[0], -1, false, &cur_pid);
					if (fd < 0)
						break;
				}
			} else if (i == commands_size-1) {
				create_subprocess(&commands[i], fd, true, &cur_pid);
			} else {
				fd = create_subprocess(&commands[i], fd, false, &cur_pid);
				if (fd < 0)
					break;
			}
			add_process(cur_processes, cur_pid);

		}
		int status;
		waitpid(cur_processes->pids[cur_processes->process_size-1], &status, 0);
		free(cur_processes);
		cur_processes = NULL;


shell_error_end:
		;

	}


	return 0;
}



int create_subprocess(struct command *command, int stdin_pipe, bool last_command_flag, pid_t *child_pid)
{
	int fd[2];
	pipe(fd);
	pid_t pid = fork();
	if (pid < 0) { // error
	} else if (pid == 0) { // child
		if (stdin_pipe >= 0)
			dup2(stdin_pipe, 0);
		close(fd[0]);
		if (last_command_flag)
			close(fd[1]);
		else
			dup2(fd[1], 1);
		if (command->stdin_redirect != NULL) {
			close(0);
			if (open(command->stdin_redirect, O_RDONLY) < 0) {
				perror("An error has occured in open()");
				return -1;
			}
		}
		if (command->stdout_redirect != NULL) {
			close(1);
			if (open(command->stdout_redirect, O_WRONLY | O_CREAT, 0666) < 0) {
				perror("An error has occured in open()");
				return -1;
			}
		}
		if (command->stderr_redirect != NULL) {
			close(2);
			if (open(command->stderr_redirect, O_WRONLY | O_CREAT, 0666) < 0) {
				perror("An error has occured in open()");
				return -1;
			}
		}
		if (execvp(command->args[0], command->args) < 0) {
			perror("execvp: ");
			exit(EXIT_FAILURE);
		}
	} else { // parent
		*child_pid = pid;
		close(fd[1]);
		if (last_command_flag) {
			close(fd[0]);
		}
	}
	if (last_command_flag) {
		return -1;
	}

	return fd[0];
}
