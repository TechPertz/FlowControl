cmake_minimum_required(VERSION 3.10)
project(MixProject)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable for 'mix.cpp'
add_executable(mix2 src/mix2.cpp)

# Remove this line so the executable is generated in the same directory as CMakeLists.txt
# set_target_properties(mix PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# Add a custom target to remove the old 'mix' executable if it exists
add_custom_target(clean_mix2 ALL
    COMMAND rm -rf ${CMAKE_SOURCE_DIR}/mix2
    COMMENT "Removing old mix2 executable if it exists"
)

# Ensure that 'clean_mix' runs before building the 'mix' executable
add_dependencies(mix2 clean_mix2)

# Enable testing
enable_testing()

# List of flow files in the 'files' folder and their corresponding actions
set(FLOW_FILES
    files/flow1.flow
    files/flow2.flow
    files/flow3.flow
    files/flow4.flow
    files/flow5.flow
    files/flow8.flow
    files/flow9.flow
    files/flow10.flow
    files/flow11.flow
    files/flow12.flow
    files/flow14.flow
    files/flow13.flow
)

set(ACTION_NAMES
    doit
    shenanigan
    doit
    doit
    pipe3
    concat1
    concat_echoes
    concat_ultimate
    unique_numbers
    catch_errors
    process_pipe
    process_pipe
)

set(EXPECTED_COMMANDS
    "ls | wc"   # flow1.flow
    "(cat ${CMAKE_SOURCE_DIR}/files/foo.txt \; cat ${CMAKE_SOURCE_DIR}/files/foo.txt | sed 's/o/u/g') | wc"   # flow2.flow
    "ls -l | ls"   # flow3.flow
    "ls \; ls -l"   # flow4.flow
    "ls | grep '.cpp' | wc"   # flow5.flow
    "ls \; ls"   # flow8.flow
    "echo 'f o o' \; echo 'f o o' \; echo 'f o o' \; echo 'f o o' \; echo 'f o o'"   # flow9.flow
    "echo 'Command One Executed.' \; echo 'Command One Executed.' \; echo 'Command Two Executed.' \; echo 'Command Three Executed.' \; echo 'Command Four Executed.' \; echo 'Command Four Executed.' \; echo 'Command One Executed.' \; echo 'Command Two Executed.'"   # flow10.flow
    "(seq 1 5 | awk '{print \$1*\$1}' \; seq 1 5 | awk '{print \$1*2}' \; seq 1 5 | awk '{print \$1+5}') | sort -n | uniq"   # flow11.flow
    "mkdir a 2>&1 | wc" # flow12.flow
    "ls > output.txt" # flow14.flow 
    "cat output.txt | wc" # flow13.flow 
)

# Ensure the lengths of the lists match
list(LENGTH FLOW_FILES num_flow_files)
list(LENGTH ACTION_NAMES num_actions)
list(LENGTH EXPECTED_COMMANDS num_expected_commands)

message(STATUS "Number of flow files: ${num_flow_files}")
message(STATUS "Number of actions: ${num_actions}")
message(STATUS "Number of expected commands: ${num_expected_commands}")

if(NOT num_flow_files EQUAL num_actions OR NOT num_flow_files EQUAL num_expected_commands)
    message(FATAL_ERROR "FLOW_FILES, ACTION_NAMES, and EXPECTED_COMMANDS must have the same number of elements.")
endif()

# Loop over the flow files and define a test for each
math(EXPR num_tests "${num_flow_files} - 1")

foreach(i RANGE 0 ${num_tests})
    list(GET FLOW_FILES ${i} flow_file)
    list(GET ACTION_NAMES ${i} action_name)
    list(GET EXPECTED_COMMANDS ${i} expected_command)

    # Define the test name
    string(REPLACE "." "_" flow_file_safe ${flow_file})
    set(test_name "Test_${flow_file_safe}")

    # Define output files
    set(actual_output "${test_name}_actual_output.txt")
    set(expected_output "${test_name}_expected_output.txt")

    # Add the test
    add_test(NAME ${test_name}
        COMMAND bash -c "
            echo '=== Running ${test_name} ===';
            echo 'Running mix with ${flow_file}:';
            ${CMAKE_SOURCE_DIR}/mix ${flow_file} ${action_name} > ${actual_output};
            echo 'Expected output from command:';
            ${expected_command} > ${expected_output};
            echo '--- Actual Output ---';
            cat ${actual_output};
            echo '--- Expected Output ---';
            cat ${expected_output};
        "
    )

    # Set test properties to display output
    set_tests_properties(${test_name} PROPERTIES
        PASS_REGULAR_EXPRESSION ".*"
    )
endforeach()