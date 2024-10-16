#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// Struct for nodes (commands)
struct Node {
    std::string name;
    std::vector<std::string> command;
};

// Struct to represent pipes between nodes
struct FlowPipe {
    std::string from;
    std::string to;
};

// Maps to store nodes and pipes
std::map<std::string, Node> nodes;
std::map<std::string, FlowPipe> pipes;

// Global variable to store intermediate pipe input/output
std::string global_pipe_input = "";

// Helper function to tokenize command strings, handling quoted strings
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
    if (execvp(args[0], args.data()) == -1) {
        perror("[ERROR] execvp failed");
        exit(EXIT_FAILURE);
    }
}

// Function to capture the output from a file descriptor (pipe)
std::string capture_output(int fd) {
    std::string output;
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    return output;
}

// Function to execute a pipe
std::string execute_pipe(const FlowPipe &flow_pipe) {
    std::cout << "[DEBUG] Executing pipe from " << flow_pipe.from << " to " << flow_pipe.to << std::endl;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("[ERROR] pipe creation failed");
        exit(EXIT_FAILURE);
    }

    // Fork for the producer (first command)
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("[ERROR] fork failed for producer");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {  // Child process 1 (producer)
        std::cout << "[DEBUG] Entering producer child process: " << flow_pipe.from << std::endl;
        close(pipefd[0]);  // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
        close(pipefd[1]);  // Close the write end after redirection

        // Execute the first command (producer)
        execute_node(nodes[flow_pipe.from]);
        std::cerr << "[ERROR] execvp failed for producer\n";
        _exit(EXIT_FAILURE);  // Exit if execvp fails
    }

    close(pipefd[1]);  // Parent closes the write end of the pipe (no need to write anymore)

    // Wait for the producer to finish
    std::cout << "[DEBUG] Waiting for producer (pid1) to finish...\n";
    waitpid(pid1, nullptr, 0);
    std::cout << "[DEBUG] Producer (pid1) finished\n";

    // Capture the output from the producer
    std::string producer_output = capture_output(pipefd[0]);  // Read from the pipe connected to producer's stdout
    close(pipefd[0]);  // Parent closes the read end of the pipe

    std::cout << "[DEBUG] Producer finished. Captured output: \n" << producer_output << std::endl;

    // Create a new pipe for the consumer process
    int consumer_pipefd[2];  
    if (pipe(consumer_pipefd) == -1) {
        perror("[ERROR] pipe creation failed for consumer_pipefd");
        exit(EXIT_FAILURE);
    }

    // Fork for the consumer (second command)
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("[ERROR] fork failed for consumer");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {  // Child process 2 (consumer)
        std::cout << "[DEBUG] Entering consumer child process: " << flow_pipe.to << std::endl;
        close(consumer_pipefd[0]);  // Close unused read end of the consumer pipe
        dup2(consumer_pipefd[1], STDOUT_FILENO);  // Redirect stdout to the consumer pipe (to capture output)
        close(consumer_pipefd[1]);  // Close the write end after redirection

        // Redirect the producer's output to the consumer's stdin
        int temp_pipefd[2];
        if (pipe(temp_pipefd) == -1) {
            perror("[ERROR] temp pipe creation failed");
            exit(EXIT_FAILURE);
        }
        write(temp_pipefd[1], producer_output.c_str(), producer_output.size());  // Write producer output to a temp pipe
        close(temp_pipefd[1]);  // Close write end of temp pipe after writing

        dup2(temp_pipefd[0], STDIN_FILENO);  // Redirect the temp pipe to the consumer's stdin
        close(temp_pipefd[0]);  // Close read end after redirection

        // Execute the consumer command
        execute_node(nodes[flow_pipe.to]);
        std::cerr << "[ERROR] execvp failed for consumer\n";
        _exit(EXIT_FAILURE);  // Exit if execvp fails
    }

    close(consumer_pipefd[1]);  // Parent closes the write end of the consumer pipe

    // Wait for the consumer to finish
    std::cout << "[DEBUG] Waiting for consumer (pid2) to finish...\n";
    waitpid(pid2, nullptr, 0);
    std::cout << "[DEBUG] Consumer (pid2) finished\n";

    // Capture the consumer's output
    std::string consumer_output = capture_output(consumer_pipefd[0]);  // Read from the consumer pipe
    close(consumer_pipefd[0]);  // Close read end after capturing output

    std::cout << "[DEBUG] Consumer finished. Captured output: \n" << consumer_output << std::endl;

    return consumer_output;  // Return the consumer's output
}

// Function to parse and store a node
void parse_node(const std::string &line, std::ifstream &flow_file) {
    std::string name, command_line;

    std::string prefix = "node=";
    if (line.substr(0, prefix.length()) == prefix) {
        name = line.substr(prefix.length());
    } else {
        std::cerr << "[ERROR] Invalid node format: " << line << "\n";
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
        std::cerr << "[ERROR] Invalid pipe format: " << line << "\n";
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
        std::cerr << "[ERROR] Could not open flow file " << flow_file_name << "\n";
        exit(1);
    }

    parse_flow_file(flow_file);

    if (pipes.find(action) != pipes.end()) {
        std::cout << "[DEBUG] Executing pipe: " << action << std::endl;
        std::string result = execute_pipe(pipes[action]);
        std::cout << "Final Output:\n" << result << std::endl;
    } else {
        std::cerr << "[ERROR] Unknown action '" << action << "'\n";
        return 1;
    }

    return 0;
}
