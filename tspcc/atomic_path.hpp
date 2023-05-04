//
//  path.hpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include <iostream>
#include <cstring>
#include "path.hpp"

#ifndef _atomic_path_hpp
#define _atomic_path_hpp

class AtomicPath : Path {
public:
	void copy(AtomicPath* o)
	{
		throw "NEED TO BE IMPLEMENTED!";
		if (max() != o->max()) {
			delete[] _nodes;
			_nodes = new int[o->max() + 1];
		}
		_graph = o->_graph;
		_size = o->_size;
		_distance = o->_distance;
		for (int i=0; i<_size; i++)
			_nodes[i] = o->_nodes[i];
	}
};
#endif // _atomic_path_hpp