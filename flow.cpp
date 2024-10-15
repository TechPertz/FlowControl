#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>

#define MAX_LEN 1024

// Struct for nodes, pipes, and concatenations
struct Node {
    std::string name;
    std::vector<std::string> command;
};

struct FlowPipe {
    std::string from;
    std::string to;
};

struct Concatenation {
    std::string name;
    std::vector<std::string> parts;
};

// Maps for storing nodes, pipes, and concatenations
std::map<std::string, Node> nodes;
std::map<std::string, FlowPipe> pipes;
std::map<std::string, Concatenation> concatenations;

// Function to execute a pipe
void execute_pipe(const std::string &pipe_name);

// Function to execute concatenations
void execute_concatenation(const std::string &concat_name);

// Function to find and execute nodes, pipes, or concatenations
void parse_and_execute_flow_file(std::ifstream &flow_file, const std::string &target);

// Helper function to tokenize command strings
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to parse and store a node
// Function to parse and store a node
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    // Extract node name
    std::istringstream iss(line);
    std::string node_str, equal_sign;
    iss >> node_str >> equal_sign >> name; // node=echo_foo
    std::getline(flow_file, command_line); // Read command line

    // Tokenize and store node command
    Node node;
    node.name = name;
    node.command = tokenize_command(command_line);

    nodes[name] = node;
    std::cout << "Debug: Parsed node " << name << " with command " << command_line << "\n";
}

// Function to parse and store a pipe
void parse_pipe(const std::string &line, std::ifstream &flow_file) {
    std::string pipe_name, from, to;

    // Extract pipe name
    std::istringstream iss(line);
    std::string pipe_str, equal_sign;
    iss >> pipe_str >> equal_sign >> pipe_name; // pipe=another_to_sed

    // Read 'from' and 'to' lines
    std::getline(flow_file, from);
    from = from.substr(from.find('=') + 1); // Extract 'from'
    
    std::getline(flow_file, to);
    to = to.substr(to.find('=') + 1); // Extract 'to'

    FlowPipe pipe;
    pipe.from = from;
    pipe.to = to;

    pipes[pipe_name] = pipe;
    std::cout << "Debug: Parsed pipe from " << from << " to " << to << "\n";
}

// Function to parse and store a concatenation
void parse_concatenation(const std::string &line, std::ifstream &flow_file) {
    std::string concat_name, parts_line;
    int part_count;

    // Extract concatenation name
    std::istringstream iss(line);
    std::string concat_str, equal_sign;
    iss >> concat_str >> equal_sign >> concat_name; // concatenate=group_commands

    std::getline(flow_file, parts_line);
    sscanf(parts_line.c_str(), "parts=%d", &part_count);

    Concatenation concat;
    concat.name = concat_name;

    // Parse individual parts
    for (int i = 0; i < part_count; ++i) {
        std::string part;
        std::getline(flow_file, part);
        part = part.substr(part.find('=') + 1); // Extract part name
        concat.parts.push_back(part);
        std::cout << "Debug: Added part " << concat.parts[i] << " to concatenation " << concat_name << "\n";
    }

    concatenations[concat_name] = concat;
    std::cout << "Debug: Parsed concatenation " << concat_name << " with " << part_count << " parts\n";
}

// Function to execute a single node
void execute_node(const std::string &node_name) {
    if (nodes.find(node_name) == nodes.end()) {
        std::cerr << "Error: Node " << node_name << " not found!\n";
        return;
    }

    Node &node = nodes[node_name];
    std::vector<char *> args;
    for (auto &arg : node.command) {
        args.push_back(&arg[0]);
    }
    args.push_back(nullptr);

    execvp(args[0], args.data());
    perror("execvp failed");
    exit(1);
}

// Function to execute a pipe
void execute_pipe(const std::string &pipe_name) {
    if (pipes.find(pipe_name) == pipes.end()) {
        std::cerr << "Error: Pipe " << pipe_name << " not found!\n";
        return;
    }

    FlowPipe &pipe = pipes[pipe_name];
    int pipe_fds[2];
    if (::pipe(pipe_fds) == -1) { // use the system call
        perror("pipe failed");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // First command: Redirect stdout to pipe
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        execute_node(pipe.from);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Second command: Redirect stdin from pipe
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        execute_node(pipe.to);
    }

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    wait(NULL);
    wait(NULL);
    std::cout << "Debug: Pipe execution completed\n";
}

// Function to execute a concatenation
void execute_concatenation(const std::string &concat_name) {
    if (concatenations.find(concat_name) == concatenations.end()) {
        std::cerr << "Error: Concatenation " << concat_name << " not found!\n";
        return;
    }

    Concatenation &concat = concatenations[concat_name];
    for (const std::string &part : concat.parts) {
        pid_t pid = fork();
        if (pid == 0) {
            execute_node(part);
        }
        wait(NULL);
    }
    std::cout << "Debug: Concatenation " << concat_name << " executed\n";
}

// Function to parse and execute the flow file
void parse_and_execute_flow_file(std::ifstream &flow_file, const std::string &target) {
    std::string line;

    // Parse the flow file line by line
    while (std::getline(flow_file, line)) {
        std::cout << "Debug: Read line " << line << "\n";
        if (line.find("node=") == 0) {
            parse_node(line, flow_file);
        } else if (line.find("pipe=") == 0) {
            parse_pipe(line, flow_file);
        } else if (line.find("concatenate=") == 0) {
            parse_concatenation(line, flow_file);
        }
    }

    // Execute based on the target
    if (pipes.find(target) != pipes.end()) {
        execute_pipe(target);
    } else if (concatenations.find(target) != concatenations.end()) {
        execute_concatenation(target);
    } else {
        std::cerr << "Error: Target " << target << " not found in pipes or concatenations!\n";
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <flow_file> <target>\n";
        exit(1);
    }

    std::ifstream flow_file(argv[1]);
    if (!flow_file.is_open()) {
        std::cerr << "Error: Could not open flow file " << argv[1] << "\n";
        exit(1);
    }

    std::string target = argv[2];
    std::cout << "Debug: Opening flow file " << argv[1] << "\n";
    parse_and_execute_flow_file(flow_file, target);

    return 0;
}