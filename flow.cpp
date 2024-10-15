#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

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
        // Skip empty lines or lines that only contain whitespace
        if (line.empty() || line.find_first_not_of(' ') == std::string::npos) {
            continue;
        }

        std::cout << "Processing line: " << line << std::endl;  // Debug print

        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            std::cout << "Key: " << key << ", Value: " << value << std::endl;  // Debug print

            if (key.rfind("node", 0) == 0) {
                // Now handle nodes correctly, and expect the next line to be the command
                current_node = value;
                std::cout << "Found node: " << current_node << std::endl;
            } else if (key == "command" && !current_node.empty()) {
                // Now we're reading the command for the current node
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
                std::cout << "Parsed node: " << current_node << " with command: " << node.command << std::endl;
                current_node.clear();  // Clear current node after storing it
            } else if (key == "pipe") {
                // Parsing pipe
                std::string pipe_name = value;
                std::string from, to;

                // Parsing the 'from' line
                if (std::getline(file, line)) {
                    std::istringstream from_line(line);
                    std::string from_key;
                    if (std::getline(from_line, from_key, '=') && std::getline(from_line, from)) {
                        std::cout << "Parsed 'from' for pipe: " << from << std::endl;
                    } else {
                        std::cerr << "Error: Malformed 'from' line in pipe" << std::endl;
                    }
                }

                // Parsing the 'to' line
                if (std::getline(file, line)) {
                    std::istringstream to_line(line);
                    std::string to_key;
                    if (std::getline(to_line, to_key, '=') && std::getline(to_line, to)) {
                        std::cout << "Parsed 'to' for pipe: " << to << std::endl;
                    } else {
                        std::cerr << "Error: Malformed 'to' line in pipe" << std::endl;
                    }
                }

                pipes[pipe_name] = {from, to};
                std::cout << "Parsed pipe: " << pipe_name << " from: " << from << " to: " << to << std::endl;
            } else if (key == "concatenate") {
                // Parsing concatenation
                std::string concat_name = value;
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
                concatenations[concat_name] = concat;
                std::cout << "Parsed concatenation: " << concat_name << " with parts: " << concat.parts << std::endl;
            }
        } else {
            std::cerr << "Error: Malformed line '" << line << "'" << std::endl;
        }
    }
}

// Function to print parsed data (for debugging)
void printParsedData() {
    std::cout << "\nParsed Nodes:" << std::endl;
    for (const auto &pair : nodes) {
        std::cout << "Node Name: " << pair.first << ", Command: " << pair.second.command;
        if (!pair.second.args.empty()) {
            std::cout << ", Args: ";
            for (const auto &arg : pair.second.args) {
                std::cout << arg << " ";
            }
        }
        std::cout << std::endl;
    }

    std::cout << "\nParsed Pipes:" << std::endl;
    for (const auto &pair : pipes) {
        std::cout << "Pipe Name: " << pair.first << ", From: " << pair.second.first << ", To: " << pair.second.second << std::endl;
    }

    std::cout << "\nParsed Concatenations:" << std::endl;
    for (const auto &pair : concatenations) {
        std::cout << "Concatenation Name: " << pair.first << ", Parts: " << pair.second.parts << std::endl;
        for (const auto &part : pair.second.part_nodes) {
            std::cout << "  Part Node: " << part << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <flow_file>" << std::endl;
        return 1;
    }

    std::string flow_file = argv[1];
    parseFlowFile(flow_file);
    
    // Print parsed data to verify correctness
    printParsedData();

    return 0;
}