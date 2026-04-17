#pragma once

#include "instance.hpp"
#include "preprocess.hpp"

class Localize
{
public:
	struct Area;
	struct Train;
	struct Level;
	struct Choke_branch;

	typedef Instance::idx_t idx_t;

	static const idx_t IDX_MAX = Instance::IDX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Localize(const Preprocess& prepr, const bool group_opt=true);


	std::vector<Area> areas = {};
	Array<Area> areas_choke;
	Array<Area> areas_branch;

	std::vector<Level> levels = {};
	std::vector<Train> trains = {};

	Flag is_area_choke;
	std::vector<idx_t> res_area = {};
	
	METHOD_N(areas)
	METHOD_N(trains)
	METHOD_RANGE(areas, idx_t)
	METHOD_RANGE(trains, idx_t)

	METHOD_N(areas_choke);
	METHOD_N(areas_branch);

private:
	std::vector<idx_t> area_res = {};
	std::vector<idx_t> train_areas = {};

	void make_areas(const bool group_opt);
	void make_levels();
	void make_trains();

};


struct Localize::Area
{
	idx_t idx = IDX_MAX;
	Array<idx_t> res;
	Flag has_train;
};


struct Localize::Choke_branch
{
	idx_t choke = IDX_MAX;
	idx_t branch = IDX_MAX;
};


struct Localize::Level
{
	Choke_branch area;
};


struct Localize::Train
{
	idx_t idx = IDX_MAX;
	Array<Level> levels;
	Array<idx_t> areas;
};


inline std::ostream& operator<<(std::ostream& os, const Localize::Level& level)
{
	bool empty = true;
	if (level.area.choke < Localize::IDX_MAX) {
		os << "(" << level.area.choke; 
		empty = false;
	}

	if (level.area.branch < Localize::IDX_MAX) {
		os << (empty ? "(B" : ", B") << level.area.branch; 
		empty = false;
	}

	os << (empty ? "( )" : ")");

	return os;
}
