# OS_HW_2

## To test:

1. Download and extract this repository.
2. Run `python3 run.py`
3. Open TestResult_{current_time}.txt for more information.

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