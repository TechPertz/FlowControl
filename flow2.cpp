#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

#define MAX_LEN 1024

// Structs for nodes, pipes, and concatenations
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
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    std::string prefix = "node=";
    if (line.substr(0, prefix.length()) == prefix) {
        name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid node format: " << line << "\n";
        return;
    }

    std::getline(flow_file, command_line);

    std::string command_prefix = "command=";
    if (command_line.substr(0, command_prefix.length()) == command_prefix) {
        command_line = command_line.substr(command_prefix.length());
    }

    Node node;
    node.name = name;
    node.command = tokenize_command(command_line);

    nodes[name] = node;
    std::cout << "Parsed node: " << name << " with command: ";
    for (const auto &cmd : node.command) {
        std::cout << cmd << " ";
    }
    std::cout << "\n";
}

// Function to parse and store a pipe
void parse_pipe(const std::string &line, std::ifstream &flow_file) {
    std::string pipe_name, from, to;

    std::string prefix = "pipe=";
    if (line.substr(0, prefix.length()) == prefix) {
        pipe_name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid pipe format: " << line << "\n";
        return;
    }

    std::getline(flow_file, from);
    from = from.substr(from.find('=') + 1); 
    
    std::getline(flow_file, to);
    to = to.substr(to.find('=') + 1); 

    FlowPipe pipe;
    pipe.from = from;
    pipe.to = to;

    pipes[pipe_name] = pipe;
    std::cout << "Parsed pipe: " << pipe_name << " from " << from << " to " << to << "\n";
}

// Function to parse and store a concatenation
void parse_concatenation(const std::string &line, std::ifstream &flow_file) {
    std::string concat_name, parts_line;
    int part_count;

    std::string prefix = "concatenate=";
    if (line.substr(0, prefix.length()) == prefix) {
        concat_name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid concatenation format: " << line << "\n";
        return;
    }

    std::getline(flow_file, parts_line);
    sscanf(parts_line.c_str(), "parts=%d", &part_count);

    Concatenation concat;
    concat.name = concat_name;

    for (int i = 0; i < part_count; ++i) {
        std::string part;
        std::getline(flow_file, part);
        part = part.substr(part.find('=') + 1);
        concat.parts.push_back(part);
        std::cout << "Added part " << concat.parts[i] << " to concatenation " << concat_name << "\n";
    }

    concatenations[concat_name] = concat;
    std::cout << "Parsed concatenation: " << concat_name << " with " << part_count << " parts\n";
}

// Function to execute a command
void execute_command(const Node &node) {
    std::cout << "Executing command: " << node.name << "\n";
    for (const auto &cmd : node.command) {
        std::cout << cmd << " ";
    }
    std::cout << std::endl;

    std::vector<char*> args;
    for (auto &arg : node.command) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(NULL);

    if (execvp(args[0], args.data()) == -1) {
        std::cerr << "Error: Failed to execute command: " << args[0] << "\n";
        exit(EXIT_FAILURE);
    }
}

// Function to set up and execute a pipe between two nodes
void execute_pipe(const FlowPipe &pipe) {
    std::cout << "Executing pipe from " << pipe.from << " to " << pipe.to << "\n";

    int pipe_fds[2];
    if (::pipe(pipe_fds) == -1) {
        std::cerr << "Error: Failed to create pipe\n";
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) { 
        close(pipe_fds[0]); 
        dup2(pipe_fds[1], STDOUT_FILENO); 
        close(pipe_fds[1]); 

        std::cout << "Executing node: " << pipe.from << "\n";
        execute_command(nodes[pipe.from]);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) { 
        close(pipe_fds[1]); 
        dup2(pipe_fds[0], STDIN_FILENO); 
        close(pipe_fds[0]); 

        std::cout << "Executing node: " << pipe.to << "\n";
        execute_command(nodes[pipe.to]);
    }

    close(pipe_fds[0]);
    close(pipe_fds[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// Function to execute a concatenation of nodes or pipes
void execute_concatenation(const Concatenation &concat) {
    std::cout << "Executing concatenation: " << concat.name << "\n";
    for (const auto &part : concat.parts) {
        if (pipes.find(part) != pipes.end()) {
            execute_pipe(pipes[part]); 
        } else if (nodes.find(part) != nodes.end()) {
            pid_t pid = fork();
            if (pid == 0) {
                execute_command(nodes[part]); 
            }
            waitpid(pid, NULL, 0);
        }
    }
}

// Function to parse the flow file and execute the given target (pipe or concatenation)
void parse_and_execute_flow_file(const std::string &flow_file_path, const std::string &target) {
    std::ifstream flow_file(flow_file_path);
    if (!flow_file.is_open()) {
        std::cerr << "Error: Could not open flow file " << flow_file_path << "\n";
        exit(1);
    }

    std::string line;

    while (std::getline(flow_file, line)) {
        std::cout << "Read line: " << line << "\n";
        if (line.find("node=") == 0) {
            parse_node(line, flow_file);
        } else if (line.find("pipe=") == 0) {
            parse_pipe(line, flow_file);
        } else if (line.find("concatenate=") == 0) {
            parse_concatenation(line, flow_file);
        }
    }

    if (pipes.find(target) != pipes.end()) {
        std::cout << "Executing pipe: " << target << "\n";
        execute_pipe(pipes[target]);
    } else if (concatenations.find(target) != concatenations.end()) {
        std::cout << "Executing concatenation: " << target << "\n";
        execute_concatenation(concatenations[target]);
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

    parse_and_execute_flow_file(argv[1], argv[2]);
    return 0;
}
