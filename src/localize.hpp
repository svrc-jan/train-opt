#pragma once

#include "preprocess.hpp"


enum Area_type
{
	AREA_BRANCH,
	AREA_CHOKE,
	AREA_DEFAULT = 0xff
};

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
	std::vector<idx_t> res_area = {};
	

	METHOD_N(areas);
	METHOD_RANGE(areas, idx_t)

private:
	std::vector<idx_t> area_res = {};

	void make_areas();

};


struct Localize::Area
{
	idx_t idx = IDX_MAX;
	uint8_t typ = AREA_DEFAULT;
	Array<idx_t> res;
	Array<idx_t> trains;
};
