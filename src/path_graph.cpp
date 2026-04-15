#include "path_graph.hpp"

#include <iostream>

using namespace std;


Path_graph::Path_graph(const Instance& inst)
	: inst(inst), paths(inst)
{
	size_t n_trains = inst.n_trains();
	size_t n_vtx = inst.max_paths_len + n_trains;

	this->graph.set_vertices(n_vtx);

	this->v_start.resize(n_trains);
	this->v_end.resize(n_trains);
	this->path_res_uses.resize(n_trains, vector<Res_use>(0));

	this->op_vtx.resize(inst.n_ops());

	this->vtx_op_in.resize(n_vtx);
	this->vtx_op_out.resize(n_vtx);

	for (auto& train : inst.trains) {
		this->v_start[train.idx] = train.path_idx + train.idx;
	}
}


void Path_graph::set_all_paths(const Instance::Paths& paths_)
{
	for (size_t t = 0; t < this->inst.n_trains(); t++) {
		this->set_path(t, paths_[t]);
	}
}


void Path_graph::set_path(const idx_t t, const Array<idx_t>& path)
{
	this->paths[t].copy_from(path);

	vertex_t v = this->v_start[t];
	this->vtx_op_in[v] = IDX_MAX;

	auto& res_uses = this->path_res_uses[t];
	res_uses.clear();

	for (idx_t o : path) {
		vertex_t w = v + 1;

		this->op_vtx[o] = v;
		this->vtx_op_out[v] = o;
		this->vtx_op_in[w] = o;

		auto& op = this->inst.ops[o];
		
		this->graph.add_edge(v, w, op.dur);

		for (auto& res : op.res) {
			res_uses.push_back({{v, w}, res.idx, res.time});
		}
	}
}

vector<Path_graph::Alt_edges> Path_graph::group_edges(const idx_t t1, const idx_t t2)
{
	const auto& ru1 = this->path_res_uses[t1];
	auto ru2 = this->path_res_uses[t2];

	sort(ru2.begin(), ru2.end(), Res_use::idx_comparator);

	size_t n_res = this->inst.n_res();
	vector<Interval<int>> ru2_mp(n_res, {-1, -1});

	for (size_t i = 0; i < ru2.size(); i++) {
		size_t r = ru2[i].idx;

		if (ru2_mp[r].start < 0) {
			ru2_mp[r].start = i;
		}
		ru2_mp[r].end = i+1;
	}


	vector<Alt_edges> edges;

	for (auto& x1 : ru1) {
		size_t r = x1.idx;

		// ru2 has not r
		if (ru2_mp[r].start < 0) {
			continue;
		}
			
		for (int i = ru2_mp[r].start; i < ru2_mp[r].end; i++) {
			auto& x2 = ru2[i];

			edges.push_back({x1.vtx, x2.vtx});
		}
	}

	vector<size_t> group_end = {};
	size_t n_edges = edges.size();

	size_t i = 0;
	size_t j = 0;


	while (j < n_edges) {
		j++;
		while (i < j) {
			auto& e1 = edges[i];
		
			for (size_t k = j; k < n_edges; k++) {
				auto& e2 = edges[k];
				/* e1: t1 -> t2
				* e1.first.end -> e1.second.start
				*
				* e2: t1 <- t2
				* e2.first.start <- e2.second.end
				*
				* 
				* e2: t1 -> t2
				* e2.first.end -> e2.second.start
				* 
				* e1: t1 <- t2
				* e1.first.start <- e1.second.end
				*/

				bool creates_cycle = 
					((e2.first.start <= e1.first.end) && (e1.second.start <= e2.second.end)) ||
					((e1.first.start <= e2.first.end) && (e2.second.start <= e1.second.end));

				if (creates_cycle) {
					auto tmp = e2;
					for (size_t l = k; l > j; l--) {
						edges[l] = edges[l - 1];
					}
					edges[j] = tmp;
					j++;
				}
			}
			i++;
		}

		group_end.push_back(j);
	}
	

	size_t group_start = 0;
	for (size_t x : group_end) {
		cout << " " << x - group_start;
		group_start = x;
	}

	cout << endl;

	return edges;
}

