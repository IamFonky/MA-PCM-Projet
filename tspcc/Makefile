#  Copyright (c) 2012 Marcelo Pasin. All rights reserved.

CFLAGS=-O3 -Wall
LDFLAGS=-O3 -lm

all: tspcc queue_path_test atomic

tspcc: tspcc.o
	c++ -o tspcc $(LDFLAGS) tspcc.o
	
tspcc.o: tspcc.cpp graph.hpp path.hpp tspfile.hpp atomic_queue.hpp lifo.hpp
	c++ $(CFLAGS) -c tspcc.cpp

tspcc_orig: tspcc_orig.o
	c++ -o tspcc_orig $(LDFLAGS) tspcc_orig.o
	
tspcc_orig.o: tspcc_orig.cpp graph.hpp path.hpp tspfile.hpp atomic_queue.hpp
	c++ $(CFLAGS) -c tspcc_orig.cpp

queue_path_test: queue_path_test.o
	c++ -o queue_path_test $(LDFLAGS) queue_path_test.o

queue_path_test.o: queue_path_test.cpp graph.hpp path.hpp tspfile.hpp atomic_queue.hpp
	c++ $(CFLAGS) -c queue_path_test.cpp

atomic: atomic.cpp
	g++ -latomic -o atomic atomic.cpp

omp:
	make tspcc CFLAGS="-fopenmp -O3" LDFLAGS="-fopenmp -O3"

clean:
	rm -f *.o tspcc atomic
