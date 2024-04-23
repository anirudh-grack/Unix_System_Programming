#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>
#include <limits.h>
#define MAX_ARGS 5
#define MAX_COMMAND_LENGTH 100
#define MAX_FILES 6


pid_t back_processes[MAX_ARGS]; // Created an Array to store background processes
// -> run_command(char *args[], int argc, int bkgr) : Used for running the commands using execvp
void run_command(char *args[], int argc, int bkgr) {
    pid_t pid = fork();

    if (pid < 0) {
        // When fork fails
        perror("Error while forking in run_command");
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        // If execvp returns, it must have failed
        perror("Invalid Command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        if (!bkgr) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            // If its Background process
            printf("[%d] %d\n", back_processes[0], pid);
            back_processes[0] = pid;
        }
    }
}
// -> handle_tilde_2(char* path) : It is used to expand the ~ symbol to the user's home directory path
void handle_tilde_2(char* path) {
    char* get_path = (char*)malloc(PATH_MAX * sizeof(char));
    if (get_path == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    
    char* pos = path;
    char* result_pos = get_path;
    
    while (*pos != '\0') {
        if (*pos == '~') {
            char* get_home = getenv("HOME");
            if (get_home == NULL) {
                printf("Error with HOME in handle_tilde_2\n");
                free(get_path);
                return;
            }
            strcpy(result_pos, get_home);
            result_pos += strlen(get_home);
            pos++;
        } else {
            *result_pos = *pos;
            result_pos++;
            pos++;
        }
    }
    
    *result_pos = '\0'; // Used for Null-terminate the final string
    
    strcpy(path, get_path);
    free(get_path);
}
// -> handle_pipe_commands(char *actualcommand): Used for handling pipe symbol '|' between commands   
void handle_pipe_commands(char *actualcommand) {
    handle_tilde_2(actualcommand);
    int total_args = 0;
    char *commands_array[8]; // Maximum of 7 commands for piping
    char *token = strtok(actualcommand, "|");

    while (token != NULL && total_args < 8) {
        commands_array[total_args++] = token;
        token = strtok(NULL, "|");
    }
    // Check if total_args exceeds 7 (more than 6 pipes)
    if (total_args > 7) {
        printf("Exceeded maximum number of pipes allowed (6 pipes allowed)\n");
        return;
    }
    // Create pipes
    int pipes[total_args - 1][2];
    for (int i = 0; i < total_args - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Error while piping");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < total_args; i++) {
        char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
        int argc = 0;
        // Tokenize the command
        token = strtok(commands_array[i], " ");
        while (token != NULL && argc < MAX_ARGS + 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL; // NULL at the end of array
        if (argc>5){
        printf("Total arguments greater than 5 not allowed\n");
        return;
        }
        // Executeing commands
        pid_t pid = fork();
        if (pid == -1) {
            perror("Error while forking in handle_pipe_commands");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (i != 0) {
                // Redirecting input from the previous command
                dup2(pipes[i - 1][0], 0);
                close(pipes[i - 1][0]);
                close(pipes[i - 1][1]);
            }
            if (i != total_args - 1) {
                // Redirecting output to the next command
                dup2(pipes[i][1], 1);
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            // Closeing all pipes
            for (int j = 0; j < total_args - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Execute the command
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    // Closeing all pipes
    for (int i = 0; i < total_args - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // Waiting for all children to finish
    for (int i = 0; i < total_args; i++) {
        wait(NULL);
    }
}
// -> concatenate_files(char *file_list[]) : Helps to concatenate the files in the order in which they are listed and diplay the final result on the stdout
void concatenate_files(char *file_list[]) {
    int num_files = 0;
    while (file_list[num_files] != NULL) {
        num_files++;
    }
    // printf("numfiles is %d\n",num_files);
    // Check if the number of files exceeds the maximum allowed
    if (num_files > MAX_FILES) {
        printf("Exceeded maximum number of operators allowed (5 concatenation allowed)\n");
        return;
    }
    char *cat_command[MAX_FILES + 2]; // +2 for "cat" command and NULL terminator
    int i;
    cat_command[0] = "cat"; // "cat" command
    for (i = 0; file_list[i] != NULL; ++i) {
        cat_command[i + 1] = file_list[i]; // file names
    }
    cat_command[i + 1] = NULL; // NULL at the end of array
    run_command(cat_command, i + 1,0); // Executing the "cat" command
}
// -> handle_tilde(char *path): replaces the tilde (~) character with user's home directory path
void handle_tilde(char *path) {
    if (path[0] == '~') {
        const char *home_dir;
        if (path[1] == '/' || path[1] == '\0') {
            // expanding ~ or ~/ path
            home_dir = getenv("HOME");
            if (home_dir == NULL) {
                struct passwd *pw = getpwuid(getuid());
                if (pw != NULL)
                    home_dir = pw->pw_dir;
            }
        } else {
            // Otherwise
            char *username_end = strchr(path, '/');
            if (username_end == NULL)
                return;  // Invalid path
            *username_end = '\0';
            struct passwd *pw = getpwnam(path + 1);
            if (pw != NULL)
                home_dir = pw->pw_dir;
            else
                return;  // Invalid username
            *username_end = '/';
        }
        memmove(path + strlen(home_dir), path + 1, strlen(path + 1) + 1);
        memcpy(path, home_dir, strlen(home_dir));
    }
}
// -> parse_and_concatenate_files(char *command) :serves the purpose of parsing a command string containing filenames separated by the # symbol 
void parse_and_concatenate_files(char *command) {
        char *file_list[MAX_FILES + 1]; // +1 for NULL terminator
    int file_index = 0;
    char *token = strtok(command, "#");

    while (token != NULL) {
        // Trim leading and trailing spaces
        while (*token == ' ')
            ++token;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ')
            --end;
        end[1] = '\0';

        file_list[file_index++] = token;
        token = strtok(NULL, "#");
    }
    file_list[file_index] = NULL; // NULL at the end of array
    concatenate_files(file_list);
}
// -> handle_greater_symbol(char *command): For handling output redirection
void handle_greater_symbol(char *command) {
	// handle tilde 1st
    handle_tilde_2(command);
    // Parse command as well as file name
    char *cmd = strtok(command, ">");
    char *filename = strtok(NULL, ">");
    if (cmd == NULL || filename == NULL) {
        printf("Invalid command syntax\n");
        return;
    }
    // It is Triming leading and trailing spaces from filename
    while (*filename == ' ')
        ++filename;
    char *end = filename + strlen(filename) - 1;
    while (end > filename && *end == ' ')
        --end;
    end[1] = '\0';
    //handle_tilde(filename);
    // Tokenize the command
    char *token;
    char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
    int argc = 0;
    token = strtok(cmd, " ");
    while (token != NULL && argc < MAX_ARGS + 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    if (argc > MAX_ARGS) {
        printf("Arguments count should be >=1 and <=5\n");
        return;
    }
    args[argc] = NULL; // NULL at the end of array
    // printf("filename %s\n",filename);
    // Redirect output to the file
    int orig_stdout = dup(STDOUT_FILENO); // Save original stdout
    int fd = open(filename, O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
        printf("File '%s' not found\n", filename);
        return;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
    // Execute the command
    run_command(args, argc,0);
    // To restore standard output
    dup2(orig_stdout, STDOUT_FILENO);
    close(orig_stdout);
}
// -> handle_ggreater_symbol(char *command): For handling append redirection
void handle_ggreater_symbol(char *command) {
    // handle tilde 1st
    handle_tilde_2(command);
    // Parse command and file name
    char *cmd = strtok(command, ">>");
    char *filename = strtok(NULL, ">>");
    if (cmd == NULL || filename == NULL) {
        printf("Invalid command syntax\n");
        return;
    }
    // For Triming leading and trailing spaces from filename
    while (*filename == ' ')
        ++filename;
    char *end = filename + strlen(filename) - 1;
    while (end > filename && *end == ' ')
        --end;
    end[1] = '\0';
    // handle_tilde(filename);
    // Tokenize the command
    char *token;
    char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
    int argc = 0;
    token = strtok(cmd, " ");
    while (token != NULL && argc < MAX_ARGS + 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    if (argc > MAX_ARGS) {
        printf("Arguments count should be >=1 and <=5\n");
        return;
    }
    args[argc] = NULL; // NULL at the end of array
     //printf("filename %s\n",filename);
    // Redirect output to the file
    int orig_stdout = dup(STDOUT_FILENO); // Save original stdout
    int fd = open(filename, O_WRONLY | O_APPEND, 0666);
    if (fd == -1) {
        printf("File '%s' not found\n", filename);
        return;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
    // Execute the command
    run_command(args, argc,0);
    // Restore standard output
    dup2(orig_stdout, STDOUT_FILENO);
    close(orig_stdout);
}
// -> handle_less_symbol(char *command): For handling input redirection
void handle_less_symbol(char *command) {
    char *token;
    char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
    int argc = 0;
    token = strtok(command, "<");
    while (token != NULL && argc < MAX_ARGS + 1) {
        args[argc++] = token;
        token = strtok(NULL, "<");
    }
    if (argc != 2) {
        printf("Invalid syntax for redirection\n");
        return;
    }
    // Triming leading and trailing spaces in 2nd portion of the command (file name)
    char *file_name = args[1];
    while (*file_name == ' ')
        ++file_name;
    char *end = file_name + strlen(file_name) - 1;
    while (end > file_name && *end == ' ')
        --end;
    end[1] = '\0';
    handle_tilde(file_name);
    // Open the file for reading
    int input_fd = open(file_name, O_RDONLY);
    if (input_fd == -1) {
        perror("open");
        return;
    }
    // Save the current stdin file descriptor
    int stdin_saved = dup(STDIN_FILENO);
    if (stdin_saved == -1) {
        perror("dup");
        close(input_fd);
        return;
    }
    // Redirect stdin to the file
    if (dup2(input_fd, STDIN_FILENO) == -1) {
        perror("dup2");
        close(input_fd);
        close(stdin_saved);
        return;
    }
    // Close the file descriptor for the input file
    close(input_fd);
    // Tokenize the command before redirection
    argc = 0;
    token = strtok(args[0], " ");
    while (token != NULL && argc < MAX_ARGS + 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL; // NULL at the end of array
    if (argc > MAX_ARGS) {
        printf("Arguments count should be >=1 and <=5\n");
        return;
    }
    // Execute the command
    run_command(args, argc,0);
    // Restore the original stdin file descriptor
    if (dup2(stdin_saved, STDIN_FILENO) == -1) {
        perror("dup2");
    }
    close(stdin_saved);
}
// -> run_command_logical(char *command):  runs a command within a logical operation
void run_command_logical(char *command) {
    char *args[MAX_ARGS];
    char *token;
    int i = 0;
    token = strtok(command, " \n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
    if (args[0] != NULL) {
        if (execvp(args[0], args) == -1) {
            printf("Unable to execute command: %s\n", command);
            exit(EXIT_FAILURE);
        }
    }
}
// -> bring_to_foreground(): This function is used to bring the last background process into the foreground
void bring_to_foreground() {
    if (back_processes[0] != 0) {
        int status;
        // Wait for the last background process
        waitpid(back_processes[0], &status, 0);
        back_processes[0] = 0; // Reset the background process
    } else {
        printf("No background process to bring to foreground\n");
    }
}
// -> sequential_execution(char *command): Function executes multiple commands sequentially separated by semicolons (;)
void sequential_execution(char *command) {
	handle_tilde_2(command);
    char *token;
    char *commands[MAX_ARGS + 1]; // +1 for NULL terminator
    int command_index = 0;
    // Tokenize the command based on ';'
    token = strtok(command, ";");
    while (token != NULL && command_index < MAX_ARGS + 1) {
        // Triming leading and trailing spaces
        while (*token == ' ')
            ++token;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ')
            --end;
        end[1] = '\0';
        commands[command_index++] = token;
        token = strtok(NULL, ";");
    }
    commands[command_index] = NULL; // NULL at the end of array
        if (command_index >5)
        {
            printf("No of commands more than 5 not allowed\n",command_index);
            return;
        }
    // Execute each command sequentially
    for (int i = 0; i < command_index; ++i) {
        // Tokenize each command and execute it
        char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
        int argc = 0;
        token = strtok(commands[i], " ");
        while (token != NULL && argc < MAX_ARGS + 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
           }
        args[argc] = NULL; // NULL at the end of array
        // Execute the command
        if (argc >= 1 && argc <= MAX_ARGS) {
            run_command(args, argc, 0);
        } else {
            printf("Arguments count should be >=1 and <=5\n");

        }
    }
}
// -> handle_logical_operators(char *command): To handles logical operators (&& and ||) within a command
void handle_logical_operators(char *command) {
    char *token;
    char *remaining = command;
    int status;
    int last_status = 0;
    int skip_next = 0;

    while ((token = strtok_r(remaining,"&&\n", &remaining))) {
        if (strstr(token, "||") != NULL) {
            char *s_command;
            char *sub_remaining = token;
            int s_status = 1;
            while ((s_command = strtok_r(sub_remaining,"||\n", &sub_remaining))) {
                if (!skip_next) {
                    int pid = fork();
                    if (pid == -1) {
                        perror("Error while forking in handle_logical_operators || block");
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        run_command_logical(s_command);
                    } else {
                        wait(&status);
                        if (status == 0) {
                            s_status = 0;
                            break;
                        }
                    }
                } else 
                {
                    skip_next = 0;
                }
            }
            last_status = s_status;
        } else {
            if (!skip_next) {
                int pid = fork();
                if (pid == -1) {
                    perror("Error while forking in handle_logical_operators && block");
                    exit(EXIT_FAILURE);
                } else if (pid == 0) {
                    run_command_logical(token);
                } else 
                {
                    wait(&status);
                    if (status != 0) {
                        skip_next = 1;}
                }
            } else {
                skip_next = 0;
            }
            last_status = status;
        }
    }
}
// -> open_terminal(char *command): Function to open terminal with given command
int open_terminal(char *command) {
    pid_t pid = fork(); // Fork to detach the terminal process

    if (pid < 0) {
        // Fork failed
        perror("Error while forking in open_terminal");
        return 0; // Failure
    } else if (pid == 0) {
        // Child process
        // Try different terminal emulators
        if (execlp("gnome-terminal", "gnome-terminal", "--", command, NULL) != -1) {
            exit(EXIT_SUCCESS); // Success
        }
        if (execlp("xterm", "xterm", "-e", command, NULL) != -1) {
            exit(EXIT_SUCCESS); // Success
        }
        if (execlp("konsole", "konsole", "-e", command, NULL) != -1) {
            exit(EXIT_SUCCESS); // Success
        }
        exit(EXIT_FAILURE); // All attempts failed
    } else {
        // Parent process
        return 1; // Success
    }
}
int main() {
    while (1) {
         char command[MAX_COMMAND_LENGTH];
        printf("shell24$ ");
        fflush(stdout);
        // To read the input that user gives
        if (fgets(command, sizeof(command), stdin) == NULL) {
            // Error or EOF
            break;
        }
        // To remove trailing newline character
        command[strcspn(command, "\n")] = '\0';
        if (strlen(command) == 0) {
         //   printf("No command entered\n");
            continue;
        }
        // && and ||
        char *logical_pos = strstr(command, "&&");
        if (logical_pos != NULL) {
            handle_logical_operators(command);
            continue;
        }
        logical_pos = strstr(command, "||");
        if (logical_pos != NULL) {
            handle_logical_operators(command);
            continue;
        }
        // Check for ';' character
        char *semi_pos = strchr(command, ';');
        if (semi_pos != NULL) {
        sequential_execution(command);
        continue;
        }
        // Check for '|' character
        char *pipe_pos = strchr(command, '|');
        if (pipe_pos != NULL) {
            handle_pipe_commands(command);
            continue;
        }
        // Check for '#' character
        char *hash_pos = strstr(command, "#");
        if (hash_pos != NULL) {
            parse_and_concatenate_files(command);
            continue;
        }
        // Check for '<' character
        char *less_pos = strchr(command, '<');
        if (less_pos != NULL) {
            handle_less_symbol(command);
            continue;
        }
        // Check for '>>' character
        char *ggreater_pos = strstr(command, ">>");
        if (ggreater_pos != NULL)
        {
            handle_ggreater_symbol(command);
            continue;
        }
        // Check for '>' character
        char *greater_pos = strstr(command, ">");
        if (greater_pos != NULL)
        {
            handle_greater_symbol(command);
            continue;
        }
        // Check for background operator '&'
        int bkgr = 0;
        if (command[strlen(command) - 1] == '&') {
            bkgr = 1;
            command[strlen(command) - 1] = '\0'; // Remove the '&' character
        }
        // Checking for fg 
        if (strcmp(command, "fg") == 0) {
            bring_to_foreground();
        } else {
            // Tokenize the command
            char *token;
            char *args[MAX_ARGS + 2]; // +2 for command name plus NULL terminator
            int argc = 0;

            token = strtok(command, " ");
            while (token != NULL && argc < MAX_ARGS + 1) {
                args[argc++] = token;
                token = strtok(NULL, " ");
            }

            if (argc > MAX_ARGS) {
                printf("Agruments count should be >=1 and <=5\n");
                continue;
            }
        // to replace tilde in arguments
        for (int i = 1; i < argc; i++) {
            handle_tilde(args[i]);
        }
            args[argc] = NULL; // NULL at the end of array 

            // Rule 1: Check if the command is "newt"
            if (strcmp(args[0], "newt") == 0) {
                printf("Creating a new shell24 session\n");
                // Fork and run a new shell24 session
                if (!open_terminal("./Minishell")) {
                    printf("Failed to open terminal with command.\n");
                }
            } else {
                // Rule 2: Check argument count and run the command passed using run_command
                if (argc >= 1 && argc <= MAX_ARGS) {
                    run_command(args, argc, bkgr);
                } else {
                    printf("Arguments count should be >=1 and <=5\n");
                }
            }
        }
    }

    return 0;
}
