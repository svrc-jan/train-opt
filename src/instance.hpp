#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <ranges>

#include <nlohmann/json.hpp>

#include "utils/macros.hpp"
#include "utils/array.hpp"

using json = nlohmann::json;


class Instance
{
public:
	struct Res;
	struct Op;
	struct Train;
	struct Obj;
	class Paths;

	typedef uint16_t idx_t;
	typedef uint32_t tim_t;
	typedef uint16_t dur_t;

	static const idx_t IDX_MAX = std::numeric_limits<idx_t>::max();
	static const dur_t DUR_MAX = std::numeric_limits<dur_t>::max();
	static const tim_t TIM_MAX = std::numeric_limits<tim_t>::max();

	Array<Train> trains;
	Array<Op> ops;
	Array<Obj> objs;

	idx_t max_paths_len = 0;

	Instance(const std::string& file_name);
	~Instance();

	METHOD_N(trains)
	METHOD_N(ops)
	METHOD_N(op_succ)
	METHOD_N(op_pred)
	METHOD_N(op_res)
	inline size_t n_res() const { return this->res_name_to_idx.size(); }

	inline auto ops_range() const { return std::views::iota(0U, this->n_ops()); }
	inline auto trains_range() const { return std::views::iota(0U, this->n_trains()); }

	inline Paths get_empty_paths() const;
	Paths get_random_paths() const;

private:
	void* data_ptr = nullptr;

	Array<Res> op_res;
	Array<uint16_t> op_succ;
	Array<uint16_t> op_pred;

	std::map<std::string, uint16_t> res_name_to_idx = {};

	void prepare(json inst_jsn);
	void parse(json inst_jsn);
	void assign_arrays();
	void assign_pred_ops();
	
	void propagate_lower_bounds();
	void propagate_upper_bounds();
	void set_max_bound();
	void set_leading_trailing();

	void add_res_name(std::string res_name);
};


struct Instance::Res
{
	idx_t idx = IDX_MAX;
	dur_t time = 0;

	bool operator<(const Res& other) const { return this->idx < other.idx; }
	bool operator==(const Res& other) const { return this->idx == other.idx; }

	bool operator<(int other) const { return this->idx < other; }
	bool operator==(int other) const { return this->idx == other; }
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
	Array<Res> res;

	inline bool is_leading() const;
	inline bool is_trailing() const;

	METHOD_N(succ)
	METHOD_N(pred)
	METHOD_N(res)
};


struct Instance::Train
{
	idx_t idx = IDX_MAX;
	idx_t op_first = IDX_MAX;
	idx_t path_idx = IDX_MAX;
	uint8_t has_leading = 0;
	uint8_t has_trailing = 0;
	Array<Op> ops;

	METHOD_N(ops)
	METHOD_AFTER(op)
	METHOD_LAST(op)
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

inline bool Instance::Op::is_leading() const
{
	return this->pred.empty() && this->res.empty();
}


inline bool Instance::Op::is_trailing() const
{
	return this->succ.empty() && this->res.empty();
}



