# Variable to control the flow file and source file name (change this to flow1, flow2, etc.)
FLOWX = flow4

# Source and flow files are based on the FLOWX variable
SRC = $(FLOWX).cpp
EXEC = $(FLOWX)
FLOW1 = flow1.flow
FLOW2 = flow2.flow

.PHONY: all clean compile run

# Default target: clean, compile, and run
all: clean compile run

# Clean target: delete the executable if it exists
clean:
	@echo "Cleaning up $(EXEC)..."
	@if [ -f $(EXEC) ]; then rm -f $(EXEC); fi

# Compile target: compile the source file into an executable with the name $(EXEC)
compile:
	@echo "Compiling $(SRC) into $(EXEC)..."
	@g++ -std=c++11 $(SRC) -o $(EXEC)

# Run target: run the flow1.flow and flow2.flow, and compare results with real commands
run:
	@echo "\nRunning $(EXEC) with $(FLOW1)..."
	./$(EXEC) $(FLOW1) doit
	@echo "\nComparing with real command: ls | wc"
	@ls | wc
	
	@echo "\nRunning $(EXEC) with $(FLOW2)..."
	./$(EXEC) $(FLOW2) shenanigan
	@echo "\nComparing with real command: ( cat foo.txt ; cat foo.txt | sed 's/o/u/g' ) | wc"
	@( cat foo.txt ; cat foo.txt | sed 's/o/u/g' ) | wc
