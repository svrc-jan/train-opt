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

	Localize(const Preprocess& prepr);

	std::vector<Area> areas = {};

	METHOD_N(areas);
	METHOD_RANGE(areas, idx_t)

private:
	std::vector<idx_t> area_res = {};

	void make_areas();

};


struct Localize::Area
{
	idx_t idx = IDX_MAX;
	Array<idx_t> res;
};

