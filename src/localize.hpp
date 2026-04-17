#pragma once

#include "preprocess.hpp"


class Localize
{
public:
	struct Area;

	typedef Instance::idx_t idx_t;
	static const idx_t IDX_MAX = Instance::IDX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Localize(const Preprocess& prepr, const bool group_opt=true);


	std::vector<Area> areas = {};
	Array<Area> areas_choke;
	Array<Area> areas_branch;

	Flag is_area_choke;
	std::vector<idx_t> res_area = {};
	
	METHOD_N(areas);
	METHOD_RANGE(areas, idx_t)

	METHOD_N(areas_choke);
	METHOD_N(areas_branch);

private:
	std::vector<idx_t> area_res = {};

	void make_areas(const bool group_opt);
	void make_area_trains();

};


struct Localize::Area
{
	idx_t idx = IDX_MAX;
	Array<idx_t> res;
	Array<idx_t> trains;
};




