#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

struct Concatenation {
    std::vector<std::string> parts;
};

std::unordered_map<std::string, std::string> nodes;               // Maps node names to commands
std::unordered_map<std::string, Concatenation> concatenations;    // Maps concatenation names to their parts

/**
 * Split a string into tokens based on whitespace.
 */
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * Parse the .flow file and populate nodes and concatenations.
 */
bool parseFlowFile(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Error opening flow file: " << filename << std::endl;
        return false;
    }

    std::string line;
    std::string currentNode;
    std::string currentConcat;
    while (std::getline(infile, line)) {
        if (line.empty())
            continue;

        if (line.substr(0, 5) == "node=") {
            currentNode = line.substr(5);
        } else if (line.substr(0, 8) == "command=") {
            if (currentNode.empty()) {
                std::cerr << "Command defined without a node." << std::endl;
                return false;
            }
            nodes[currentNode] = line.substr(8);
            currentNode.clear();
        } else if (line.substr(0, 12) == "concatenate=") {
            currentConcat = line.substr(12);
            concatenations[currentConcat] = Concatenation();
        } else if (line.substr(0, 6) == "parts=") {
            // Number of parts, can be ignored as we use dynamic vectors
        } else if (line.substr(0, 5) == "part_") {
            if (currentConcat.empty()) {
                std::cerr << "Part defined without a concatenation." << std::endl;
                return false;
            }
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                std::cerr << "Invalid part definition." << std::endl;
                return false;
            }
            std::string partName = line.substr(pos + 1);
            concatenations[currentConcat].parts.push_back(partName);
        }
        // Ignore other lines or add more parsing as needed
    }
    return true;
}

/**
 * Execute a command represented by a node.
 */
int executeNode(const std::string& nodeName) {
    auto it = nodes.find(nodeName);
    if (it == nodes.end()) {
        std::cerr << "Node not found: " << nodeName << std::endl;
        return -1;
    }

    std::string command = it->second;
    std::vector<std::string> args = split(command);

    // Convert args to char* array for execvp
    std::vector<char*> argv;
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);  // Null-terminate the array

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process
        execvp(argv[0], argv.data());
        // If execvp returns, an error occurred
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return status;
    }
}

/**
 * Execute a concatenation action.
 */
int executeConcatenation(const std::string& concatName) {
    auto it = concatenations.find(concatName);
    if (it == concatenations.end()) {
        std::cerr << "Concatenation not found: " << concatName << std::endl;
        return -1;
    }

    for (const auto& partName : it->second.parts) {
        // Check if the part is a node
        if (nodes.find(partName) != nodes.end()) {
            int status = executeNode(partName);
            if (status != 0) {
                std::cerr << "Command failed: " << partName << std::endl;
                return status;
            }
        } else if (concatenations.find(partName) != concatenations.end()) {
            // If the part is another concatenation, execute it recursively
            int status = executeConcatenation(partName);
            if (status != 0) {
                std::cerr << "Concatenation failed: " << partName << std::endl;
                return status;
            }
        } else {
            std::cerr << "Unknown part: " << partName << std::endl;
            return -1;
        }
    }
    return 0;
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
    if (!parseFlowFile(flowFile)) {
        return EXIT_FAILURE;
    }

    // Check if the action is a concatenation
    if (concatenations.find(action) != concatenations.end()) {
        int status = executeConcatenation(action);
        if (status != 0) {
            std::cerr << "Execution failed for action: " << action << std::endl;
            return EXIT_FAILURE;
        }
    } else if (nodes.find(action) != nodes.end()) {
        // If the action is a node, execute it directly
        int status = executeNode(action);
        if (status != 0) {
            std::cerr << "Execution failed for node: " << action << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        std::cerr << "Action not found: " << action << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}