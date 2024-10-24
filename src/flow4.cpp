#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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

// Helper function to tokenize command strings, treating quoted strings as a single token
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;

    while (iss >> token) {
        if (token[0] == '"' || token[0] == '\'') {
            char quote_char = token[0];
            std::string quoted_token = token;

            while (quoted_token.back() != quote_char || quoted_token.length() == 1) {
                std::string next_token;
                if (!(iss >> next_token)) {
                    std::cerr << "Error: Mismatched quotes in command: " << command_line << "\n";
                    return tokens;
                }
                quoted_token += " " + next_token;
            }

            quoted_token = quoted_token.substr(1, quoted_token.length() - 2);
            tokens.push_back(quoted_token);
        } else {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// Function to prepare args for execvp
std::vector<char*> prepare_args(const Node &node) {
    std::vector<char*> args;
    for (const auto &arg : node.command) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);  // Null-terminated list of arguments
    return args;
}

// Function to execute a node command using execvp and print the command being executed
void execute_node(const Node &node, int input_fd = -1, bool is_pipe = false) {
    std::cout << "[DEBUG] Inside execute_node for command: ";
    for (const auto &arg : node.command) {
        std::cout << arg << " ";
    }
    std::cout << std::endl;

    if (input_fd != -1) {
        std::cout << "[DEBUG] Reading input from previous command\n";
        dup2(input_fd, STDIN_FILENO);  // Redirect input from previous command
        close(input_fd);
    }

    std::vector<char*> args = prepare_args(node);

    // Debug: Check the arguments before calling execvp
    std::cout << "[DEBUG] About to execute execvp with: " << args[0] << std::endl;
    for (int i = 0; args[i] != nullptr; i++) {
        std::cout << "[DEBUG] Arg[" << i << "]: " << args[i] << std::endl;
    }

    if (execvp(args[0], args.data()) == -1) {  // Execute the command
        perror("[ERROR] execvp failed");
        std::cout << "[DEBUG] execvp failed for command: " << node.command[0] << std::endl;
        exit(EXIT_FAILURE);  // Exit if execvp fails
    }
}


// Function to handle piping between two nodes (e.g., ls | wc) with output capture
void execute_pipe(const FlowPipe &pipe) {
    std::cout << "[DEBUG] Executing pipe from " << pipe.from << " to " << pipe.to << std::endl;

    int pipefd[2];  // Create a pipe
    if (::pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    std::cout << "[DEBUG] Pipe created successfully" << std::endl;

    // Fork for the producer (first command)
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {  // Child process 1 (producer)
        std::cout << "[DEBUG] Entering producer child process: " << pipe.from << std::endl;

        // Debug before closing the pipe's read end
        std::cout << "[DEBUG] Closing read end of the pipe in producer" << std::endl;
        if (close(pipefd[0]) == -1) {
            perror("[ERROR] close(pipefd[0]) failed in producer");
            _exit(EXIT_FAILURE);  // Exit if closing fails
        }

        // Debug before redirecting stdout to the pipe's write end
        std::cout << "[DEBUG] Redirecting stdout to the pipe's write end" << std::endl;
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("[ERROR] dup2 failed in producer");
            _exit(EXIT_FAILURE);  // Exit if redirection fails
        }

        // Debug after dup2
        std::cout << "[DEBUG] Closing write end of the pipe in producer" << std::endl;
        if (close(pipefd[1]) == -1) {
            perror("[ERROR] close(pipefd[1]) failed in producer");
            _exit(EXIT_FAILURE);  // Exit if closing fails
        }

        // Debug: Check if we're about to call execute_node for the producer
        std::cout << "[DEBUG] Calling execute_node for producer: " << pipe.from << std::endl;

        // Execute the first command (producer)
        execute_node(nodes[pipe.from]);  // This should execute the 'ls -l' command

        std::cerr << "[ERROR] execvp failed for producer: " << pipe.from << std::endl;
        _exit(EXIT_FAILURE);  // Ensure child process exits if execvp fails
    }



    // Parent closes the write end after forking producer (no need for parent to keep it open)
    close(pipefd[1]);

    // Wait for the producer to finish before starting the consumer
    std::cout << "[DEBUG] Waiting for producer (pid1) to finish...\n";
    waitpid(pid1, nullptr, 0);
    std::cout << "[DEBUG] Producer (pid1) finished\n";

    // Fork for the consumer (second command)
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {  // Child process 2 (consumer)
        std::cout << "[DEBUG] Entering consumer child process: " << pipe.to << std::endl;
        close(pipefd[0]);  // Close write end in the consumer
        dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe's read end
        close(pipefd[0]);  // Close the read end after redirection

        // Execute the second command (consumer)
        execute_node(nodes[pipe.to]);  // This should execute the 'ls' command
        _exit(EXIT_SUCCESS);  // Ensure child process exits after execvp
    }

    close(pipefd[0]);  // Parent closes the read end after forking consumer

    // Wait for the consumer to finish
    std::cout << "[DEBUG] Waiting for consumer (pid2) to finish...\n";
    waitpid(pid2, nullptr, 0);
    std::cout << "[DEBUG] Both producer and consumer finished\n";
}



// Function to handle concatenation of multiple parts (cat foo.txt ; cat foo.txt | sed 's/o/u/g' | wc)
void execute_concatenation(const Concatenation &concat, const std::string &pipe_to) {
    std::cout << "[DEBUG] Executing concatenation: " << concat.name << std::endl;
    
    int temp_fd[2];
    if (pipe(temp_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Debug: Track each part of the concatenation
    std::cout << "[DEBUG] Number of parts in concatenation: " << concat.parts.size() << std::endl;

    for (size_t i = 0; i < concat.parts.size(); ++i) {
        const std::string &part_name = concat.parts[i];
        std::cout << "[DEBUG] Executing part: " << part_name << std::endl;

        if (nodes.find(part_name) != nodes.end()) {
            std::cout << "[DEBUG] Part " << part_name << " is a node" << std::endl;
            execute_node(nodes[part_name], temp_fd[0], i < concat.parts.size() - 1);
        } else if (pipes.find(part_name) != pipes.end()) {
            std::cout << "[DEBUG] Part " << part_name << " is a pipe" << std::endl;
            execute_pipe(pipes[part_name]);
        }
    }

    // Debug: Output after concatenation before passing to wc
    std::cout << "[DEBUG] Concatenation output before final command" << std::endl;

    // Now pipe the concatenated output into wc (or any other final command)
    pid_t pid_wc = fork();
    if (pid_wc == 0) {
        // Redirect the concatenated output (temp_fd[0]) to wc's stdin
        dup2(temp_fd[0], STDIN_FILENO);  // wc reads from temp_fd[0]
        close(temp_fd[0]);

        // Execute the final command (e.g., wc)
        std::cout << "[DEBUG] Executing final command: " << pipe_to << std::endl;
        execute_node(nodes[pipe_to]);
    }

    close(temp_fd[0]);
    close(temp_fd[1]);

    // Debug: Wait for the final command to finish
    std::cout << "[DEBUG] Waiting for final command to finish" << std::endl;
    waitpid(pid_wc, nullptr, 0);
    std::cout << "[DEBUG] Final command finished" << std::endl;
}


// Function to parse and store a node
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    std::string prefix = "node=";
    if (line.substr(0, prefix.length()) == prefix) {
        name = line.substr(prefix.length());
    } else {
        std::cerr << "[DEBUG] Error: Invalid node format: " << line << "\n";
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
    std::cout << "[DEBUG] Parsed node: " << name << " with command: " << command_line << std::endl;
}

// Function to parse and store a pipe
void parse_pipe(const std::string &line, std::ifstream &flow_file) {
    std::string pipe_name, from, to;

    std::string prefix = "pipe=";
    if (line.substr(0, prefix.length()) == prefix) {
        pipe_name = line.substr(prefix.length());
    } else {
        std::cerr << "[DEBUG] Error: Invalid pipe format: " << line << "\n";
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
    std::cout << "[DEBUG] Parsed pipe: " << pipe_name << " from " << from << " to " << to << std::endl;
}

// Function to parse and store a concatenation
void parse_concatenation(const std::string &line, std::ifstream &flow_file) {
    std::string concat_name, parts_line;
    int part_count;

    std::string prefix = "concatenate=";
    if (line.substr(0, prefix.length()) == prefix) {
        concat_name = line.substr(prefix.length());
    } else {
        std::cerr << "[DEBUG] Error: Invalid concatenation format: " << line << "\n";
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
    }

    concatenations[concat_name] = concat;
    std::cout << "[DEBUG] Parsed concatenation: " << concat_name << " with " << part_count << " parts" << std::endl;
}

// Function to parse the flow file
void parse_flow_file(std::ifstream &flow_file) {
    std::string line;

    while (std::getline(flow_file, line)) {
        if (line.find("node=") == 0) {
            parse_node(line, flow_file);
        } else if (line.find("pipe=") == 0) {
            parse_pipe(line, flow_file);
        } else if (line.find("concatenate=") == 0) {
            parse_concatenation(line, flow_file);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <flow_file> <action>\n";
        exit(1);
    }

    std::string flow_file_name = argv[1];
    std::string action = argv[2];

    std::ifstream flow_file(flow_file_name);
    if (!flow_file.is_open()) {
        std::cerr << "[DEBUG] Error: Could not open flow file " << flow_file_name << "\n";
        exit(1);
    }

    parse_flow_file(flow_file);

    if (pipes.find(action) != pipes.end()) {
        std::cout << "[DEBUG] Executing pipe: " << action << std::endl;
        execute_pipe(pipes[action]);
    } else if (concatenations.find(action) != concatenations.end()) {
        std::cout << "[DEBUG] Executing concatenation: " << action << std::endl;
        execute_concatenation(concatenations[action], action);
    } else {
        std::cerr << "[DEBUG] Error: Unknown action '" << action << "'\n";
        return 1;
    }

    return 0;
}
