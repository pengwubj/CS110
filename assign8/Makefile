# CS110 Assignment 8 Makefile
CXX = /usr/bin/g++-5

# The CPPFLAGS variable sets compile flags for g++:
#  -g          compile with debug information
#  -Wall       give all diagnostic warnings
#  -pedantic   require compliance with ANSI standard
#  -O0         do not optimize generated code
#  -std=c++0x  go with the c++0x experimental extensions for thread support (and other nifty things)
#  -D_GLIBCXX_USE_NANOSLEEP included for this_thread::sleep_for and this_thread::sleep_until support
#  -D_GLIBCXX_USE_SCHED_YIELD included for this_thread::yield support
CPPFLAGS = -g -Wall -pedantic -O0 -std=c++0x -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD -I/afs/ir/class/cs110/local/include/ -I/afs/ir/class/cs110/include

# The LDFLAGS variable sets flags for linker
# -lm        link to libm (math library)
# -pthread   link in libpthread (thread library) to back C++11 extensions
# -lthreads  link to course-specific concurrency functions and classes
# -lrand     link to a C++11-backed random number generator module
# -socket++  link to third party socket++ library for sockbuf and iosockstream classes
LDFLAGS = -lm -lpthread -L/afs/ir/class/cs110/local/lib -lthreadpool -L/afs/ir/class/cs110/local/lib -lthreads -L/afs/ir/class/cs110/local/lib -lrand -L/afs/ir/class/cs110/lib/socket++ -lsocket++ -Wl,-rpath=/afs/ir/class/cs110/lib/socket++

# In this section, you list the files that are part of the project.
# If you add/change names of header/source files, here is where you
# edit the Makefile.
PROGRAMS = mr.cc mrm.cc mrr.cc word-count-mapper.cc word-count-reducer.cc
EXTRAS = mapreduce-server.cc mapreduce-worker.cc mapreduce-mapper.cc mapreduce-reducer.cc \
         client-socket.cc server-socket.cc mr-nodes.cc mr-messages.cc mr-env.cc mr-utils.cc mr-random.cc \
         mr-names.cc
HEADERS = $(EXTRAS:.cc=.h) mapreduce-server-exception.h thread-pool.h
SOURCES = $(PROGRAMS) $(EXTRAS)
OBJECTS = $(SOURCES:.cc=.o)
TARGETS = $(PROGRAMS:.cc=)

default: $(TARGETS)

directories:
	rm -fr files/intermediate
	rm -fr files/output
	mkdir -p files/intermediate
	mkdir -p files/output

mr: mr.o mapreduce-server.o server-socket.o client-socket.o mr-nodes.o mr-messages.o mr-env.o mr-utils.o mr-names.o
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

mrm: mrm.o mapreduce-worker.o mapreduce-mapper.o client-socket.o mr-messages.o mr-env.o mr-names.o
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

mrr: mrr.o mapreduce-worker.o mapreduce-reducer.o client-socket.o mr-messages.o mr-env.o mr-names.o
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

word-count-mapper: word-count-mapper.o mr-random.o
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

word-count-reducer: word-count-reducer.o mr-random.o
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

# In make's default rules, a .o automatically depends on its .cc file
# (so editing the .cc will cause recompilation into its .o file).
# The line below creates additional dependencies, most notably that it
# will cause the .cc to recompiled if any included .h file changes.

Makefile.dependencies:: $(SOURCES) $(HEADERS)
	$(CXX) $(CPPFLAGS) -MM $(SOURCES) > Makefile.dependencies

-include Makefile.dependencies

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
# The phony target "spartan" is used to remove all compilation products and extra backup files. 

.PHONY: clean spartan

filefree:
	rm -fr files/intermediate/* files/output/*	

clean: filefree
	rm -f $(TARGETS) $(OBJECTS) core Makefile.dependencies

spartan: clean
	rm -f *~
