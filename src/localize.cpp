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
	Disjoint_set disj_set(this->inst.n_res());

	
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

	cout << "num res: " << this->inst.n_res() << ", num sect: " << disj_set.n_sets << endl;

	this->areas.resize(disj_set.n_sets);
	for (auto a : this->areas_range()) {
		this->areas[a].idx = a;
	}

	auto dist_set_res = disj_set.get_result();

}

