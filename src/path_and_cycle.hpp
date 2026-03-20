#pragma once

#include "utils/block_list.hpp"
#include "instance.hpp"
#include "graph.hpp"

class Path_and_cycle
{
public:
	const Instance& inst;
	
	Path_and_cycle(const Instance& isnt);
	~Path_and_cycle();

	void set_paths(const Instance::Paths& paths);
	void solve();

private:
	struct Item_o2g;

	Graph graph;
	Instance::Paths paths;

	std::vector<Item_o2g> o2g = {};
	std::vector<uint16_t> v_end = {};

	Block_list<std::pair<idx_t, dur_t>> res_uses;
};


struct Path_and_cycle::Item_o2g
{
	uint16_t start = UINT16_MAX;
	uint16_t end = UINT16_MAX;
	uint32_t edge = UINT32_MAX;
};
