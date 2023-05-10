//
//  tspcc.cpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include "graph.hpp"
#include "path.hpp"
#include "atomic_path.hpp"
#include "tspfile.hpp"
#include "include/atomic_queue/atomic_queue.h"

#include <pthread.h>
#include <getopt.h>
#include <queue>
#include <chrono>
#include <thread>

#define QUEUE_SIZE 16
// #define NB_THREADS 16

enum Verbosity {
	VER_NONE = 0,
	VER_GRAPH = 1,
	VER_SHORTER = 2,
	VER_BOUND = 4,
	VER_ANALYSE = 8,
	VER_COUNTERS = 16,
	VER_QUEUE = 32,
	VER_ALL = 255
};

using Element = Path*;                                                        // Queue element type.
Element constexpr NIL = static_cast<Element>(NULL);                                // Atomic elements require a special value that cannot be pushed/popped.
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.

static struct {
	AtomicPath* shortest;
	Queue* jobs;
	Verbosity verbose;
	struct {
		int verified;	// # of paths checked
		int found;	// # of times a shorter path was found
		int* bound;	// # of bound operations per level
	} counter;
	int size;
	int total;		// number of paths to check
	int* fact;
	int nb_threads;
} global;

static const struct {
	char RED[6];
	char BLUE[6];
	char ORIGINAL[6];
} COLOR = {
	.RED = { 27, '[', '3', '1', 'm', 0 },
	.BLUE = { 27, '[', '3', '6', 'm', 0 },
	.ORIGINAL = { 27, '[', '3', '9', 'm', 0 },
};

static void branch_and_bound(Path* current)
{
	if (global.verbose & VER_ANALYSE)
		std::cout << "analysing " << current << '\n';

	if (current->leaf()) {
		// this is a leaf
		current->add(0);
		if (global.verbose & VER_COUNTERS)
			global.counter.verified ++;
		if (current->distance() < global.shortest->distance()) {
			if (global.verbose & VER_SHORTER)
				std::cout << "shorter: " << current << '\n';
			global.shortest->copyIfShorter(current);
			if (global.verbose & VER_COUNTERS)
				global.counter.found ++;
		}
		current->pop();
	} else {
		// not yet a leaf
		if (current->distance() < global.shortest->distance()) {
			// continue branching
			for (int i=1; i<current->max(); i++) {
				if (!current->contains(i)) {
					current->add(i);
					// Vérif si queue est dispo
					// si oui, écrire dans la queue
					bool pushed = false;
					if(global.jobs->get_size() < QUEUE_SIZE){
						Path* newPath = new Path(current);
						global.jobs->push(newPath);
						if(global.verbose & VER_QUEUE){
							pthread_t tid = pthread_self();
							std::cout << "push in queue " << global.jobs->get_size() << '\n';
							std::cout << "path " << newPath << '\n';
							std::cout << "TID : " << tid << "\n";
						}
						pushed = true;
						// pushed = global.jobs->push(Path(current));
					}
					// si il n'y a pas eu de push
					// Continuer de taffer
					if(!pushed){
						branch_and_bound(current);
					}

					current->pop();
				}
			}
		} else {
			// current already >= shortest known so far, bound
			if (global.verbose & VER_BOUND )
				std::cout << "bound " << current << '\n';
			if (global.verbose & VER_COUNTERS)
				global.counter.bound[current->size()] ++;
		}
	}
}

static void* branch_and_bound_task(void *arg)
{	
	int IS_RUNNING = 1;
	while(!global.jobs->was_empty()){
		// if(!global.jobs->was_empty()){
			Path* job_to_do =  global.jobs->pop();
			if(global.verbose & VER_QUEUE){
				std::cout << "pop in queue " << global.jobs->get_size() << '\n';
				std::cout << "path " << job_to_do << '\n';
			}
			branch_and_bound(job_to_do);
		// }
	}
	return 0;
}

void reset_counters(int size)
{
	global.size = size;
	global.counter.verified = 0;
	global.counter.found = 0;
	global.counter.bound = new int[global.size];
	global.fact = new int[global.size];
	for (int i=0; i<global.size; i++) {
		global.counter.bound[i] = 0;
		if (i) {
			int pos = global.size - i;
			global.fact[pos] = (i-1) ? (i * global.fact[pos+1]) : 1;
		}
	}
	global.total = global.fact[0] = global.fact[1];
}

void print_counters()
{
	std::cout << "total: " << global.total << '\n';
	std::cout << "verified: " << global.counter.verified << '\n';
	std::cout << "found shorter: " << global.counter.found << '\n';
	std::cout << "bound (per level):";
	for (int i=0; i<global.size; i++)
		std::cout << ' ' << global.counter.bound[i];
	std::cout << "\nbound equivalent (per level): ";
	int equiv = 0;
	for (int i=0; i<global.size; i++) {
		int e = global.fact[i] * global.counter.bound[i];
		std::cout << ' ' << e;
		equiv += e;
	}
	std::cout << "\nbound equivalent (total): " << equiv << '\n';
	std::cout << "check: total " << (global.total==(global.counter.verified + equiv) ? "==" : "!=") << " verified + total bound equivalent\n";
}

// Graph* parse_args(int argc, char* argv[]){
// 	option longopts[] = {
//         {"file", required_argument, NULL, 'f'}, 
//         {"verbosity", optional_argument, NULL, 'v'}, 
//         {"thread-count", optional_argument, NULL, 't'}, 
//         {"queue-size", optional_argument, NULL, 'q'}, 
// 		{0}};

//     while (1) {
//         const int opt = getopt_long(argc, argv, "vtq::", longopts, 0);

//         if (opt == -1) {
//             break;
//         }

//         switch (opt) {
//             case 'f':
//                 // ...
// 				cvalue = optarg;
// 			break;
//             case 'v':
//                 // ...
// 			break;
//             case 't':
//                 // ...
// 			break;
//             case 'q':
//                 // ...
// 			break;
//         }
//     }
// }

int main(int argc, char* argv[])
{
	char* fname = 0;
	if (argc == 2) {
		fname = argv[1];
		// global.verbose = (Verbosity)(VER_QUEUE);
		// global.verbose = (Verbosity)(VER_ANALYSE);
		global.verbose = (Verbosity)(VER_NONE);
		global.nb_threads = 1;
	} else {
		if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'v') {
			global.verbose = (Verbosity) (argv[1][2] ? atoi(argv[1]+2) : 1);
			fname = argv[2];
		}
		if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 't') {
			global.nb_threads = (Verbosity) (argv[1][2] ? atoi(argv[1]+2) : 1);
			fname = argv[2];
		} else {
			fprintf(stderr, "usage: %s [-v#] filename\n", argv[0]);
			exit(1);
		}
	}

	std::cout << "number of threads " << global.nb_threads << '\n';
	std::cout << "size of queue " << QUEUE_SIZE << '\n';


	Graph* g = TSPFile::graph(fname);
	if (global.verbose & VER_GRAPH)
		std::cout << COLOR.BLUE << g << COLOR.ORIGINAL;

	if (global.verbose & VER_COUNTERS)
		reset_counters(g->size());

	global.shortest = new AtomicPath(g);
	for (int i=0; i<g->size(); i++) {
		global.shortest->add(i);
	}
	global.shortest->add(0);

	Path* current = new Path(g);
	current->add(0);

	// Start thread t1
    // pthread_t workers[NB_THREADS];
    // pthread_t* workers = (pthread_t*)malloc(global.nb_threads*sizeof(pthread_t));
    pthread_t* workers = new pthread_t[global.nb_threads];
 
	global.jobs = new Queue(QUEUE_SIZE);
	global.jobs->push(current);

	pthread_create(workers, NULL, branch_and_bound_task, NULL);
	// Waiting 1 sec before starting the other threads
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	for(int i = 1; i < global.nb_threads; ++i){
		pthread_create(&(workers[i]), NULL, branch_and_bound_task, NULL);
	}

	for(int i = 0; i < global.nb_threads; ++i){
		pthread_join(workers[i],NULL);
	}

	std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

	if (global.verbose & VER_COUNTERS)
		print_counters();

	return 0;
}
