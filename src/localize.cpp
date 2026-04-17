#include "localize.hpp"

#include <set>

#include "utils/disjoint_set.hpp"

using namespace std;

Localize::Localize(const Preprocess& prepr) 
	: inst(prepr.inst), prepr(prepr)
{
	this->make_areas();
}


void Localize::make_areas()
{
	size_t n_res = this->inst.n_res();
	Disjoint_set disj_set(n_res);

	vector<idx_t> prev_req_res = {};
	vector<idx_t> req_res;
	vector<idx_t> opt_res;

	for (auto l : this->prepr.levels_range()) {
		req_res.clear();
		opt_res.clear();

		for (auto r : this->inst.res_range()) {
			auto grs = this->prepr.global_res_state[r];

			if (this->prepr.res_count[l][r] == 0) {
				// nothing
			}
			else if (grs == RES_REQ) {
				req_res.push_back(r);
			}
			else {
				opt_res.push_back(r);
			}
			

			for (size_t i = 1; i < req_res.size(); i++) {
				disj_set.union_set(req_res[0], req_res[i]);
			}

			for (size_t i = 1; i < opt_res.size(); i++) {
				disj_set.union_set(opt_res[0], opt_res[i]);
			}

			for (auto r1 : prev_req_res) {
				for (auto r2 : req_res) {
					disj_set.union_set(r1, r2);
				}
			}
		}
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

	this->area_res.resize(n_res);
	size_t area_res_idx = 0;

	for (auto& area : this->areas) {
		area.res.assign_offset(this->area_res, area_res_idx, true);
	}

	this->res_area.resize(n_res);
	for (auto r : this->inst.res_range()) {
		idx_t a = set_idx[r];
		this->res_area[r] = a;

		auto& area = this->areas[a];
		this->areas[a].res.push_back(r);

		uint8_t area_typ = (this->prepr.global_res_state[r] == RES_REQ) ? AREA_CHOKE : AREA_BRANCH; 
		
		assert(area.typ == area_typ || area.typ == AREA_DEFAULT);
		area.typ = area_typ;
	}

}

