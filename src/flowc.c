#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

// Constants
#define MAX_NODES 100
#define MAX_PIPES 100
#define MAX_CONCATENATIONS 100
#define MAX_STDERR_CAPTURES 100
#define MAX_FILE_NODES 100
#define MAX_COMMAND_ARGS 20
#define MAX_COMMAND_LENGTH 1024
#define MAX_ACTION_NAME_LENGTH 100
#define MAX_PARTS 10

// Structs for nodes, pipes, concatenations, stderr captures, and file nodes
typedef struct {
    char name[MAX_ACTION_NAME_LENGTH];
    char *command_args[MAX_COMMAND_ARGS];
} Node;

typedef struct {
    char name[MAX_ACTION_NAME_LENGTH];
    char from[MAX_ACTION_NAME_LENGTH];
    char to[MAX_ACTION_NAME_LENGTH];
} FlowPipe;

typedef struct {
    char name[MAX_ACTION_NAME_LENGTH];
    int num_parts;
    char parts[MAX_PARTS][MAX_ACTION_NAME_LENGTH];
} Concatenation;

typedef struct {
    char name[MAX_ACTION_NAME_LENGTH];
    char from[MAX_ACTION_NAME_LENGTH];
} StderrCapture;

typedef struct {
    char name[MAX_ACTION_NAME_LENGTH];
    char filename[MAX_COMMAND_LENGTH];
} FileNode;

// Arrays for storing nodes, pipes, concatenations, stderr captures, and file nodes
Node nodes[MAX_NODES];
int num_nodes = 0;

FlowPipe pipes[MAX_PIPES];
int num_pipes = 0;

Concatenation concatenations[MAX_CONCATENATIONS];
int num_concatenations = 0;

StderrCapture stderr_captures[MAX_STDERR_CAPTURES];
int num_stderr_captures = 0;

FileNode file_nodes[MAX_FILE_NODES];
int num_file_nodes = 0;

// Function Declarations
void parse_flow_file(const char* filename);
void execute_action(const char *action_name, int output_fd, int suppress_errors);
void execute_node(const Node *node, int input_fd, int output_fd, int redirect_stderr);
void execute_pipe(const FlowPipe *pipe, int output_fd, int suppress_errors);
void execute_concatenation(const Concatenation *concat, int output_fd, int suppress_errors);
void execute_stderr_capture(const StderrCapture *stderr_capture, int output_fd);
void execute_file_node(const FileNode *file_node, int output_fd);
Node* find_node(const char *name);
FlowPipe* find_pipe(const char *name);
Concatenation* find_concatenation(const char *name);
StderrCapture* find_stderr_capture(const char *name);
FileNode* find_file_node(const char *name);
char** tokenize_command(char *command_line);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        write(STDERR_FILENO, "Usage: ./flow <file.flow> <action>\n", 35);
        return EXIT_FAILURE;
    }

    parse_flow_file(argv[1]);
    execute_action(argv[2], STDOUT_FILENO, 0);

    return EXIT_SUCCESS;
}

// Function Definitions

void parse_flow_file(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("[ERROR] Could not open flow file");
        exit(EXIT_FAILURE);
    }

    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
        perror("[ERROR] Could not read flow file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0';
    close(fd);

    char *line = strtok(buffer, "\n");
    while (line != NULL) {
        if (strncmp(line, "node=", 5) == 0) {
            Node node;
            strncpy(node.name, line + 5, MAX_ACTION_NAME_LENGTH);
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "command=", 8) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing command for node\n", 33);
                exit(EXIT_FAILURE);
            }
            char *command_line = line + 8;
            node.command_args[0] = strtok(command_line, " ");
            int arg_idx = 1;
            while (arg_idx < MAX_COMMAND_ARGS - 1 && (node.command_args[arg_idx] = strtok(NULL, " ")) != NULL) {
                arg_idx++;
            }
            node.command_args[arg_idx] = NULL;
            nodes[num_nodes++] = node;
        } else if (strncmp(line, "pipe=", 5) == 0) {
            FlowPipe pipe;
            strncpy(pipe.name, line + 5, MAX_ACTION_NAME_LENGTH);
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "from=", 5) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing 'from' for pipe\n", 32);
                exit(EXIT_FAILURE);
            }
            strncpy(pipe.from, line + 5, MAX_ACTION_NAME_LENGTH);
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "to=", 3) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing 'to' for pipe\n", 30);
                exit(EXIT_FAILURE);
            }
            strncpy(pipe.to, line + 3, MAX_ACTION_NAME_LENGTH);
            pipes[num_pipes++] = pipe;
        } else if (strncmp(line, "concatenate=", 12) == 0) {
            Concatenation concat;
            strncpy(concat.name, line + 12, MAX_ACTION_NAME_LENGTH);
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "parts=",6) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing 'parts' for concatenation\n", 43);
                exit(EXIT_FAILURE);
            }
            concat.num_parts = atoi(line + 6);
            for (int i = 0; i < concat.num_parts; i++) {
                line = strtok(NULL, "\n");
                if (line == NULL) {
                    write(STDERR_FILENO, "[ERROR] Missing 'part' for concatenation\n", 42);
                    exit(EXIT_FAILURE);
                }
                char *equal_sign = strchr(line, '=');
                if (equal_sign == NULL) {
                    write(STDERR_FILENO, "[ERROR] Invalid 'part' format\n", 30);
                    exit(EXIT_FAILURE);
                }
                strncpy(concat.parts[i], equal_sign + 1, MAX_ACTION_NAME_LENGTH);
            }
            concatenations[num_concatenations++] = concat;
        } else if (strncmp(line, "stderr", 6) == 0) {
            StderrCapture stderr_capture;
            if (line[6] == '=') {
                strncpy(stderr_capture.name, line + 7, MAX_ACTION_NAME_LENGTH);
            } else {
                strcpy(stderr_capture.name, "");
            }
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "from=", 5) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing 'from' for stderr\n", 33);
                exit(EXIT_FAILURE);
            }
            strncpy(stderr_capture.from, line + 5, MAX_ACTION_NAME_LENGTH);
            stderr_captures[num_stderr_captures++] = stderr_capture;
        } else if (strncmp(line, "file=", 5) == 0) {
            FileNode file_node;
            strncpy(file_node.name, line + 5, MAX_ACTION_NAME_LENGTH);
            line = strtok(NULL, "\n");
            if (line == NULL || strncmp(line, "name=", 5) != 0) {
                write(STDERR_FILENO, "[ERROR] Missing 'name' for file\n", 31);
                exit(EXIT_FAILURE);
            }
            strncpy(file_node.filename, line + 5, MAX_COMMAND_LENGTH);
            file_nodes[num_file_nodes++] = file_node;
        }
        line = strtok(NULL, "\n");
    }
}

void execute_action(const char *action_name, int output_fd, int suppress_errors) {
    Node *node = find_node(action_name);
    if (node != NULL) {
        execute_node(node, STDIN_FILENO, output_fd, suppress_errors);
        return;
    }

    FlowPipe *pipe = find_pipe(action_name);
    if (pipe != NULL) {
        execute_pipe(pipe, output_fd, suppress_errors);
        return;
    }

    Concatenation *concat = find_concatenation(action_name);
    if (concat != NULL) {
        execute_concatenation(concat, output_fd, suppress_errors);
        return;
    }

    StderrCapture *stderr_capture = find_stderr_capture(action_name);
    if (stderr_capture != NULL) {
        execute_stderr_capture(stderr_capture, output_fd);
        return;
    }

    FileNode *file_node = find_file_node(action_name);
    if (file_node != NULL) {
        execute_file_node(file_node, output_fd);
        return;
    }

    write(STDERR_FILENO, "[ERROR] Unknown action\n", 23);
    exit(EXIT_FAILURE);
}

void execute_node(const Node *node, int input_fd, int output_fd, int redirect_stderr) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        if (redirect_stderr) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }
        execvp(node->command_args[0], node->command_args);
        perror("[ERROR] execvp failed");
        exit(EXIT_FAILURE);
    }
    int status;
    waitpid(pid, &status, 0);
    if (!redirect_stderr && WIFEXITED(status) && WEXITSTATUS(status) != 0 && !redirect_stderr) {
        write(STDERR_FILENO, "[ERROR] Child process exited with error\n", 40);
    }
}

void execute_pipe(const FlowPipe *pipe, int output_fd, int suppress_errors) {
    int fd[2];
    if (pipe2(fd, O_CLOEXEC) == -1) {
        // If pipe2 is not available, use pipe and fcntl
        if (pipe(fd) == -1) {
            perror("[ERROR] pipe failed");
            exit(EXIT_FAILURE);
        }
        fcntl(fd[0], F_SETFD, FD_CLOEXEC);
        fcntl(fd[1], F_SETFD, FD_CLOEXEC);
    }

    pid_t pid_from = fork();
    if (pid_from == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid_from == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execute_action(pipe->from, STDOUT_FILENO, suppress_errors);
        exit(EXIT_FAILURE);
    }

    pid_t pid_to = fork();
    if (pid_to == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid_to == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execute_action(pipe->to, output_fd, suppress_errors);
        exit(EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid_from, NULL, 0);
    waitpid(pid_to, NULL, 0);
}

void execute_concatenation(const Concatenation *concat, int output_fd, int suppress_errors) {
    for (int i = 0; i < concat->num_parts; i++) {
        execute_action(concat->parts[i], output_fd, suppress_errors);
    }
}

void execute_stderr_capture(const StderrCapture *stderr_capture, int output_fd) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("[ERROR] pipe failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[0]);
        close(fd[1]);
        execute_action(stderr_capture->from, STDOUT_FILENO, 1);
        exit(EXIT_FAILURE);
    }

    close(fd[1]);
    execute_node(&(Node){"cat", {"cat", NULL}}, fd[0], output_fd, 0);
    close(fd[0]);
    waitpid(pid, NULL, 0);
}

void execute_file_node(const FileNode *file_node, int output_fd) {
    int fd = open(file_node->filename, O_RDONLY);
    if (fd == -1) {
        perror("[ERROR] open failed");
        exit(EXIT_FAILURE);
    }
    execute_node(&(Node){"cat", {"cat", NULL}}, fd, output_fd, 0);
    close(fd);
}

// Helper functions to find actions by name
Node* find_node(const char *name) {
    for (int i = 0; i < num_nodes; i++) {
        if (strcmp(nodes[i].name, name) == 0) {
            return &nodes[i];
        }
    }
    return NULL;
}

FlowPipe* find_pipe(const char *name) {
    for (int i = 0; i < num_pipes; i++) {
        if (strcmp(pipes[i].name, name) == 0) {
            return &pipes[i];
        }
    }
    return NULL;
}

Concatenation* find_concatenation(const char *name) {
    for (int i = 0; i < num_concatenations; i++) {
        if (strcmp(concatenations[i].name, name) == 0) {
            return &concatenations[i];
        }
    }
    return NULL;
}

StderrCapture* find_stderr_capture(const char *name) {
    for (int i = 0; i < num_stderr_captures; i++) {
        if (strcmp(stderr_captures[i].name, name) == 0) {
            return &stderr_captures[i];
        }
    }
    return NULL;
}

FileNode* find_file_node(const char *name) {
    for (int i = 0; i < num_file_nodes; i++) {
        if (strcmp(file_nodes[i].name, name) == 0) {
            return &file_nodes[i];
        }
    }
    return NULL;
}