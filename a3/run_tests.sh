#!/bin/bash

# test to see whether the output of test_constraints matches the expected output

# $# is the number of command line arguments 
if [ $# -ne 2 ]
then 
    echo "Usage: ./runtests.sh input expected_output"
    exit 1
fi

# $1 is the first command line argument and $2 is the second
./server $1 > tmp.out
diff tmp.out $2
if [ $? -eq 0 ]
then
    echo "Test passed"
    rm tmp.out
else 
    echo "Test failed: check tmp.out for output"
fi
