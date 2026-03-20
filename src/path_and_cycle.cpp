#include "path_and_cycle.hpp"


Path_and_cycle::Path_and_cycle(const Instance& inst) 
	: inst(inst), graph(Graph())
{
	this->o2g.resize(inst.n_ops(), Item_o2g());
	this->res_uses.set_num_lists(inst.n_res());
}


Path_and_cycle::~Path_and_cycle()
{

}


void Path_and_cycle::set_paths(const Instance::Paths& paths_)
{
	this->paths = paths_;

	uint16_t v = 0;
	for (auto& path : this->paths) {
		v += path.size + 1;
	}

	this->graph.set_vertices(v);
	this->v_end.reserve(this->inst.n_trains());
	this->v_end.clear();
	

	v = 0;
	for (auto& path : this->paths) {
		for (auto o : path) {
			auto& op = this->inst.ops[o];

			for (auto& res : op.res) {
				this->res_uses.add(res.idx, {o, res.time});
			}

			uint16_t w = v + 1;
			uint32_t e = this->graph.add_edge(v, w, op.dur);

			this->o2g[o] = {v, w, e};

			v++;
		}

		this->v_end.push_back(v);
		v++;
	}
}


void Path_and_cycle::solve()
{

	this->graph.make_path(this->v_end);
}
