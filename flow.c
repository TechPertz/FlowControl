#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_COMMANDS 100
#define MAX_NODES 100
#define MAX_LEN 1024

// Structure for a node (command)
typedef struct {
    char *name;
    char *command[MAX_COMMANDS];
} Node;

// Structure for a pipe
typedef struct {
    char *from;
    char *to;
} FlowPipe;

// Structure for a concatenation
typedef struct {
    char *name;  // Name of the concatenation
    char *parts[MAX_NODES];
    int part_count;
} Concatenate;

// Global arrays to store nodes, pipes, and concatenations
Node nodes[MAX_NODES];
FlowPipe pipes[MAX_NODES];
Concatenate concatenations[MAX_NODES];
int node_count = 0, pipe_count = 0, concat_count = 0;

// Function to find a node by its name
int find_node_by_name(char *name) {
    printf("Debug: Entered find_node_by_name with name=%s\n", name);
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].name, name) == 0) {
            printf("Debug: Found node %s\n", name);
            return i;
        }
    }
    printf("Debug: Node %s not found\n", name);
    return -1;
}

void parse_node(char *line, FILE *flow_file) {
    printf("Debug: Entered parse_node with line=%s\n", line);
    char name[MAX_LEN], command[MAX_LEN];

    // Extract the node name
    sscanf(line, "node=%s", name);

    // Read the command line from the flow file (not stdin)
    if (fgets(line, MAX_LEN, flow_file) == NULL) {
        printf("Debug: Error reading command line\n");
        return;
    }
    sscanf(line, "command=%[^\n]", command);

    // Store the node information
    nodes[node_count].name = strdup(name);
    char *token = strtok(command, " ");
    int i = 0;
    while (token != NULL) {
        nodes[node_count].command[i++] = strdup(token);
        token = strtok(NULL, " ");
    }
    nodes[node_count].command[i] = NULL;
    printf("Debug: Parsed node %s with command %s\n", name, command);
    node_count++;
}

// Function to parse a pipe
void parse_pipe(char *line, FILE *flow_file) {
    printf("Debug: Entered parse_pipe with line=%s\n", line);
    char pipe_name[MAX_LEN], from[MAX_LEN], to[MAX_LEN];
    
    // Parse the pipe name (if necessary)
    if (sscanf(line, "pipe=%s", pipe_name) != 1) {
        printf("Debug: Error parsing pipe name: %s\n", line);
        return;
    }

    // Read the 'from' line
    if (fgets(line, MAX_LEN, flow_file) == NULL) {
        printf("Debug: Error reading 'from' line\n");
        return;
    }
    sscanf(line, "from=%s", from);

    // Read the 'to' line
    if (fgets(line, MAX_LEN, flow_file) == NULL) {
        printf("Debug: Error reading 'to' line\n");
        return;
    }
    sscanf(line, "to=%s", to);

    pipes[pipe_count].from = strdup(from);
    pipes[pipe_count].to = strdup(to);
    printf("Debug: Parsed pipe from %s to %s\n", from, to);
    pipe_count++;
}

// Function to parse a concatenation
void parse_concatenate(char *line, FILE *flow_file) {
    printf("Debug: Entered parse_concatenate with line=%s\n", line);

    int parts;
    char name[MAX_LEN];
    
    // Parse the concatenation name and the number of parts
    if (sscanf(line, "concatenate=%s parts=%d", name, &parts) != 2) {
        printf("Debug: Error parsing concatenate line: %s\n", line);
        return;
    }

    concatenations[concat_count].name = strdup(name);
    concatenations[concat_count].part_count = parts;
    
    // Loop to parse each part of the concatenation
    for (int i = 0; i < parts; i++) {
        if (fgets(line, MAX_LEN, flow_file) == NULL) {
            printf("Debug: Error reading part_%d line\n", i);
            return;
        }
        
        // Expect the format part_x=<node>
        char part[MAX_LEN];
        if (sscanf(line, "part_%d=%s", &i, part) != 2) {
            printf("Debug: Error parsing part_%d line: %s\n", i, line);
            return;
        }
        
        concatenations[concat_count].parts[i] = strdup(part);
        printf("Debug: Added part_%d: %s to concatenation %s\n", i, part, name);
    }
    
    printf("Debug: Parsed concatenation %s with %d parts\n", name, parts);
    concat_count++;
}


// Function to execute a single command
void execute_node(Node node) {
    printf("Debug: Entered execute_node for node %s with command %s\n", node.name, node.command[0]);
    pid_t pid = fork();
    if (pid == 0) { // Child process
        printf("Debug: Forked child process to exec %s\n", node.command[0]);
        execvp(node.command[0], node.command);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) { // Parent process
        printf("Debug: Waiting for child process %d to finish\n", pid);
        wait(NULL); // Wait for the child to finish
        printf("Debug: Child process finished\n");
    } else {
        perror("fork failed");
    }
}

// Function to execute pipes
void execute_pipe(FlowPipe flow_pipe) {
    printf("Debug: Entered execute_pipe from %s to %s\n", flow_pipe.from, flow_pipe.to);
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe failed");
        exit(1);
    }
    printf("Debug: Pipe created successfully\n");

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // First command: Redirect stdout to pipe
        printf("Debug: Forked first child process for pipe, from %s\n", flow_pipe.from);
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        int node_index = find_node_by_name(flow_pipe.from);
        if (node_index != -1) {
            printf("Debug: Executing first command %s\n", nodes[node_index].command[0]);
            execvp(nodes[node_index].command[0], nodes[node_index].command);
            perror("execvp failed");
            exit(1);
        } else {
            printf("Debug: Node %s not found for first command in pipe\n", flow_pipe.from);
            exit(1);
        }
    } else if (pid1 > 0) {
        printf("Debug: First process for pipe created with pid=%d\n", pid1);
    } else {
        perror("fork failed");
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Second command: Redirect stdin from pipe
        printf("Debug: Forked second child process for pipe, to %s\n", flow_pipe.to);
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        int node_index = find_node_by_name(flow_pipe.to);
        if (node_index != -1) {
            printf("Debug: Executing second command %s\n", nodes[node_index].command[0]);
            execvp(nodes[node_index].command[0], nodes[node_index].command);
            perror("execvp failed");
            exit(1);
        } else {
            printf("Debug: Node %s not found for second command in pipe\n", flow_pipe.to);
            exit(1);
        }
    } else if (pid2 > 0) {
        printf("Debug: Second process for pipe created with pid=%d\n", pid2);
    } else {
        perror("fork failed");
    }

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    wait(NULL);
    wait(NULL);
    printf("Debug: Pipe execution completed\n");
}

// Function to execute concatenations
void execute_concatenate(Concatenate concat) {
    printf("Debug: Entered execute_concatenate for concatenation %s with %d parts\n", concat.name, concat.part_count);
    for (int i = 0; i < concat.part_count; i++) {
        printf("Debug: Executing part %d: %s\n", i, concat.parts[i]);
        int node_index = find_node_by_name(concat.parts[i]);
        if (node_index != -1) {
            execute_node(nodes[node_index]);
        }
    }
}

// Function to parse the flow file and execute a specific action (pipe or concatenation)
void parse_and_execute_flow_file(FILE *flow_file, const char *target) {
    printf("Debug: Entered parse_and_execute_flow_file with target %s\n", target);
    char line[MAX_LEN];
    int found = 0;

    // Parse the flow file line by line
    while (fgets(line, MAX_LEN, flow_file) != NULL) {
        printf("Debug: Read line %s\n", line);
        if (strncmp(line, "node", 4) == 0) {
            parse_node(line, flow_file); // Pass the file pointer
        } else if (strncmp(line, "pipe", 4) == 0) {
            parse_pipe(line, flow_file); // Pass the file pointer
        } else if (strncmp(line, "concatenate", 11) == 0) {
            parse_concatenate(line, flow_file); // Pass the file pointer
        }
    }

    // Execute the specific pipe or concatenation based on the target
    printf("Debug: Looking for target %s in pipes and concatenations\n", target);
    for (int i = 0; i < pipe_count; i++) {
        if (strcmp(target, pipes[i].to) == 0 || strcmp(target, pipes[i].from) == 0) {
            printf("Debug: Found pipe to execute, from %s to %s\n", pipes[i].from, pipes[i].to);
            execute_pipe(pipes[i]); // Call the pipe execution logic
            found = 1;
            break;
        }
    }

    for (int i = 0; i < concat_count; i++) {
        if (strcmp(target, concatenations[i].name) == 0) {
            printf("Debug: Found concatenation to execute: %s\n", concatenations[i].name);
            execute_concatenate(concatenations[i]);
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Error: Could not find target %s in the flow file.\n", target);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    printf("Debug: Entered main\n");
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <flow_file> <target_pipe_or_concatenation>\n", argv[0]);
        exit(1);
    }

    printf("Debug: Opening flow file %s\n", argv[1]);
    FILE *flow_file = fopen(argv[1], "r");
    if (flow_file == NULL) {
        perror("fopen failed");
        exit(1);
    }

    parse_and_execute_flow_file(flow_file, argv[2]);
    fclose(flow_file);
    printf("Debug: Flow file execution completed\n");

    return 0;
}
