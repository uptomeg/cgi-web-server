all : simple die large server

simple : simple.o cgi.o
	gcc -Wall -g -o $@ $^

die : die.o 
	gcc -Wall -g -o $@ $^

large : large.o cgi.o
	gcc -Wall -g -o $@ $^
	
server : server.o
	gcc -Wall -g -o $@ $^

%.o : %.c 
	gcc -Wall -g -c $<
	
# "make tests" will run the four tests on test_constraints
tests : all
	./run_tests.sh msg1 msg1.expect
	./run_tests.sh msg2 msg2.expect
	./run_tests.sh msg3 msg3.expect
	./run_tests.sh msg4 msg4.expect
	./run_tests.sh msg5 msg5.expect
	./run_tests.sh msg6 msg6.expect

# Dependencies for header files
# In practice there are tools to automatically generate these dependencies
cgi.o : cgi.h

clean : 
	rm *.o simple die large server
