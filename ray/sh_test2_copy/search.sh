#!bin/bash
#First you can use grep (-n) to find the number of lines of string.
grep -n "$2" $1 | cut -f1 -d: > $3

#Then you can use awk to separate the answer.

