# CMake generated Testfile for 
# Source directory: /Users/reetnandy/vscode/OS_HW_2
# Build directory: /Users/reetnandy/vscode/OS_HW_2/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(Test_files/flow1_flow "bash" "-c" "
            echo '=== Running Test_files/flow1_flow ===';
            echo 'Running mix with files/flow1.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow1.flow doit > Test_files/flow1_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls | wc > Test_files/flow1_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow1_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow1_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow1_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow2_flow "bash" "-c" "
            echo '=== Running Test_files/flow2_flow ===';
            echo 'Running mix with files/flow2.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow2.flow shenanigan > Test_files/flow2_flow_actual_output.txt;
            echo 'Expected output from command:';
            (cat /Users/reetnandy/vscode/OS_HW_2/files/foo.txt ; cat /Users/reetnandy/vscode/OS_HW_2/files/foo.txt | sed 's/o/u/g') | wc > Test_files/flow2_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow2_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow2_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow2_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow3_flow "bash" "-c" "
            echo '=== Running Test_files/flow3_flow ===';
            echo 'Running mix with files/flow3.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow3.flow doit > Test_files/flow3_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls -l | ls > Test_files/flow3_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow3_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow3_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow3_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow4_flow "bash" "-c" "
            echo '=== Running Test_files/flow4_flow ===';
            echo 'Running mix with files/flow4.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow4.flow doit > Test_files/flow4_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls ; ls -l > Test_files/flow4_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow4_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow4_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow4_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow5_flow "bash" "-c" "
            echo '=== Running Test_files/flow5_flow ===';
            echo 'Running mix with files/flow5.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow5.flow pipe3 > Test_files/flow5_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls | grep '.cpp' | wc > Test_files/flow5_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow5_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow5_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow5_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow8_flow "bash" "-c" "
            echo '=== Running Test_files/flow8_flow ===';
            echo 'Running mix with files/flow8.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow8.flow concat1 > Test_files/flow8_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls ; ls > Test_files/flow8_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow8_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow8_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow8_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow9_flow "bash" "-c" "
            echo '=== Running Test_files/flow9_flow ===';
            echo 'Running mix with files/flow9.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow9.flow concat_echoes > Test_files/flow9_flow_actual_output.txt;
            echo 'Expected output from command:';
            echo 'f o o' ; echo 'f o o' ; echo 'f o o' ; echo 'f o o' ; echo 'f o o' > Test_files/flow9_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow9_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow9_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow9_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow10_flow "bash" "-c" "
            echo '=== Running Test_files/flow10_flow ===';
            echo 'Running mix with files/flow10.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow10.flow concat_ultimate > Test_files/flow10_flow_actual_output.txt;
            echo 'Expected output from command:';
            echo 'Command One Executed.' ; echo 'Command One Executed.' ; echo 'Command Two Executed.' ; echo 'Command Three Executed.' ; echo 'Command Four Executed.' ; echo 'Command Four Executed.' ; echo 'Command One Executed.' ; echo 'Command Two Executed.' > Test_files/flow10_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow10_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow10_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow10_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow11_flow "bash" "-c" "
            echo '=== Running Test_files/flow11_flow ===';
            echo 'Running mix with files/flow11.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow11.flow unique_numbers > Test_files/flow11_flow_actual_output.txt;
            echo 'Expected output from command:';
            (seq 1 5 | awk '{print \$1*\$1}' ; seq 1 5 | awk '{print \$1*2}' ; seq 1 5 | awk '{print \$1+5}') | sort -n | uniq > Test_files/flow11_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow11_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow11_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow11_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow12_flow "bash" "-c" "
            echo '=== Running Test_files/flow12_flow ===';
            echo 'Running mix with files/flow12.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow12.flow catch_errors > Test_files/flow12_flow_actual_output.txt;
            echo 'Expected output from command:';
            mkdir a 2>&1 | wc > Test_files/flow12_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow12_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow12_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow12_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow14_flow "bash" "-c" "
            echo '=== Running Test_files/flow14_flow ===';
            echo 'Running mix with files/flow14.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow14.flow process_pipe > Test_files/flow14_flow_actual_output.txt;
            echo 'Expected output from command:';
            ls > output.txt > Test_files/flow14_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow14_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow14_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow14_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
add_test(Test_files/flow13_flow "bash" "-c" "
            echo '=== Running Test_files/flow13_flow ===';
            echo 'Running mix with files/flow13.flow:';
            /Users/reetnandy/vscode/OS_HW_2/mix files/flow13.flow process_pipe > Test_files/flow13_flow_actual_output.txt;
            echo 'Expected output from command:';
            cat output.txt | wc > Test_files/flow13_flow_expected_output.txt;
            echo '--- Actual Output ---';
            cat Test_files/flow13_flow_actual_output.txt;
            echo '--- Expected Output ---';
            cat Test_files/flow13_flow_expected_output.txt;
        ")
set_tests_properties(Test_files/flow13_flow PROPERTIES  PASS_REGULAR_EXPRESSION ".*" _BACKTRACE_TRIPLES "/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;102;add_test;/Users/reetnandy/vscode/OS_HW_2/CMakeLists.txt;0;")
