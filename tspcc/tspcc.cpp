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
#include <chrono>

// #define QUEUE_SIZE 16
// #define NB_THREADS 16

enum Verbosity
{
	VER_NONE = 0,
	VER_GRAPH = 1,
	VER_SHORTER = 2,
	VER_BOUND = 4,
	VER_ANALYSE = 8,
	VER_COUNTERS = 16,
	VER_QUEUE = 32,
	VER_LOG_STAT = 64,
	VER_ALL = 255
};

using Element = Path *;															 // Queue element type.
Element constexpr NIL = static_cast<Element>(NULL);								 // Atomic elements require a special value that cannot be pushed/popped.
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.

static struct
{
	AtomicPath *shortest;
	Queue *jobs;
	Verbosity verbose;
	struct
	{
		int verified; // # of paths checked
		int found;	  // # of times a shorter path was found
		int *bound;	  // # of bound operations per level
	} counter;
	int size;
	int total; // number of paths to check
	int *fact;
	int nb_samples;
	int min_nb_threads;
	int max_nb_threads;
	int nb_threads;
	int min_queue_size;
	int max_queue_size;
	int queue_size;
} global;

static const struct
{
	char RED[6];
	char BLUE[6];
	char ORIGINAL[6];
} COLOR = {
	.RED = {27, '[', '3', '1', 'm', 0},
	.BLUE = {27, '[', '3', '6', 'm', 0},
	.ORIGINAL = {27, '[', '3', '9', 'm', 0},
};

static void branch_and_bound(Path *current)
{
	if (global.verbose & VER_ANALYSE)
		std::cout << "analysing " << current << '\n';

	if (current->leaf())
	{
		// this is a leaf
		current->add(0);
		if (global.verbose & VER_COUNTERS)
			global.counter.verified++;
		if (current->distance() < global.shortest->distance())
		{
			if (global.verbose & VER_SHORTER)
				std::cout << "shorter: " << current << '\n';
			global.shortest->copyIfShorter(current);
			if (global.verbose & VER_COUNTERS)
				global.counter.found++;
		}
		current->pop();
	}
	else
	{
		// not yet a leaf
		if (current->distance() < global.shortest->distance())
		{
			// continue branching
			for (int i = 1; i < current->max(); i++)
			{
				if (!current->contains(i))
				{
					current->add(i);
					// Vérif si queue est dispo
					// si oui, écrire dans la queue
					bool pushed = false;
					if (global.jobs->get_size() < global.queue_size)
					{
						Path *newPath = new Path(current);
						global.jobs->push(newPath);
						if (global.verbose & VER_QUEUE)
						{
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
					if (!pushed)
					{
						branch_and_bound(current);
					}

					current->pop();
				}
			}
		}
		else
		{
			// current already >= shortest known so far, bound
			if (global.verbose & VER_BOUND)
				std::cout << "bound " << current << '\n';
			if (global.verbose & VER_COUNTERS)
				global.counter.bound[current->size()]++;
		}
	}
}

static void *branch_and_bound_task(void *arg)
{
	int IS_RUNNING = 1;
	while (!global.jobs->was_empty())
	{
		// if(!global.jobs->was_empty()){
		Path *job_to_do = global.jobs->pop();
		if (global.verbose & VER_QUEUE)
		{
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
	for (int i = 0; i < global.size; i++)
	{
		global.counter.bound[i] = 0;
		if (i)
		{
			int pos = global.size - i;
			global.fact[pos] = (i - 1) ? (i * global.fact[pos + 1]) : 1;
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
	for (int i = 0; i < global.size; i++)
		std::cout << ' ' << global.counter.bound[i];
	std::cout << "\nbound equivalent (per level): ";
	int equiv = 0;
	for (int i = 0; i < global.size; i++)
	{
		int e = global.fact[i] * global.counter.bound[i];
		std::cout << ' ' << e;
		equiv += e;
	}
	std::cout << "\nbound equivalent (total): " << equiv << '\n';
	std::cout << "check: total " << (global.total == (global.counter.verified + equiv) ? "==" : "!=") << " verified + total bound equivalent\n";
}

Graph *parse_args(int argc, char *argv[])
{

	int opt;

	option longopts[] = {
		{"file", required_argument, NULL, 'f'},
		{"verbosity", required_argument, NULL, 'v'},
		{"samples-count", required_argument, NULL, 's'},
		{"thread-count-min", required_argument, NULL, 't'},
		{"thread-count-max", required_argument, NULL, 'T'},
		{"queue-size-min", required_argument, NULL, 'q'},
		{"queue-size-max", required_argument, NULL, 'Q'},
	};

	Graph *graph = NULL;
	global.verbose = VER_NONE;
	global.nb_samples = 1;
	global.min_nb_threads = 2;
	global.max_nb_threads = -1;
	global.min_queue_size = 10;
	global.max_queue_size = -1;

	while ((opt = getopt_long(argc, argv, "f:v:s:t:T:q:Q:", longopts, 0)) != -1)
	{
		switch (opt)
		{
		case 'f':
			graph = TSPFile::graph(optarg);
			break;
		case 'v':
			global.verbose = (Verbosity)atoi(optarg);
			break;
		case 's':
			global.nb_samples = atoi(optarg);
			break;
		case 't':
			global.min_nb_threads = atoi(optarg);
			break;
		case 'T':
			global.max_nb_threads = atoi(optarg);
			break;
		case 'q':
			global.min_queue_size = atoi(optarg);
			break;
		case 'Q':
			global.max_queue_size = atoi(optarg);
			break;
		}
	}

	if (global.max_nb_threads < global.min_nb_threads)
		global.max_nb_threads = global.min_nb_threads;
	if (global.max_queue_size < global.min_queue_size)
		global.max_queue_size = global.min_queue_size;

	if (graph == NULL)
	{
		perror("you need to provide a file name with the option -f\n");
		exit(1);
	}

	return graph;
}

int main(int argc, char *argv[])
{
	Graph *g = parse_args(argc, argv);

	for (int nb_threads = global.min_nb_threads; nb_threads <= global.max_nb_threads; ++nb_threads)
	{
		for (int queue_size = global.min_queue_size; queue_size <= global.max_queue_size; ++queue_size)
		{
			for (int sample = 0; sample < global.nb_samples; ++sample)
			{
				global.nb_threads = nb_threads;
				global.queue_size = queue_size;

				auto start = std::chrono::high_resolution_clock::now();

				if (global.verbose != VER_LOG_STAT)
				{
					std::cout << "number of threads " << global.nb_threads << '\n';
					std::cout << "size of queue " << global.queue_size << '\n';
				}

				if (global.verbose & VER_GRAPH)
					std::cout << COLOR.BLUE << g << COLOR.ORIGINAL;

				if (global.verbose & VER_COUNTERS)
					reset_counters(g->size());

				global.shortest = new AtomicPath(g);
				for (int i = 0; i < g->size(); i++)
				{
					global.shortest->add(i);
				}
				global.shortest->add(0);

				Path *current = new Path(g);
				current->add(0);

				pthread_t *workers = new pthread_t[global.nb_threads];

				global.jobs = new Queue(global.queue_size);
				global.jobs->push(current);

				pthread_create(workers, NULL, branch_and_bound_task, NULL);
				// Waiting 1 sec before starting the other threads
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				for (int i = 1; i < global.nb_threads; ++i)
				{
					pthread_create(&(workers[i]), NULL, branch_and_bound_task, NULL);
				}

				for (int i = 0; i < global.nb_threads; ++i)
				{
					pthread_join(workers[i], NULL);
				}

				if (global.verbose != VER_LOG_STAT)
					std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

				if (global.verbose & VER_COUNTERS)
					print_counters();

				if (global.verbose & VER_LOG_STAT)
				{
					printf("%d;%d;%d;%ld\n",
						   global.nb_threads,
						   global.queue_size,
						   global.shortest->distance(),
						   std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count());
				}

				delete global.shortest;
				delete current;
				delete workers;
			}
		}
	}

	return 0;
}