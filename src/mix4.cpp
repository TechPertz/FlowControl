#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>

// Structs for nodes, pipes, concatenations, stderr captures, and file nodes
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

struct StderrCapture {
    std::string name;
    std::string from;
};

struct FileNode {
    std::string name;
    std::string filename;
};

// Maps for storing nodes, pipes, concatenations, stderr captures, and file nodes
std::unordered_map<std::string, Node> nodes;
std::unordered_map<std::string, FlowPipe> pipes;
std::unordered_map<std::string, Concatenation> concatenations;
std::unordered_map<std::string, StderrCapture> stderrCaptures;
std::unordered_map<std::string, FileNode> fileNodes;

// Helper function to execute commands or actions
void execute_command(const std::vector<std::string> &command_args, int input_fd, int output_fd, bool redirect_stderr, bool suppress_error_messages) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("[ERROR] fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {  // Child process
        if (input_fd != STDIN_FILENO) {
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("[ERROR] dup2 input_fd failed");
                exit(EXIT_FAILURE);
            }
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("[ERROR] dup2 output_fd failed");
                exit(EXIT_FAILURE);
            }
            close(output_fd);
        }
        if (redirect_stderr) {
            if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                perror("[ERROR] dup2 STDERR failed");
                exit(EXIT_FAILURE);
            }
        }
        std::vector<char*> args;
        for (const auto &arg : command_args) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);
        execvp(args[0], args.data());
        perror("[ERROR] execvp failed");
        exit(EXIT_FAILURE);
    }
    // Parent process waits for the child
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0 && !suppress_error_messages) {
            std::cerr << "[ERROR] Child process exited with error code: " << WEXITSTATUS(status) << std::endl;
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig != SIGPIPE && !suppress_error_messages) {
            std::cerr << "[ERROR] Child process was terminated by signal: " << sig << std::endl;
        }
    }
}

// Function Definitions

/**
 * Helper function to tokenize command strings, treating quoted strings as single tokens.
 */
std::vector<std::string> tokenize_command(const std::string &command_line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (char c : command_line) {
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
 * Helper function to execute any action (node, pipe, concatenation, stderr capture, or file node).
 */
void execute_action(const std::string &action_name, int input_fd = STDIN_FILENO, int output_fd = STDOUT_FILENO, bool suppress_error_messages = false) {
    if (nodes.find(action_name) != nodes.end()) {
        const Node &node = nodes[action_name];
        execute_command(node.command, input_fd, output_fd, false, suppress_error_messages);
    } else if (pipes.find(action_name) != pipes.end()) {
        const FlowPipe &flow_pipe = pipes[action_name];

        // Handle if 'from' or 'to' is a file node
        bool from_is_file = fileNodes.find(flow_pipe.from) != fileNodes.end();
        bool to_is_file = fileNodes.find(flow_pipe.to) != fileNodes.end();

        if (from_is_file && to_is_file) {
            std::cerr << "[ERROR] Both 'from' and 'to' cannot be file nodes in pipe: " << action_name << std::endl;
            exit(EXIT_FAILURE);
        }

        if (from_is_file) {
            // 'from' is a file node; open the file and set as input_fd
            const FileNode &fileNode = fileNodes[flow_pipe.from];
            int file_fd = open(fileNode.filename.c_str(), O_RDONLY);
            if (file_fd == -1) {
                perror("[ERROR] open failed");
                exit(EXIT_FAILURE);
            }
            execute_action(flow_pipe.to, file_fd, output_fd, suppress_error_messages);
            close(file_fd);
        } else if (to_is_file) {
            // 'to' is a file node; open the file and set as output_fd
            const FileNode &fileNode = fileNodes[flow_pipe.to];
            int file_fd = open(fileNode.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (file_fd == -1) {
                perror("[ERROR] open failed");
                exit(EXIT_FAILURE);
            }
            execute_action(flow_pipe.from, input_fd, file_fd, suppress_error_messages);
            close(file_fd);
        } else {
            // Regular pipe between two actions
            int fd[2];
            if (pipe(fd) == -1) {
                perror("[ERROR] pipe creation failed");
                exit(EXIT_FAILURE);
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("[ERROR] fork failed");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {  // Child process
                close(fd[0]);  // Close read end
                execute_action(flow_pipe.from, input_fd, fd[1], suppress_error_messages);
                close(fd[1]);
                exit(EXIT_SUCCESS);
            } else {
                close(fd[1]);  // Close write end
                execute_action(flow_pipe.to, fd[0], output_fd, suppress_error_messages);
                close(fd[0]);
                waitpid(pid, nullptr, 0);
            }
        }
    } else if (concatenations.find(action_name) != concatenations.end()) {
        const Concatenation &concat = concatenations[action_name];
        for (const auto &part_name : concat.parts) {
            execute_action(part_name, input_fd, output_fd, suppress_error_messages);
        }
    } else if (stderrCaptures.find(action_name) != stderrCaptures.end()) {
        const StderrCapture &stderrCapture = stderrCaptures[action_name];
        // Redirect stderr to stdout
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

        if (pid == 0) {  // Child process
            dup2(fd[1], STDOUT_FILENO);
            dup2(fd[1], STDERR_FILENO);
            close(fd[0]);
            close(fd[1]);
            execute_action(stderrCapture.from, input_fd, STDOUT_FILENO, true);
            exit(EXIT_FAILURE);
        } else {
            close(fd[1]);
            execute_command({"cat"}, fd[0], output_fd, false, suppress_error_messages);
            close(fd[0]);
            waitpid(pid, nullptr, 0);
        }
    } else if (fileNodes.find(action_name) != fileNodes.end()) {
        // For file nodes, output the file content
        const FileNode &fileNode = fileNodes[action_name];
        int file_fd = open(fileNode.filename.c_str(), O_RDONLY);
        if (file_fd == -1) {
            perror("[ERROR] open failed");
            exit(EXIT_FAILURE);
        }
        execute_command({"cat"}, file_fd, output_fd, false, suppress_error_messages);
        close(file_fd);
    } else {
        std::cerr << "[ERROR] Unknown action: " << action_name << std::endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Parse the .flow file and populate nodes, pipes, concatenations, stderr captures, and file nodes.
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
            // Parse a node
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
        } else if (line.substr(0, 5) == "pipe=") {
            // Parse a pipe
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
        } else if (line.substr(0, 12) == "concatenate=") {
            // Parse a concatenation
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
        } else if (line.substr(0, 6) == "stderr") {
            // Parse a stderr capture
            std::string stderr_name = line.substr(6);
            if (!stderr_name.empty() && stderr_name[0] == '=') {
                stderr_name = stderr_name.substr(1);
            }
            std::string from_line;
            if (!std::getline(flow_file, from_line) || from_line.substr(0, 5) != "from=") {
                std::cerr << "[ERROR] Missing 'from' for stderr: " << stderr_name << "\n";
                exit(EXIT_FAILURE);
            }
            std::string from = from_line.substr(5);
            StderrCapture stderrCapture;
            stderrCapture.name = stderr_name;
            stderrCapture.from = from;
            stderrCaptures[stderr_name] = stderrCapture;
        } else if (line.substr(0, 5) == "file=") {
            // Parse a file node
            std::string file_name = line.substr(5);
            std::string name_line;
            if (!std::getline(flow_file, name_line) || name_line.substr(0, 5) != "name=") {
                std::cerr << "[ERROR] Missing 'name' for file: " << file_name << "\n";
                exit(EXIT_FAILURE);
            }
            std::string filename = name_line.substr(5);
            FileNode fileNode;
            fileNode.name = file_name;
            fileNode.filename = filename;
            fileNodes[file_name] = fileNode;
        }
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
    execute_action(action);

    return EXIT_SUCCESS;
}