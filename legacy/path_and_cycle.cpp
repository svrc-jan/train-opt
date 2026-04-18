#include "path_and_cycle.hpp"

using namespace std;

Path_and_cycle::Path_and_cycle(const Instance& inst) 
	: inst(inst), graph(Event_graph())
{
	this->op2edge.resize(inst.n_ops(), Edge_idx());

	this->v_start.resize(this->inst.n_trains());
	this->v_last.resize(this->inst.n_trains());

	this->res_locks.resize(this->inst.n_res(), vector<Instance::Res>(0));
}


Path_and_cycle::~Path_and_cycle()
{

}


void Path_and_cycle::set_paths(const Instance::Paths& paths_)
{
	this->paths = paths_;

	vertex_t v = 0;

	for (auto& path : this->paths) {
		v += path.size() + 1;
	}

	this->time_lb.resize(v);
	this->vtx2sw.resize(v);
	this->vtx_order.resize(v);

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

			vertex_t w = v + 1;
			edge_t e = this->graph.add_edge(v, w, op.dur);

			Edge_idx edge = {e, v, w};
			this->op2edge[o] = edge;

			vtx2sw[v].op_lock = o;
			vtx2sw[w].op_unlock = o;
			
			v++;
		}

		auto& op_last = this->inst.ops[train.op_last()];
		if (train.has_trailing) {
			this->time_lb[v] = op_last.start_lb;
		}
		else {
			this->time_lb[v] = op_last.start_lb + op_last.dur;
		}

		this->v_last[t] = v;
		vtx2sw[v].op_lock = IDX_MAX;
		v++;
	}
}


void Path_and_cycle::solve()
{
	this->graph.make_path(this->v_last, this->time_lb);
	this->make_vtx_order();
	this->verify_vtx_order();

	Res_col res_col;
	bool ret = this->find_res_col(res_col);

	this->make_res_col_edges(res_col);
	res_col.swap();
	this->make_res_col_edges(res_col);
}


void Path_and_cycle::make_vtx_order()
{
	auto& graph_path = this->graph.get_path();

	for (idx_t t = 0; t < this->inst.n_trains(); t++) {
		for (idx_t v = this->v_start[t]; v <= this->v_last[t]; v++) {
			this->vtx_order[v] = {v, t, graph_path[v].time};
		}
	}

	this->merge_sort_vtx_order(0, this->inst.n_trains() - 1);
}


void Path_and_cycle::merge_sort_vtx_order(size_t t_left, size_t t_right)
{
	if (t_left < t_right) {
		size_t t_mid = t_left + (t_right - t_left)/2;
		this->merge_sort_vtx_order(t_left, t_mid);
		this->merge_sort_vtx_order(t_mid + 1, t_right);
		this->merge_vtx_order(t_left, t_mid, t_right);
	}
}


void Path_and_cycle::merge_vtx_order(size_t t_left, size_t t_mid, size_t t_right)
{
	size_t l = this->v_start[t_left];
	size_t m = this->v_last[t_mid];
	size_t r = this->v_last[t_right];

	size_t n_left = m - l + 1;
	size_t n_right = r - m;

	Vtx_time tmp_left[n_left];
	Vtx_time tmp_right[n_right];

	size_t i, j, k;

	for (i = 0; i < n_left; i++) {
		tmp_left[i] = this->vtx_order[l + i];
	}

	for (j = 0; j < n_right; j++) {
		tmp_right[j] = this->vtx_order[m + j + 1];
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

void Path_and_cycle::verify_vtx_order()
{
	size_t n_trains = this->inst.n_trains();
	idx_t v_expect[n_trains];

	for (size_t t = 0; t < n_trains; t++) {
		v_expect[t] = this->v_start[t];
	}

	tim_t tim = 0;
	for (auto& x : this->vtx_order) {
		assert(x.vertex == v_expect[x.train]);
		assert(x.time >= tim);

		v_expect[x.train] = x.vertex + 1;
		tim = x.time;
	}
}


bool Path_and_cycle::find_res_col(Res_col& res_col)
{
	size_t n_res = this->inst.n_res();
	
	Vtx_time res_lock[n_res];
	for (size_t r = 0; r < n_res; r++) {
		res_lock[r] = Vtx_time();
	}

	for (auto& x : this->vtx_order) {
		idx_t o_unlock = this->vtx2sw[x.vertex].op_unlock;
		if (o_unlock < IDX_MAX) {
			auto& op_unlock = this->inst.ops[o_unlock];

			for (auto& res : op_unlock.res) {
				auto& rl = res_lock[res.idx];
				assert(rl.vertex + 1 == x.vertex);
				rl.time = x.time + res.time;
			}
		}

		idx_t o_lock = this->vtx2sw[x.vertex].op_lock;
		if (o_lock < IDX_MAX) {
			auto& op_lock = this->inst.ops[o_lock];

			for (auto& res : op_lock.res) {
				auto& rl = res_lock[res.idx];
				if ((rl.vertex == IDX_MAX) || (rl.train == x.train) || (rl.time <= x.time)) {
					rl = {x.vertex, x.train, TIME_MAX};
				}
				else {
					res_col = {rl.vertex, rl.train, x.vertex, x.train};
					return true;
				}
			}
		}
	}

	return false;
}


void Path_and_cycle::make_res_col_edges(const Res_col& res_col)
{
	vertex_t v1 = res_col.v1;
	vertex_t v2 = res_col.v2;

	while (v1 <= this->v_last[res_col.t1] && v2 >= this->v_start[res_col.t2]) {
		bool added = false;
		for (auto& res1 : this->inst.ops[this->vtx2sw[v1].op_unlock].res) {
			for (auto& res2 : this->inst.ops[this->vtx2sw[v2].op_unlock].res) {
				if (res1.idx == res2.idx) {
					added = true;
				}
			}
		}

		if (added) {
			v1++;
			v2--;
		}
		else {
			break;
		}
	}


	v1 = min(v1, this->v_last[res_col.t1]);
	v2 = max(v2, this->v_start[res_col.t2]);

	this->fill_res_locks(this->v_start[res_col.t1], v1);

	this->res_col_edges.clear();

	for (idx_t v = res_col.v2; v < this->v_last[res_col.t2]; v++) {
		idx_t o = this->vtx2sw[v].op_lock;
		auto& op = this->inst.ops[o];
		for (auto& res : op.res) {
			for (auto& x : this->res_locks[res.idx]) {
				idx_t u = x.idx + 1;
				Edge_dur edge = {u, v, x.time};
				this->res_col_edges.push_back(edge);
			}
		}
	}
}


void Path_and_cycle::fill_res_locks(size_t v_start, size_t v_end)
{
	for (auto& ro : this->res_locks) {
		ro.clear();
	}

	for (idx_t v = v_start; v <= v_end; v++) {
		idx_t o = this->vtx2sw[v].op_lock;
		auto& op = this->inst.ops[o];
		for (auto& res : op.res) {
			this->res_locks[res.idx].push_back({v, res.time});
		}
	}
}
