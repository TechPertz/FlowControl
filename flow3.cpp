#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#define MAX_LEN 1024

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
    std::cout << "Parsed node: " << name << " with command: ";
    for (const auto &cmd : node.command) {
        std::cout << cmd << " ";
    }
    std::cout << "\n";
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
    std::cout << "Parsed pipe: " << pipe_name << " from " << from << " to " << to << "\n";
}

// Function to parse and store a concatenation
void parse_concatenation(const std::string &line, std::ifstream &flow_file) {
    std::string concat_name, parts_line;
    int part_count;

    // Extract concatenation name
    std::string prefix = "concatenate=";
    if (line.substr(0, prefix.length()) == prefix) {
        concat_name = line.substr(prefix.length());
    } else {
        std::cerr << "Error: Invalid concatenation format: " << line << "\n";
        return;
    }

    std::getline(flow_file, parts_line);
    sscanf(parts_line.c_str(), "parts=%d", &part_count);

    Concatenation concat;
    concat.name = concat_name;

    // Parse individual parts
    for (int i = 0; i < part_count; ++i) {
        std::string part;
        std::getline(flow_file, part);
        part = part.substr(part.find('=') + 1); // Extract part name
        concat.parts.push_back(part);
        std::cout << "Added part " << concat.parts[i] << " to concatenation " << concat_name << "\n";
    }

    concatenations[concat_name] = concat;
    std::cout << "Parsed concatenation: " << concat_name << " with " << part_count << " parts\n";
}

// Function to parse the flow file
void parse_flow_file(std::ifstream &flow_file) {
    std::string line;

    // Parse the flow file line by line
    while (std::getline(flow_file, line)) {
        std::cout << "Read line: " << line << "\n";
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <flow_file>\n";
        exit(1);
    }

    std::ifstream flow_file(argv[1]);
    if (!flow_file.is_open()) {
        std::cerr << "Error: Could not open flow file " << argv[1] << "\n";
        exit(1);
    }

    std::cout << "Debug: Opening flow file " << argv[1] << "\n";
    parse_flow_file(flow_file);

    return 0;
}