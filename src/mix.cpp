#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <set>
#include <functional>
#include <cstring>
#include <csignal>

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
    std::vector<std::string> parts;
};

// Maps for storing nodes, pipes, and concatenations
std::unordered_map<std::string, Node> nodes;                   // Maps node names to Node structs
std::unordered_map<std::string, FlowPipe> pipes;               // Maps pipe names to FlowPipe structs
std::unordered_map<std::string, Concatenation> concatenations; // Maps concatenation names to Concatenation structs

// Function Declarations (Prototypes)

// Helper function to tokenize command strings, treating quoted strings as single tokens.
std::vector<std::string> tokenize_command(const std::string &command_line);

// Function to prepare args for execvp.
std::vector<char*> prepare_args(const Node &node);

// Function to execute a node command using execvp.
void execute_node(const Node &node);

// Function to execute a single node, with optional output redirection.
void execute_single_node(const std::string &node_name, int output_fd = STDOUT_FILENO);

// Function to execute a concatenation.
void execute_concatenation(const std::string &concat_name, int output_fd = STDOUT_FILENO);

// Helper function to execute any action (node, pipe, or concatenation).
void execute_action(const std::string &action_name, int output_fd = STDOUT_FILENO);

// Function to execute a pipe.
void execute_pipe(const std::string &pipe_name, int output_fd = STDOUT_FILENO);

// Parse the .flow file and populate nodes, pipes, and concatenations.
void parse_flow_file(const std::string& filename);

// Main function.
int main(int argc, char* argv[]);

// Function Definitions

/**
 * Helper function to tokenize command strings, treating quoted strings as single tokens.
 */
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (size_t i = 0; i < command_line.length(); ++i) {
        char c = command_line[i];

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;  // Skip the quote character
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;  // Skip the quote character
        }

        if (std::isspace(c) && !in_single_quote && !in_double_quote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}

/**
 * Function to prepare args for execvp.
 */
std::vector<char*> prepare_args(const Node &node) {
    std::vector<char*> args;
    for (const auto &arg : node.command) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);  // Null-terminated list of arguments
    return args;
}

/**
 * Function to execute a node command using execvp.
 */
void execute_node(const Node &node) {
    std::vector<char*> args = prepare_args(node);
    execvp(args[0], args.data());  // Execute the command
    perror("[ERROR] execvp failed");  // Only reached if execvp fails
    _exit(EXIT_FAILURE);  // Exit if execvp fails
}

/**
 * Function to execute a single node, with optional output redirection.
 */
void execute_single_node(const std::string &node_name, int output_fd) {
    auto it = nodes.find(node_name);
    if (it == nodes.end()) {
        std::cerr << "[ERROR] Node not found: " << node_name << std::endl;
        exit(EXIT_FAILURE);
    }

    const Node &node = it->second;

    pid_t pid = fork();
    if (pid == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Child process
        if (output_fd != STDOUT_FILENO) {
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("[ERROR] dup2 failed");
                exit(EXIT_FAILURE);
            }
            close(output_fd);  // Close after duplication
        }
        execute_node(node);  // Execute the node
        _exit(EXIT_FAILURE);  // Should not reach here
    }

    // Parent process waits for the child
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            std::cerr << "[ERROR] Child process exited with error code: " << WEXITSTATUS(status) << std::endl;
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig != SIGPIPE) {
            std::cerr << "[ERROR] Child process was terminated by signal: " << sig << std::endl;
        }
        // Else, silently ignore SIGPIPE
    }
}

/**
 * Function to execute a concatenation.
 */
void execute_concatenation(const std::string &concat_name, int output_fd) {
    auto it = concatenations.find(concat_name);
    if (it == concatenations.end()) {
        std::cerr << "[ERROR] Concatenation not found: " << concat_name << std::endl;
        exit(EXIT_FAILURE);
    }

    const Concatenation &concat = it->second;

    for (const auto &part_name : concat.parts) {
        // Execute each part
        if (nodes.find(part_name) != nodes.end()) {
            execute_single_node(part_name, output_fd);
        } else if (pipes.find(part_name) != pipes.end()) {
            execute_pipe(part_name, output_fd);
        } else if (concatenations.find(part_name) != concatenations.end()) {
            execute_concatenation(part_name, output_fd);
        } else {
            std::cerr << "[ERROR] Unknown part: " << part_name << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Helper function to execute any action (node, pipe, or concatenation).
 */
void execute_action(const std::string &action_name, int output_fd) {
    if (nodes.find(action_name) != nodes.end()) {
        execute_single_node(action_name, output_fd);
    } else if (pipes.find(action_name) != pipes.end()) {
        execute_pipe(action_name, output_fd);
    } else if (concatenations.find(action_name) != concatenations.end()) {
        execute_concatenation(action_name, output_fd);
    } else {
        std::cerr << "[ERROR] Unknown action: " << action_name << std::endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Function to execute a pipe.
 */
void execute_pipe(const std::string &pipe_name, int output_fd) {
    auto it = pipes.find(pipe_name);
    if (it == pipes.end()) {
        std::cerr << "[ERROR] Pipe not found: " << pipe_name << std::endl;
        exit(EXIT_FAILURE);
    }

    const FlowPipe &flow_pipe = it->second;

    int fd[2];
    if (pipe(fd) == -1) {
        perror("[ERROR] pipe creation failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid_to = fork();
    if (pid_to == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    } else if (pid_to == 0) {  // Child process for 'to'
        close(fd[1]);  // Close write end
        if (dup2(fd[0], STDIN_FILENO) == -1) {
            perror("[ERROR] dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd[0]);

        if (output_fd != STDOUT_FILENO) {
            // Redirect output if necessary
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("[ERROR] dup2 failed");
                exit(EXIT_FAILURE);
            }
            close(output_fd);
        }

        // Corrected line: pass output_fd instead of STDOUT_FILENO
        execute_action(flow_pipe.to, STDOUT_FILENO);

        _exit(EXIT_FAILURE);
    }

    pid_t pid_from = fork();
    if (pid_from == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    } else if (pid_from == 0) {  // Child process for 'from'
        close(fd[0]);  // Close read end
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("[ERROR] dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd[1]);

        execute_action(flow_pipe.from, STDOUT_FILENO);

        _exit(EXIT_FAILURE);
    }

    // Parent process closes unused pipe ends and waits for child processes
    close(fd[0]);
    close(fd[1]);

    int status;
    waitpid(pid_from, &status, 0);
    waitpid(pid_to, &status, 0);
}

/**
 * Parse the .flow file and populate nodes, pipes, and concatenations.
 */
void parse_flow_file(const std::string& filename) {
    std::ifstream flow_file(filename);
    if (!flow_file.is_open()) {
        std::cerr << "[ERROR] Could not open flow file " << filename << "\n";
        exit(1);
    }

    std::string line;
    while (std::getline(flow_file, line)) {
        if (line.empty())
            continue;

        if (line.substr(0, 5) == "node=") {
            std::string node_name = line.substr(5);
            std::string command_line;
            if (!std::getline(flow_file, command_line) || command_line.substr(0, 8) != "command=") {
                std::cerr << "[ERROR] Missing command for node: " << node_name << "\n";
                exit(EXIT_FAILURE);
            }
            std::string command = command_line.substr(8);
            Node node;
            node.name = node_name;
            node.command = tokenize_command(command);
            nodes[node_name] = node;
            std::cout << "[DEBUG] Parsed node: " << node_name << " with command: " << command << std::endl;
        } else if (line.substr(0, 5) == "pipe=") {
            std::string pipe_name = line.substr(5);
            std::string from_line, to_line;
            if (!std::getline(flow_file, from_line) || !std::getline(flow_file, to_line)) {
                std::cerr << "[ERROR] Missing 'from' or 'to' for pipe: " << pipe_name << "\n";
                exit(EXIT_FAILURE);
            }
            if (from_line.substr(0, 5) != "from=" || to_line.substr(0, 3) != "to=") {
                std::cerr << "[ERROR] Invalid 'from' or 'to' format for pipe: " << pipe_name << "\n";
                exit(EXIT_FAILURE);
            }
            std::string from = from_line.substr(5);
            std::string to = to_line.substr(3);
            FlowPipe pipe;
            pipe.from = from;
            pipe.to = to;
            pipes[pipe_name] = pipe;
            std::cout << "[DEBUG] Parsed pipe: " << pipe_name << " from " << from << " to " << to << std::endl;
        } else if (line.substr(0, 12) == "concatenate=") {
            std::string concat_name = line.substr(12);
            std::string parts_line;
            if (!std::getline(flow_file, parts_line) || parts_line.substr(0, 6) != "parts=") {
                std::cerr << "[ERROR] Missing 'parts' for concatenation: " << concat_name << "\n";
                exit(EXIT_FAILURE);
            }
            int num_parts = std::stoi(parts_line.substr(6));
            Concatenation concat;
            for (int i = 0; i < num_parts; ++i) {
                std::string part_line;
                if (!std::getline(flow_file, part_line)) {
                    std::cerr << "[ERROR] Missing 'part_" << i << "' for concatenation: " << concat_name << "\n";
                    exit(EXIT_FAILURE);
                }
                std::string part_prefix = "part_" + std::to_string(i) + "=";
                if (part_line.substr(0, part_prefix.length()) != part_prefix) {
                    std::cerr << "[ERROR] Invalid 'part_" << i << "' format for concatenation: " << concat_name << "\n";
                    exit(EXIT_FAILURE);
                }
                std::string part_name = part_line.substr(part_prefix.length());
                concat.parts.push_back(part_name);
            }
            concatenations[concat_name] = concat;
            std::cout << "[DEBUG] Parsed concatenation: " << concat_name << " with " << num_parts << " parts" << std::endl;
        }
        // Ignore other lines or add more parsing as needed
    }
}

/**
 * Main function.
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./flow <file.flow> <action>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string flowFile = argv[1];
    std::string action = argv[2];

    // Parse the flow file
    parse_flow_file(flowFile);

    // Execute the action
    std::cout << "[DEBUG] Executing action: " << action << std::endl;
    execute_action(action, STDOUT_FILENO);

    return EXIT_SUCCESS;
}