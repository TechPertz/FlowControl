# FlowControl

flow5 can parse pipes normally.
flow6 can 2 nodes and 1node,1pipe
flow7 improved but same.
flow8 hadles concatenation.
mix mix.



Test Cases
0: just one node in any .flow file

1: ls | wc      // normal pipe
2: ls -l | wc       // if - arguments can be handled properly
3: ls -l | ls       // if pipe can be used for a "to" argument that doesnt take inputs
4: echo foo | wc       // no quotes for argument
4: echo 'foo2' | cat        // single quotes 
5: echo "foo1" | wc        // double quotes
6: echo 'f o o' | wc       // quotes with space

7: ls ; pwd       // normal concat
8: ls; ls ; ls -a     // concat with more than 2 parts and also arguments
9: echo foo1 ; echo 'foo2' ; echo "foo3" ; echo 'f o o 4'       // no quotes ; double ; single ; spaces

10: ls | wc ; pwd       // mix
11: ( cat foo.txt ; cat foo.txt | sed s/o/u/g ) | wc     // complicated mix

12: ( seq 1 5 | awk '{print $1*$1}'; seq 1 5 | awk '{print $1*2}'; seq 1 5 | awk '{print $1+5}' ) | sort -n | uniq       
// extra credit - chatgpt - complicated with quotes, arguments, everything

13: cd b 2>&1       // b would not exist in the directory
14: mkdir a 2>&1 | wc

15: ls > output.txt
16: cat output.txt | wc
