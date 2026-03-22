#include "path_and_cycle.hpp"

using namespace std;

Path_and_cycle::Path_and_cycle(const Instance& inst) 
	: inst(inst), graph(Graph())
{
	this->op2edge.resize(inst.n_ops(), Edge());

	this->v_start.resize(this->inst.n_trains());
	this->v_last.resize(this->inst.n_trains());
}


Path_and_cycle::~Path_and_cycle()
{

}


void Path_and_cycle::set_paths(const Instance::Paths& paths_)
{
	this->paths = paths_;

	uint16_t v = 0;

	this->time_lb.resize(v);
	this->vtx2sw.resize(v);


	this->graph.set_vertices(v);

	v = 0;
	for (auto& train : this->inst.trains) {
		size_t t = train.idx;
		auto& path = this->paths[t];
		
		this->v_start[t] = v;
		vtx2sw[v].op_unlock = IDX_MAX;

		for (auto o : path) {
			auto& op = this->inst.ops[o];

			this->time_lb[v] = op.start_lb;

			uint16_t w = v + 1;
			uint32_t e = this->graph.add_edge(v, w, op.dur);

			Edge edge = {e, v, w};
			this->op2edge[o] = edge;

			vtx2sw[v].op_lock = o;
			vtx2sw[w].op_unlock = o;
			
			v++;
		}

		auto& op_last = this->inst.ops[train.op_last()];
		if (train.has_trailing) {
			this->time_lb[t] = op_last.start_lb;
		}
		else {
			this->time_lb[t] = op_last.start_lb + op_last.dur;
		}

		this->v_last[t] = v;
		vtx2sw[v].op_lock = IDX_MAX;
		v++;
	}
}


void Path_and_cycle::solve()
{
	this->make_vtx_order();
}


void Path_and_cycle::make_vtx_order()
{
	auto& graph_path = this->graph.make_path(this->v_last, this->time_lb.data());

	for (idx_t t; t < this->inst.n_trains(); t++) {
		for (idx_t v = this->v_start[t]; v <= this->v_last[t]; v++) {
			this->vtx_order[v] = {v, t, graph_path[v].time};
		}
	}	
}


void Path_and_cycle::merge_sort_vtx_order(idx_t t_left, idx_t t_right)
{
	if (t_left < t_right) {
		idx_t t_mid = t_left + (t_right - t_left)/2;
		this->merge_sort_vtx_order(t_left, t_mid);
		this->merge_sort_vtx_order(t_mid + 1, t_right);
		this->merge_vtx_order(t_left, t_mid, t_right);
	}
}


void Path_and_cycle::merge_vtx_order(idx_t t_left, idx_t t_mid, idx_t t_right)
{
	size_t l = this->v_start[t_left];
	size_t m = this->v_last[t_mid];
	size_t r = this->v_last[t_left];

	size_t n_left = m - l + 1;
	size_t n_right = r - m;

	Vtx_time tmp_left[n_left];
	Vtx_time tmp_right[n_right];

	size_t i, j, k;

	for (i = 0; i < n_left; i++) {
		tmp_left[i] = this->vtx_order[l + i];
	}

	for (j = 0; j < n_right; j++) {
		tmp_left[j] = this->vtx_order[m + j + 1];
	}

	i = 0;
	j = 0;
	k = l;

	while (i < n_left && j < n_right) {
		if (tmp_left[i].time <= tmp_right[j].time) {
			this->vtx_order[k++] = tmp_left[i++];
		}
		else {
			this->vtx_order[k++] = tmp_right[j++];
		}
	}
	
	while (i < n_left) {
		this->vtx_order[k++] = tmp_left[i++];
	}

	while (j < n_right) {
		this->vtx_order[k++] = tmp_right[j++];
	}
}
