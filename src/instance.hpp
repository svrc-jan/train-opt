#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <ranges>


#include "utils/json_aux.hpp"
#include "utils/macros.hpp"
#include "utils/range.hpp"
#include "utils/array.hpp"

class Instance
{
public:
	struct Idx_dur;
	struct Op;
	struct Train;
	struct Obj;
	class Paths;

	typedef uint16_t idx_t;
	typedef uint16_t dur_t;
	typedef uint32_t tim_t;
	

	static const idx_t IDX_MAX = std::numeric_limits<idx_t>::max();
	static const dur_t DUR_MAX = std::numeric_limits<dur_t>::max();
	static const tim_t TIM_MAX = std::numeric_limits<tim_t>::max();

	Array<Train> trains;
	Array<Op> ops;
	Array<Obj> objs;

	idx_t n_res = 0;
	idx_t max_paths_len = 0;

	Instance(const std::string& file_name, const bool verify=false);
	~Instance();

	METHOD_N(trains)
	METHOD_N(ops)
	METHOD_N(op_succ)
	METHOD_N(op_pred)
	METHOD_N(op_res)

	METHOD_RANGE(trains, idx_t)
	METHOD_RANGE(ops, idx_t)
	METHOD_RANGE(objs, idx_t)

	inline auto res_range() const { return Range<idx_t>(0, this->n_res); }
	
	inline Paths get_empty_paths() const;
	Paths get_random_paths() const;

	bool is_op_lock(idx_t o, idx_t r) const;
	bool is_op_unlock(idx_t o, idx_t r) const;
	

private:
	void* data_ptr = nullptr;

	Array<Idx_dur> op_res;
	Array<uint16_t> op_succ;
	Array<uint16_t> op_pred;

	std::map<std::string, idx_t> res_name_to_idx = {};

	void prepare(const json& inst_jsn);
	void parse(const json& inst_jsn);
	void assign_arrays();
	void assign_pred_ops();
	
	void propagate_lower_bounds();
	void propagate_upper_bounds();
	void set_max_bound();

	void verify_json(const json& inst_jsn) const;
	void verify_pred();

	idx_t add_res_name(const std::string& res_name);
	idx_t get_res_idx(const std::string& res_name) const;
};


struct Instance::Idx_dur
{
	idx_t idx = IDX_MAX;
	dur_t dur = 0;

	operator idx_t() const { return idx; }

	bool operator<(const Idx_dur& other) const { return this->idx < other.idx; }
	bool operator==(const Idx_dur& other) const { return this->idx == other.idx; }

	bool operator<(idx_t other) const { return this->idx < other; }
	bool operator==(idx_t other) const { return this->idx == other; }
};


struct Instance::Obj
{

	idx_t op = IDX_MAX;
	uint8_t coeff = 0;
	uint8_t increment = 0;
	tim_t threshold = 0;
};

const int obj_size = sizeof(Instance::Obj);


struct Instance::Op
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t obj = IDX_MAX;

	dur_t dur = 0;

	tim_t start_lb = 0;
	tim_t start_ub = TIM_MAX;

	Array<idx_t> succ;
	Array<idx_t> pred;
	Array<Idx_dur> res;

	OPERATOR_IDX(idx_t)

	METHOD_N(succ)
	METHOD_N(pred)
	METHOD_N(res)
};


struct Instance::Train
{
	idx_t idx = IDX_MAX;
	idx_t op_first = IDX_MAX;
	idx_t path_idx = IDX_MAX;
	Array<Op> ops;

	METHOD_N(ops)
	METHOD_AFTER(op)
	METHOD_LAST(op)

	auto ops_range() const { return Range<idx_t>(op_first, op_after()); }
};


const size_t op_struct_bytes = sizeof(Instance::Op);
const size_t train_struct_bytes = sizeof(Instance::Train);


class Instance::Paths
{
private:
	std::vector<idx_t> data = {};

public:
	std::vector<Array<idx_t>> ops = {};
	
	Paths() : data({}), ops({}) {}
	Paths(const Instance& inst);
	~Paths();

	auto begin() { return this->ops.begin(); }
	auto end() { return this->ops.end(); }
	const auto begin() const { return this->ops.begin(); }
	const auto end() const { return this->ops.end(); }

	auto& operator[](size_t idx) { return this->ops[idx]; }
	const auto& operator[](size_t idx) const { return this->ops[idx]; }

	void clear();
};


inline Instance::Paths Instance::get_empty_paths() const
{
	return Paths(*this);
}



