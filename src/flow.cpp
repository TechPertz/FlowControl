#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// Structure to represent a node (a command)
struct Node {
    std::string command;
    std::vector<std::string> args;
};

// Structure to represent a concatenation sequence
struct Concatenation {
    int parts;
    std::vector<std::string> part_nodes;
};

// Maps to store nodes, pipes, and concatenations
std::map<std::string, Node> nodes;
std::map<std::string, std::pair<std::string, std::string>> pipes;
std::map<std::string, Concatenation> concatenations;

std::string current_node;  // Temporary variable to store the current node

// Function to parse the .flow file
void parseFlowFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.find_first_not_of(' ') == std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            if (key.rfind("node", 0) == 0) {
                current_node = value;
            } else if (key == "command" && !current_node.empty()) {
                std::istringstream cmd_stream(value);
                std::string cmd;
                cmd_stream >> cmd;
                Node node;
                node.command = cmd;

                std::string arg;
                while (cmd_stream >> arg) {
                    node.args.push_back(arg);
                }
                nodes[current_node] = node;
                current_node.clear();
            } else if (key == "pipe") {
                std::string pipe_name = value;
                std::string from, to;

                if (std::getline(file, line)) {
                    std::istringstream from_line(line);
                    std::getline(from_line, key, '=');
                    std::getline(from_line, from);
                }

                if (std::getline(file, line)) {
                    std::istringstream to_line(line);
                    std::getline(to_line, key, '=');
                    std::getline(to_line, to);
                }

                pipes[pipe_name] = {from, to};
            } else if (key == "concatenate") {
                Concatenation concat;
                std::getline(file, line);
                std::istringstream part_line(line);
                part_line >> key >> concat.parts;
                for (int i = 0; i < concat.parts; ++i) {
                    std::getline(file, line);
                    std::istringstream part_line(line);
                    std::string part_node;
                    part_line >> key >> part_node;
                    concat.part_nodes.push_back(part_node);
                }
                concatenations[value] = concat;
            }
        }
    }
}

// Function to prepare args and handle special cases like sed
std::vector<char *> prepareArgs(const Node &node) {
    std::vector<char *> args;
    args.push_back(const_cast<char *>(node.command.c_str()));

    for (const auto &arg : node.args) {
        args.push_back(const_cast<char *>(arg.c_str()));
    }

    args.push_back(nullptr);
    return args;
}

// Function to execute a node (command)
void executeNode(const Node &node) {
    std::vector<char *> args = prepareArgs(node);

    // Debugging prints
    std::cout << "Executing command: " << node.command << std::endl;
    std::cout << "Arguments: ";
    for (auto arg : args) {
        if (arg != nullptr) {
            std::cout << arg << " ";
        }
    }
    std::cout << std::endl;

    if (execvp(args[0], args.data()) == -1) {
        perror("execvp");
        exit(1);
    }
}

// Function to execute a concatenation of nodes
void executeConcatenation(const Concatenation &concat) {
    int prev_fd[2];  // Previous pipe for chaining commands
    int current_fd[2];

    for (int i = 0; i < concat.parts; ++i) {
        const std::string &node_name = concat.part_nodes[i];
        const Node &node = nodes[node_name];

        if (i > 0) {
            pipe(current_fd);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {  // Child process
            if (i > 0) {
                close(prev_fd[1]);
                dup2(prev_fd[0], STDIN_FILENO);
                close(prev_fd[0]);
            }

            if (i < concat.parts - 1) {
                close(current_fd[0]);
                dup2(current_fd[1], STDOUT_FILENO);
                close(current_fd[1]);
            }

            executeNode(node);
        }

        if (i > 0) {
            close(prev_fd[0]);
            close(prev_fd[1]);
        }

        if (i < concat.parts - 1) {
            prev_fd[0] = current_fd[0];
            prev_fd[1] = current_fd[1];
        }

        wait(nullptr);
    }
}

// Function to execute a pipe
void executePipe(const std::string &pipe_name) {
    auto pipe_info = pipes[pipe_name];
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(1);
    }

    if (pid1 == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        executeNode(nodes[pipe_info.first]);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        executeNode(nodes[pipe_info.second]);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <flow_file> <action>" << std::endl;
        return 1;
    }

    std::string flow_file = argv[1];
    std::string action = argv[2];

    parseFlowFile(flow_file);

    if (pipes.find(action) != pipes.end()) {
        executePipe(action);
    } else if (concatenations.find(action) != concatenations.end()) {
        executeConcatenation(concatenations[action]);
    } else {
        std::cerr << "Error: Unknown action '" << action << "'." << std::endl;
        return 1;
    }

    return 0;
}