import os
import subprocess
import datetime

flow_tests = {
    "0.flow": ("path", "pwd"),
    "1.flow": ("doit", "ls | wc"),
    "2.flow": ("doit", "ls -l | wc"),
    "3.flow": ("doit", "ls -l | ls"),
    "4.flow": ("doit", "echo foo | cat"),
    "5.flow": ("doit", "echo 'foo' | cat"),
    "6.flow": ("doit", "echo \"foo\" | cat"),
    "7.flow": ("doit", "echo 'f o o' | cat"),
    "8.flow": ("doit", "ls ; pwd"),
    "9.flow": ("doit", "ls; ls ; ls -a"),
    "10.flow": ("doit", "echo foo1 ; echo 'foo2' ; echo \"foo3\" ; echo 'f o o 4'"),
    "11.flow": ("doit", "ls | wc ; pwd"),
    "12.flow": ("doit", "( cat foo.txt ; cat foo.txt | sed s/o/u/g ) | wc"),
    "13.flow": ("doit", "( seq 1 5 | awk '{print $1*$1}'; seq 1 5 | awk '{print $1*2}'; seq 1 5 | awk '{print $1+5}' ) | sort -n | uniq"),
    "14.flow": ("doit", "mkdir a 2>&1"),
    "15.flow": ("doit", "mkdir a 2>&1 | wc"),
    "16.flow": ("doit", "ls > output.txt"),
    "17.flow": ("doit", "cat output.txt | wc")
}

def compile_flow_cpp():
    if os.path.exists('flow.cpp'):
        print("flow.cpp file found.")
        if os.path.exists('flow'):
            os.remove('flow')
            print("Removed existing 'flow' executable.")
        print("Compiling flow.cpp...")
        subprocess.run("g++ -std=c++11 flow.cpp -o flow", shell=True)
        print("Compiled.")
        return True
    else:
        print("flow.cpp file not found.")
        return False

def run_tests():
    current_time = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    results_filename = f"TestResult_{current_time}.txt"
    with open(results_filename, 'w') as result_file:
        for i, (flow_file, (action, command)) in enumerate(flow_tests.items()):
            flow_path = flow_file
            result = subprocess.run(f"./flow files/{flow_path} {action}", shell=True, capture_output=True, text=True)
            expected_output = subprocess.run(command, shell=True, capture_output=True, text=True)

            # Write detailed results to file
            result_file.write(f"\nTest {i} : {command}\n")
            result_file.write("------PROGRAM OUTPUT--------\n")
            result_file.write(result.stdout + '\n')
            result_file.write("-------ACTUAL OUTPUT-------\n")
            result_file.write(expected_output.stdout + '\n')
            result_file.write("*************************************************************************\n")

            # Print simple results to terminal
            if result.stdout.strip() == expected_output.stdout.strip():
                print(f"Test {i} passed.")
            else:
                print(f"Test {i} failed.")
    
    print(f"\nAll tests completed. Results saved to {results_filename}.\n")

if __name__ == "__main__":
    if compile_flow_cpp():
        run_tests()