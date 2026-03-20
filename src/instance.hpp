#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

#include "utils/array.hpp"

using json = nlohmann::json;

#define IDX_MAX UINT16_MAX
#define TIME_MAX UINT32_MAX
#define DUR_MAX UINT16_MAX

typedef uint16_t idx_t;
typedef uint32_t tim_t;
typedef uint16_t dur_t;


class Instance
{
public:
	struct Res;
	struct Op;
	struct Train;
	struct Obj;
	class Paths;

	Array<Train> trains = {nullptr, 0};
	Array<Op> ops = {nullptr, 0};
	Array<Obj>  objs = {nullptr, 0};

	Instance(const std::string& file_name);
	~Instance();

	inline size_t n_trains() const { return this->trains.size; }
	inline size_t n_ops() const { return this->ops.size; }
	inline size_t n_res() const { return this->res_name_to_idx.size(); }
	inline size_t n_op_succ() const { return this->op_succ.size; }
	inline size_t n_op_pred() const { return this->op_pred.size; }
	inline size_t n_op_res() const { return this->op_res.size; }

	inline Paths get_empty_paths() const;
	Paths get_random_paths() const;

private:
	void* data_ptr = nullptr;

	Array<Res> op_res = {nullptr, 0};
	Array<uint16_t> op_succ = {nullptr, 0};
	Array<uint16_t> op_pred = {nullptr, 0};

	idx_t max_paths_len = 0;

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
	tim_t start_ub = TIME_MAX;

	Array<idx_t> succ = {nullptr, 0};
	Array<idx_t> pred = {nullptr, 0};
	Array<Res> res = {nullptr, 0};

	inline bool is_leading() const;
	inline bool is_trailing() const;
};


struct Instance::Train
{
	idx_t idx = IDX_MAX;
	idx_t op_start = IDX_MAX;
	uint8_t has_leading = 0;
	uint8_t has_trailing = 0;
	idx_t path_idx = IDX_MAX;
	Array<Op> ops = {nullptr, 0};

	inline idx_t op_last() const { return op_start + this->ops.size - 1; }
	inline idx_t op_end() const { return op_start + this->ops.size; }
};


const size_t op_struct_bytes = sizeof(Instance::Op);
const size_t train_struct_bytes = sizeof(Instance::Train);


class Instance::Paths
{
private:
	void* data_ptr = nullptr;
	size_t data_size = 0;

	void copy(const Paths& obj);
	void move(Paths& obj);

public:
	Array<Array<idx_t>> ops = {nullptr, 0};

	Paths() : data_ptr(nullptr), data_size(0) {}
	Paths(const Instance& inst);
	Paths(const Paths& obj) { this->copy(obj); }
	Paths(Paths&& obj) { this->move(obj); }
	~Paths();

	Paths& operator=(const Paths& obj);
	Paths& operator=(Paths&& obj);

	Array<idx_t>* begin() { return this->ops.begin(); }
	Array<idx_t>* end() { return this->ops.end(); }
	const Array<idx_t>* begin() const { return this->ops.begin(); }
	const Array<idx_t>* end() const { return this->ops.end(); }

	void clear();
};


inline Instance::Paths Instance::get_empty_paths() const
{
	return Paths(*this);
}

inline bool Instance::Op::is_leading() const
{
	return (this->pred.size == 0) && (this->res.size == 0);
}


inline bool Instance::Op::is_trailing() const
{
	return (this->succ.size == 0) && (this->res.size == 0);
}



