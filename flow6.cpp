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

// Maps for storing nodes and pipes
std::map<std::string, Node> nodes;
std::map<std::string, FlowPipe> pipes;

// Global variable to maintain intermediate output
std::string global_pipe_input;

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

// Function to execute a node command using execvp
void execute_node(const Node &node) {
    std::vector<char*> args = prepare_args(node);
    execvp(args[0], args.data());  // Execute the command
    perror("[ERROR] execvp failed");  // Only reached if execvp fails
    _exit(EXIT_FAILURE);  // Exit if execvp fails
}

// Function to capture output from a file descriptor
std::string capture_output(int fd) {
    char buffer[4096];
    std::string output;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    return output;
}

// Function to execute a pipe (from-to pair, where from and to can be either nodes or pipes)
std::string execute_pipe(const FlowPipe &flow_pipe) {
    std::cout << "[DEBUG] Executing pipe from " << flow_pipe.from << " to " << flow_pipe.to << std::endl;

    // Recursively execute 'from' part (which can be a pipe or node)
    std::string producer_output;
    if (pipes.find(flow_pipe.from) != pipes.end()) {
        // 'from' is another pipe, recursively call execute_pipe
        producer_output = execute_pipe(pipes[flow_pipe.from]);
    } else if (nodes.find(flow_pipe.from) != nodes.end()) {
        // 'from' is a node, execute it and capture output
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("[ERROR] pipe creation failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("[ERROR] fork failed for producer");
            exit(EXIT_FAILURE);
        }

        if (pid1 == 0) {  // Child process 1 (producer)
            close(pipefd[0]);  // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
            close(pipefd[1]);  // Close the write end after redirection

            execute_node(nodes[flow_pipe.from]);  // Execute the node (producer)
            _exit(EXIT_SUCCESS);  // Exit the child process
        }

        close(pipefd[1]);  // Parent closes the write end of the pipe

        // Wait for the producer to finish
        waitpid(pid1, nullptr, 0);

        // Capture the output from the producer
        producer_output = capture_output(pipefd[0]);  // Read from the pipe connected to producer's stdout
        close(pipefd[0]);  // Parent closes the read end of the pipe
    }

    // Recursively execute 'to' part (which can be a pipe or node)
    if (pipes.find(flow_pipe.to) != pipes.end()) {
        // 'to' is another pipe, recursively call execute_pipe
        global_pipe_input = producer_output;  // Store the output in the global variable
        return execute_pipe(pipes[flow_pipe.to]);  // Recursive call for the 'to' part if it's a pipe
    } else if (nodes.find(flow_pipe.to) != nodes.end()) {
        // 'to' is a node, execute the node with the producer output as input
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("[ERROR] pipe creation failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("[ERROR] fork failed for consumer");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) {  // Child process 2 (consumer)
            close(pipefd[0]);  // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
            close(pipefd[1]);  // Close the write end after redirection

            // Pass the producer's output to the consumer's stdin
            int temp_pipefd[2];
            if (pipe(temp_pipefd) == -1) {
                perror("[ERROR] temp pipe creation failed");
                exit(EXIT_FAILURE);
            }
            write(temp_pipefd[1], producer_output.c_str(), producer_output.size());  // Write producer output to a temp pipe
            close(temp_pipefd[1]);  // Close write end after writing
            dup2(temp_pipefd[0], STDIN_FILENO);  // Redirect temp pipe to consumer's stdin
            close(temp_pipefd[0]);  // Close read end after redirection

            execute_node(nodes[flow_pipe.to]);  // Execute the node (consumer)
            _exit(EXIT_SUCCESS);  // Exit the child process
        }

        close(pipefd[1]);  // Parent closes the write end of the pipe

        // Wait for the consumer to finish
        waitpid(pid2, nullptr, 0);

        // Capture the consumer's output
        std::string consumer_output = capture_output(pipefd[0]);  // Read from the consumer pipe
        close(pipefd[0]);  // Parent closes the read end of the pipe
        return consumer_output;
    }

    return "";  // Return empty string if no valid pipe or node is found
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

// Function to parse the flow file
void parse_flow_file(std::ifstream &flow_file) {
    std::string line;

    while (std::getline(flow_file, line)) {
        if (line.find("node=") == 0) {
            parse_node(line, flow_file);
        } else if (line.find("pipe=") == 0) {
            parse_pipe(line, flow_file);
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
        std::string final_output = execute_pipe(pipes[action]);
        std::cout << "Final Output:\n" << final_output;
    } else {
        std::cerr << "[DEBUG] Error: Unknown action '" << action << "'\n";
        return 1;
    }

    return 0;
}