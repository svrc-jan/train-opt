#pragma once
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

#include "utils/array.hpp"

using json = nlohmann::json;


class Instance
{
public:
	struct Res;
	struct Op;
	struct Train;
	struct Obj;

	int max_n_train_ops = 0;

	std::vector<Train> trains = {};
	std::vector<Op> ops = {};
	std::vector<Obj> objs = {};

	Instance(const std::string& file_name);

	inline int n_trains() const { return this->trains.size(); }
	inline int n_ops() const { return this->ops.size(); }
	inline int n_res() const { return this->res_name_to_idx.size(); }
	inline int n_op_succ() const { return this->op_succ.size(); }
	inline int n_op_pred() const { return this->op_pred.size(); }
	inline int n_op_res() const { return this->op_res.size(); }


private:
	std::vector<int> op_succ = {};
	std::vector<int> op_pred = {};
	std::vector<Res> op_res = {};
	std::vector<int> train_topo_order = {};
	std::map<std::string, int> res_name_to_idx = {};

	void prepare(json inst_jsn);
	void parse(json inst_jsn);
	void assign_arrays();
	void assign_pred_ops();
	
	void propagate_lower_bounds();
	void propagate_upper_bounds();
	void set_max_bound();

	void add_res_name(std::string res_name);
};


struct Instance::Res
{
	int idx = -1;
	int time = 0;

	bool operator<(const Res& other) const { return this->idx < other.idx; }
	bool operator==(const Res& other) const { return this->idx == other.idx; }

	bool operator<(int other) const { return this->idx < other; }
	bool operator==(int other) const { return this->idx == other; }
};

struct Instance::Obj
{
	int threshold = 0;
	int coeff = 0;
	int increment = 0;
};


struct Instance::Op
{
	int train = -1;
	
	int dur = 0;
	int start_lb = 0;
	int start_ub = INT_MAX;

	Obj* obj = nullptr;

	Array<int> succ = {nullptr, 0};
	Array<int> pred = {nullptr, 0};
	Array<Res> res = {nullptr, 0};
};


struct Instance::Train
{
	int op_start = -1;
	Array<Op> ops = {nullptr, 0};

	inline int op_last() const { return this->op_start + this->ops.size - 1; }
	inline int op_end() const { return this->op_start + this->ops.size; }
};