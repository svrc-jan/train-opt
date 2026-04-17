#include "localize.hpp"

#include <set>

#include "utils/disjoint_set.hpp"

using namespace std;

Localize::Localize(const Preprocess& prepr, const bool group_opt) 
	: inst(prepr.inst), prepr(prepr)
{
	this->make_areas(group_opt);
	this->make_area_trains();
}


void Localize::make_areas(const bool group_opt)
{
	idx_t res_pos[this->inst.n_res];
	idx_t pos = 0;
	
	for (auto r : this->prepr.is_res_req.get_true_list()) {
		res_pos[r] = pos++; 
	}

	for (auto r : this->prepr.is_res_split.get_true_list()) {
		res_pos[r] = pos++; 
	}

	for (auto r : this->prepr.is_res_opt.get_true_list()) {
		res_pos[r] = pos++; 
	}

	assert(pos == this->inst.n_res);

	Disjoint_set disj_set(this->inst.n_res);

	vector<idx_t> prev_opt_res = {};
	vector<idx_t> prev_req_res = {};
	vector<idx_t> req_res;
	vector<idx_t> opt_res;


	for (auto& level : this->prepr.levels) {
		req_res.clear();
		opt_res.clear();

		for (auto r : level.res) {
			if (this->prepr.is_res_req[r]) {
				req_res.push_back(r);
			}
			else {
				opt_res.push_back(r);
			}
		}

		idx_t a, b;
		
		if (req_res.size() > 0) {
			a = res_pos[req_res[0]];
			for (size_t i = 1; i < req_res.size(); i++) {
				b = res_pos[req_res[i]];
				disj_set.union_set(a, b);
			}

			for (auto r : prev_req_res) {
				b = res_pos[r];
				disj_set.union_set(a, b);
			}
		}

		if (opt_res.size() > 0) {
			a = res_pos[opt_res[0]];
			for (size_t i = 1; i < opt_res.size(); i++) {
				b = res_pos[opt_res[i]];
				disj_set.union_set(a, b);
			}
			if (group_opt) {
				for (auto r : prev_opt_res) {
					b = res_pos[r];
					disj_set.union_set(a, b);
				}				
			}
		}

		prev_req_res = req_res;
		prev_opt_res = opt_res;
	}


	this->areas.resize(disj_set.n_sets);
	for (auto a : this->areas_range()) {
		auto& area = this->areas[a];
		area.idx = a;
		area.res.set_size(0);
	}

	auto set_idx = disj_set.get_result();
	
	for (auto r : this->inst.res_range()) {
		this->areas[set_idx[r]].res.increment_size(1);
	}

	this->area_res.resize(this->inst.n_res);
	size_t area_res_idx = 0;

	for (auto& area : this->areas) {
		area.res.assign_offset(this->area_res, area_res_idx, true);
	}

	this->is_area_choke.set_n_items(this->n_areas());
	this->res_area.resize(this->inst.n_res);

	for (auto r : this->inst.res_range()) {
		idx_t a = set_idx[res_pos[r]];
		this->res_area[r] = a;

		auto& area = this->areas[a];
		area.res.push_back(r);

		if (this->prepr.is_res_req[r]) {
			this->is_area_choke += a;
		}
	}

	size_t n_choke = this->is_area_choke.get_true_count();
	this->areas_choke.set_begin(&this->areas[0], false);
	this->areas_choke.set_size(n_choke);

	this->areas_branch.set_begin(&this->areas[n_choke], false);
	this->areas_branch.set_size(this->n_areas() - n_choke);

	for (auto& area : this->areas_choke) {
		assert(this->is_area_choke[area.idx]);

		for (auto r : area.res) {
			assert(this->prepr.is_res_req[r]);
		}
	}

	for (auto& area : this->areas_branch) {
		assert(!this->is_area_choke[area.idx]);

		for (auto r : area.res) {
			assert(!this->prepr.is_res_req[r]);
		}
	}
}


void Localize::make_area_trains()
{

}
