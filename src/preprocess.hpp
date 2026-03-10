#pragma once

#include "utils/array.hpp"
#include "instance.hpp"

class Preprocess
{
public:
	struct Op;
	struct Junction;
	struct Level;
	struct Train;
	
	const Instance& inst;

	std::vector<Op> ops = {};
	std::vector<Junction> juncts = {};
	std::vector<Train> trains = {};
	std::vector<Level> levels = {};
	
	Preprocess(const Instance& inst);
	
	inline int n_juncts() const { return this->juncts.size(); }
	inline int n_levels() const { return this->levels.size(); }
	
private:
	std::vector<int> junct_ops_in = {};
	std::vector<int> junct_ops_out = {};

	void make_junctions();
	void make_levels();
};


struct Preprocess::Op
{
	int junct_start = -1;
	int junct_end   = -1;
	int level_start = -1;
	int level_end   = -1;
};

struct Preprocess::Junction
{
	int time_lb = 0;
	int time_ub = INT_MAX;

	Array<int> ops_in  = {nullptr, 0};
	Array<int> ops_out = {nullptr, 0};
};


struct Preprocess::Level
{
	Array<int> juncts = {nullptr, 0};
};


struct Preprocess::Train
{
	int junct_start = -1;
	int level_start = -1;

	Array<Junction> juncts = {nullptr, 0}; 
	Array<Level> levels = {nullptr, 0};
	
	inline int junct_last() const { return this->junct_start + this->juncts.size - 1; }
	inline int junct_end() const { return this->junct_start + this->juncts.size; }

	inline int level_last() const { return this->level_start + this->levels.size - 1; }
	inline int level_end() const { return this->level_start + this->levels.size; }
};
