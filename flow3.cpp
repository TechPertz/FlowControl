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

// Helper function to tokenize command strings, treating quoted strings as a single token
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;

    while (iss >> token) {
        // Check if the token starts with a quote
        if (token[0] == '"' || token[0] == '\'') {
            char quote_char = token[0];  // Store the type of quote (either ' or ")
            std::string quoted_token = token;  // Start building the quoted token

            // Keep reading until we find the closing quote
            while (quoted_token.back() != quote_char || quoted_token.length() == 1) {
                std::string next_token;
                if (!(iss >> next_token)) {
                    std::cerr << "Error: Mismatched quotes in command: " << command_line << "\n";
                    return tokens;
                }
                quoted_token += " " + next_token;
            }

            // Remove the surrounding quotes and add the token
            quoted_token = quoted_token.substr(1, quoted_token.length() - 2);
            tokens.push_back(quoted_token);
        } else {
            // Regular token, add it directly
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
    
    // Execute the command with execvp
    if (execvp(args[0], args.data()) == -1) {
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

// Function to handle piping between two nodes (e.g., ls | wc)
void execute_pipe(const FlowPipe &pipe) {
    int pipefd[2];
    if (::pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {  // Child process 1 (producer - ls)
        close(pipefd[0]);  // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
        close(pipefd[1]);  // Close the pipe write end after redirecting

        // Execute the first command (ls)
        execute_node(nodes[pipe.from]);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {  // Child process 2 (consumer - wc)
        close(pipefd[1]);  // Close unused write end
        dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe read end
        close(pipefd[0]);  // Close the pipe read end after redirecting

        // Execute the second command (wc)
        execute_node(nodes[pipe.to]);
    }

    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for both processes to finish
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}

// Function to parse and store a node
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    // Extract node name
    std::string prefix = "node=";
    if (line.substr(0, prefix.length()) == prefix) {
        name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid node format: " << line << "\n";
        return;
    }

    // Read the next line for the command
    std::getline(flow_file, command_line);

    // Strip out 'command=' from the command line
    std::string command_prefix = "command=";
    if (command_line.substr(0, command_prefix.length()) == command_prefix) {
        command_line = command_line.substr(command_prefix.length());
    }

    // Tokenize and store node command
    Node node;
    node.name = name;
    node.command = tokenize_command(command_line);

    nodes[name] = node;
}

// Function to parse and store a pipe
void parse_pipe(const std::string &line, std::ifstream &flow_file) {
    std::string pipe_name, from, to;

    // Extract pipe name
    std::string prefix = "pipe=";
    if (line.substr(0, prefix.length()) == prefix) {
        pipe_name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid pipe format: " << line << "\n";
        return;
    }

    // Read 'from' and 'to' lines
    std::getline(flow_file, from);
    from = from.substr(from.find('=') + 1); // Extract 'from'
    
    std::getline(flow_file, to);
    to = to.substr(to.find('=') + 1); // Extract 'to'

    FlowPipe pipe;
    pipe.from = from;
    pipe.to = to;

    pipes[pipe_name] = pipe;
}

// Function to parse the flow file
void parse_flow_file(std::ifstream &flow_file) {
    std::string line;

    // Parse the flow file line by line
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
        std::cerr << "Error: Could not open flow file " << flow_file_name << "\n";
        exit(1);
    }

    // Parse the flow file
    parse_flow_file(flow_file);

    // Execute the action (pipe)
    if (pipes.find(action) != pipes.end()) {
        execute_pipe(pipes[action]);
    } else {
        std::cerr << "Error: Unknown action '" << action << "'\n";
        return 1;
    }

    return 0;
}