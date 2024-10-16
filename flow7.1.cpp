#include <iostream>
#include <fstream>
#include <sstream>
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

// Structs for nodes and pipes
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

// Helper function to tokenize command strings, treating quoted strings as single tokens
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;
    char quote_char = '\0';
    bool in_quotes = false;

    while (iss) {
        char c = iss.get();

        if (c == EOF) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            break;
        }

        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
            } else {
                token += c;
            }
        } else {
            if (c == '\'' || c == '"') {
                in_quotes = true;
                quote_char = c;
            } else if (isspace(c)) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
    }

    if (in_quotes) {
        std::cerr << "[ERROR] Mismatched quotes in command: " << command_line << "\n";
        exit(EXIT_FAILURE);
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

// Function to handle recursive pipe execution with concurrent processes
void execute_pipe(const FlowPipe &flow_pipe) {
    std::cout << "[DEBUG] Executing pipe from " << flow_pipe.from << " to " << flow_pipe.to << std::endl;

    // Vector to store commands
    std::vector<Node> command_nodes;

    // Set to keep track of visited nodes/pipes to prevent circular dependencies
    std::set<std::string> visited;

    // Recursive function to build command chain
    std::function<void(const std::string&)> build_command_chain = [&](const std::string& name) {
        if (visited.count(name)) {
            std::cerr << "[ERROR] Detected circular dependency involving: " << name << std::endl;
            exit(EXIT_FAILURE);
        }
        visited.insert(name);

        if (nodes.find(name) != nodes.end()) {
            command_nodes.push_back(nodes[name]);
        } else if (pipes.find(name) != pipes.end()) {
            build_command_chain(pipes[name].from);
            build_command_chain(pipes[name].to);
        } else {
            std::cerr << "[ERROR] Unknown node or pipe: " << name << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    // Build the command chain
    build_command_chain(flow_pipe.from);
    build_command_chain(flow_pipe.to);

    int num_commands = command_nodes.size();
    std::vector<int> pipefds;

    // Create the necessary pipes
    for (int i = 0; i < num_commands - 1; ++i) {
        int fd[2];
        if (pipe(fd) == -1) {
            perror("[ERROR] pipe creation failed");
            exit(EXIT_FAILURE);
        }
        pipefds.push_back(fd[0]); // read end
        pipefds.push_back(fd[1]); // write end
    }

    // Fork and execute commands
    for (int i = 0; i < num_commands; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("[ERROR] fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process

            // If not the first command, set stdin to the previous pipe's read end
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) == -1) {
                    perror("[ERROR] dup2 stdin failed");
                    exit(EXIT_FAILURE);
                }
            }

            // If not the last command, set stdout to the current pipe's write end
            if (i < num_commands - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("[ERROR] dup2 stdout failed");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all pipe fds in child
            for (size_t j = 0; j < pipefds.size(); ++j) {
                close(pipefds[j]);
            }

            // Execute the command
            execute_node(command_nodes[i]);
            _exit(EXIT_FAILURE); // Should not reach here
        }
    }

    // Parent process closes all pipe fds
    for (size_t i = 0; i < pipefds.size(); ++i) {
        close(pipefds[i]);
    }

    // Wait for all child processes
    int status;
    for (int i = 0; i < num_commands; ++i) {
        wait(&status);
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
}

// Function to parse and store a node
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    std::string prefix = "node=";
    if (line.substr(0, prefix.length()) == prefix) {
        name = line.substr(prefix.length());
    } else {
        std::cerr << "[ERROR] Invalid node format: " << line << "\n";
        exit(EXIT_FAILURE);
    }

    if (!std::getline(flow_file, command_line)) {
        std::cerr << "[ERROR] Missing command for node: " << name << "\n";
        exit(EXIT_FAILURE);
    }

    std::string command_prefix = "command=";
    if (command_line.substr(0, command_prefix.length()) == command_prefix) {
        command_line = command_line.substr(command_prefix.length());
    } else {
        std::cerr << "[ERROR] Invalid command format for node: " << name << "\n";
        exit(EXIT_FAILURE);
    }

    Node node;
    node.name = name;
    node.command = tokenize_command(command_line);

    nodes[name] = node;
    std::cout << "[DEBUG] Parsed node: " << name << " with command: " << command_line << std::endl;
}

// Function to parse and store a pipe
void parse_pipe(const std::string &line, std::ifstream &flow_file) {
    std::string pipe_name, from_line, to_line;

    std::string prefix = "pipe=";
    if (line.substr(0, prefix.length()) == prefix) {
        pipe_name = line.substr(prefix.length());
    } else {
        std::cerr << "[ERROR] Invalid pipe format: " << line << "\n";
        exit(EXIT_FAILURE);
    }

    if (!std::getline(flow_file, from_line) || !std::getline(flow_file, to_line)) {
        std::cerr << "[ERROR] Missing 'from' or 'to' for pipe: " << pipe_name << "\n";
        exit(EXIT_FAILURE);
    }

    std::string from_prefix = "from=";
    std::string to_prefix = "to=";

    if (from_line.substr(0, from_prefix.length()) != from_prefix ||
        to_line.substr(0, to_prefix.length()) != to_prefix) {
        std::cerr << "[ERROR] Invalid 'from' or 'to' format for pipe: " << pipe_name << "\n";
        exit(EXIT_FAILURE);
    }

    std::string from = from_line.substr(from_prefix.length());
    std::string to = to_line.substr(to_prefix.length());

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
        if (line.empty()) continue;

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
        std::cerr << "[ERROR] Could not open flow file " << flow_file_name << "\n";
        exit(1);
    }

    parse_flow_file(flow_file);

    if (pipes.find(action) != pipes.end()) {
        std::cout << "[DEBUG] Executing pipe: " << action << std::endl;
        execute_pipe(pipes[action]);
    } else if (nodes.find(action) != nodes.end()) {
        std::cout << "[DEBUG] Executing node: " << action << std::endl;

        pid_t pid = fork();
        if (pid == -1) {
            perror("[ERROR] fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {  // Child process
            execute_node(nodes[action]);  // Execute the node
            _exit(EXIT_FAILURE);  // Should not reach here
        }

        // Wait for the child process to finish
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
    } else {
        std::cerr << "[ERROR] Unknown action '" << action << "'\n";
        return 1;
    }

    return 0;
}