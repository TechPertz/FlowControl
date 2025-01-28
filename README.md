# FlowControl: Command Sequence Execution Framework

## Overview

FlowControl is a C++ framework designed for interpreting and executing sequences of commands described in `.flow` files. This framework simulates shell command execution, including complex piping, redirection, and concatenation, tailored for Unix-like systems. It's an ideal tool for automating and testing command line operations, making it highly useful for developers and system administrators.

## Features

- **Command Execution**: Directly execute Unix commands through structured scripts.
- **Piping and Redirections**: Supports complex piping (`|`) and redirections (`>`, `2>&1`).
- **Concatenations**: Execute commands in sequence using concatenation (`;`).
- **File Output Handling**: Direct command outputs to files for logging and further processing.
- **Error Handling**: Advanced error redirection and handling for robust script testing.
- **Automated Testing**: Includes a Python script for automated testing against expected outputs.

## Installation

Ensure you have GCC (supporting C++11 or later) and Python 3.x installed. Clone the repository and compile the source:

```bash
git clone https://yourrepositorylink.com/FlowControl.git
cd FlowControl
g++ -std=c++11 flow.cpp -o flow
```

## Usage Guide

`.flow` files dictate the operations within the framework. Here’s how to structure these files:

### Basic Command Node

Defines a single command operation.

```plaintext
node=ls_node
command=ls -l
```

### Piping Output

Directs the output of one command to another.

```plaintext
node=echo_node
command=echo "Hello World"

node=wc_node
command=wc -w

pipe=echo_to_wc
from=echo_node
to=wc_node
```

### Concatenating Commands

Executes multiple commands in sequence.

```plaintext
node=date_node
command=date

node=whoami_node
command=whoami

concatenate=system_info
parts=2
part_0=date_node
part_1=whoami_node
```

### Error Handling Example

Redirects error output to another command.

```plaintext
node=mkdir_attempt
command=mkdir existing_dir

stderr=stdout_to_stderr_for_mkdir
from=mkdir_attempt

pipe=catch_errors
from=stdout_to_stderr_for_mkdir
to=word_count_node
```

### File Output Handling

Captures the output of a command and writes it to a file.

```plaintext
node=list_files
command=ls

file=output_file
name=output.txt

pipe=process_pipe
from=list_files
to=output_file
```

## Testing Framework

Run `python3 run.py` to test all `.flow` files in the `files` directory. This script compiles the program, runs tests, and saves results to `TestResult_{DateTime}.txt`.

## Different Cases

- **Case 0**: A single command node.
- **Case 1**: Command output redirection to a file.
- **Case 2**: Piping commands.
- **Case 3**: Concatenating commands.
- **Case 4**: Error handling.
- **Case 5**: File input/output handling.
- **Case 6**: Complex command sequences.

## To test:

1. Download and extract this repository.
2. Run `python3 run.py`
3. Open `TestResult_{current_time}.txt` for more information.

### Test Cases

- **0**: just one node in any `.flow` file
- **1**: `ls | wc`      // normal pipe
- **2**: `ls -l | wc`       // if `-` arguments can be handled properly
- **3**: `ls -l | ls`       // if pipe can be used for a "to" argument that doesn't take inputs
- **4**: `echo foo | cat`       // no quotes for argument
- **5**: `echo 'foo' | cat`        // single quotes 
- **6**: `echo "foo" | cat`        // double quotes
- **7**: `echo 'f o o' | cat`       // quotes with space
- **8**: `ls ; pwd`       // normal concat
- **9**: `ls; ls ; ls -a`     // concat with more than 2 parts and also arguments
- **10**: `echo foo1 ; echo 'foo2' ; echo "foo3" ; echo 'f o o 4'`       // no quotes; double; single; spaces
- **11**: `ls | wc ; pwd`       // mix
- **12**: `( cat foo.txt ; cat foo.txt | sed s/o/u/g ) | wc`     // complicated mix
- **13**: `( seq 1 5 | awk '{print $1*$1}'; seq 1 5 | awk '{print $1*2}'; seq 1 5 | awk '{print $1+5}' ) | sort -n | uniq` // extra credit - chatgpt - complicated with quotes, arguments, everything
- **14**: `mkdir a 2>&1`       // b would not exist in the directory (calling stderr as action directly)
- **15**: `mldir a 2>&1 | wc`   // calling non-stderr as action  
- **16**: `ls > output.txt`
- **17**: `cat output.txt | wc`