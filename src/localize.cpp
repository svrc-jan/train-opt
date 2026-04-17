#include "localize.hpp"

#include <set>

#include "utils/disjoint_set.hpp"

using namespace std;

Localize::Localize(const Preprocess& prepr, const bool group_opt) 
	: inst(prepr.inst), prepr(prepr)
{
	this->make_areas(group_opt);
	this->make_levels();
	this->make_trains();
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

	vector<idx_t> req_res;
	vector<idx_t> opt_res;

	idx_t prev_req = IDX_MAX;
	idx_t prev_opt = IDX_MAX;

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

		if (level.n_pred() == 0) {
			prev_req = IDX_MAX;
			prev_opt = IDX_MAX;
		}

		idx_t a, b;
		
		if (req_res.size() > 0) {
			if (prev_req < IDX_MAX) {
				a = res_pos[prev_req];
				b = res_pos[req_res[0]];
				disj_set.union_set(a, b);
			}
			
			for (size_t i = 1; i < req_res.size(); i++) {
				a = res_pos[req_res[i-1]];
				b = res_pos[req_res[i]];
				disj_set.union_set(a, b);
			}

			prev_req = req_res.back();
		}
		else {
			prev_req = IDX_MAX;
		}

		if (opt_res.size() > 0) {
			if (group_opt && prev_opt < IDX_MAX) {
				a = res_pos[prev_opt];
				b = res_pos[opt_res[0]];
				disj_set.union_set(a, b);
			}

			for (size_t i = 1; i < opt_res.size(); i++) {
				a = res_pos[opt_res[i-1]];
				b = res_pos[opt_res[i]];
				disj_set.union_set(a, b);
			}
			
			prev_opt = opt_res.back();
		}
		else {
			prev_opt = IDX_MAX;
		}
	}

	this->areas.resize(disj_set.n_sets);
	for (auto a : this->areas_range()) {
		auto& area = this->areas[a];
		area.idx = a;
		area.res.set_size(0);
		area.has_train.set_n_items(this->inst.n_trains());
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


void Localize::make_levels()
{
	this->levels.resize(this->prepr.n_levels());
	for (auto& level_p : this->prepr.levels) {
		auto& level = this->levels[level_p.idx];

		for (auto r : level_p.res) {
			idx_t a = this->res_area[r];
			if (this->prepr.is_res_req[r]) {
				assert(level.area.choke == a || level.area.choke == IDX_MAX);
				level.area.choke = a;
			}
			else {
				assert(level.area.branch == a || level.area.branch == IDX_MAX);
				level.area.branch = a;
			}
		}
	}
}


void Localize::make_trains()
{
	this->trains.resize(this->inst.n_trains());

	Flag area_flag(this->n_areas());
	
	size_t idx = 0;

	for (auto& train_p : this->prepr.trains){
		auto& train = this->trains[train_p.idx];
		train.idx = train_p.idx;
		
		train.levels.set_begin(&this->levels[train_p.level_first]);
		train.levels.set_size(train_p.n_levels());

		area_flag.clear();

		for (auto& level : train.levels) {
			auto add_if_valid = [&](idx_t x) {
				if (x < IDX_MAX) {
					area_flag += x;
				} 
			};

			add_if_valid(level.area.choke);
			add_if_valid(level.area.branch);
		}
		
		train.areas.set_size(area_flag.get_true_count());
		for (auto a : area_flag.get_true_list()) {
			this->areas[a].has_train += train.idx;
		}

		idx += train.areas.size();
	}

	this->train_areas.resize(idx);
	idx = 0;
	
	bool choke_reentry = false;
	bool branch_reentry = false;


	for (auto& train : this->trains) {
		train.areas.assign_offset(this->train_areas, idx, true);

		area_flag.clear();

		Choke_branch pred = {IDX_MAX, IDX_MAX};
		for (auto& level : train.levels) {
			idx_t c = level.area.choke;
			idx_t b = level.area.branch;

			if (c < IDX_MAX && area_flag[c]) {
				if (pred.choke != c) {
					choke_reentry = true;
					// cout << "train: " << train.idx << " - choke reentry: " << c << endl;
				}
			}

			if (b < IDX_MAX && area_flag[b]) {
				if (pred.branch != b) {
					branch_reentry = true;
					// cout << "train: " << train.idx << " - branch reentry: " << b << endl;
				}
			}

			auto add_if_missing = [&](idx_t x) {
				if (x < IDX_MAX && !area_flag[x]) {
					area_flag += x;
					train.areas.push_back(x);
				}
			};

			add_if_missing(c);
			add_if_missing(b);

			if (c < IDX_MAX) {
				pred.choke = c;
			}
			if (b < IDX_MAX) {
				pred.branch = b;
			}
		}
	}

	if (choke_reentry) {
		cout << "choke reentry" << (branch_reentry ? " and " : "\n"); 
	}
	if (branch_reentry) {
		cout << "branch reentry" << endl;
	}
}

