#include "solver.hpp"

#include <queue>

using namespace std;


Solver::Solver(const Localize& local)
	: inst(local.inst), prepr(local.prepr), local(local)
{
	this->graph.set_n_vtx(this->prepr.n_levels());
	this->init_res_use();
}


void Solver::get_init_sol()
{
	cout << "path init" << endl;

	this->init_path();
	this->add_path_to_graph();

	assert(!this->graph.make_order(this->first_levels));
	this->graph.update_time();

	cout << "obj: " << this->get_obj() << endl;
}


void Solver::init_res_use()
{
	size_t n_res = this->inst.n_res;
	size_t n_trains = this->inst.n_trains();

	this->res_use.resize(n_res);
	this->res_use_data.resize(n_res*n_trains);

	for (auto r : this->inst.res_range()) {
		this->res_use[r] = &this->res_use_data[r*n_trains];
	}
}


void Solver::init_path()
{
	size_t n_ops = this->inst.n_ops();
	size_t n_levels = this->prepr.n_levels();

	this->path.resize(n_levels, IDX_MAX);
	this->first_levels.resize(this->inst.n_trains());

	tim_t time[n_ops];
	idx_t pred[n_ops];
	
	idx_t in_deg[n_ops];
	for (auto& op : this->inst.ops) {
		time[op.idx] = TIM_MAX;
		pred[op.idx] = IDX_MAX;

		in_deg[op.idx] = op.n_pred();
	}
	
	queue<idx_t> q;
	for (auto& train_p : this->prepr.trains) {
		this->first_levels[train_p.idx] = train_p.level_first;

		auto& train_i = this->inst.trains[train_p.idx];
		idx_t o = train_i.op_first;

		time[o] = 0;
		q.push(o);

		while (!q.empty()) {
			o = q.front(); q.pop();
			auto& op = this->inst.ops[o];

			for (auto s : op.succ) {
				tim_t new_time = time[o] + (tim_t)op.dur;
				if (time[s] > new_time) {
					time[s] = new_time;
					pred[s] = o;
				}

				in_deg[s] -= 1;
				if (in_deg[s] == 0) {
					q.push(s);
				}
			}
  		}

		o = train_i.op_last();
		while (o < IDX_MAX) {
			this->path[this->prepr.op_level[o].start] = o;
			o = pred[o];
		}
	}
}


void Solver::update_res_use()
{
	fill(this->res_use_data.begin(), this->res_use_data.end(), Res_use());

	for (auto& train : this->prepr.trains) {
		for (auto l : train.levels_range()) {
			idx_t o = this->path[l];
			if (o == IDX_MAX) {
				continue;
			}
			auto& l_op = this->prepr.op_level[o];
			
			for (auto& res : this->inst.ops[o].res) {
				auto& ru = this->res_use[res.idx][train.idx];

				ru.level.start = (ru.level.start == IDX_MAX) ? l_op.start : ru.level.start;
				ru.level.end = l_op.end;
				ru.time = res.time;
			}
		}
	}
}

void Solver::add_path_to_graph()
{
	for (auto o : this->path) {
		if (o == IDX_MAX) {
			continue;
		}

		auto& l_op = this->prepr.op_level[o];
		auto& op = this->inst.ops[o];

		graph.time_lb[l_op.start] = op.start_lb;
		graph.add_edge({l_op.start, l_op.end, op.dur});
	}
}


Solver::tim_t Solver::get_obj()
{
	tim_t val = 0;
	for (auto o : this->path) {
		idx_t obj_idx = (o == IDX_MAX) ? IDX_MAX : this->inst.ops[o].obj;
		if (obj_idx == IDX_MAX) {
			continue;
		}

		auto& obj = this->inst.objs[obj_idx];
		tim_t diff = this->graph.time[this->prepr.op_level[o].start];
		val += (diff > 0) ? (diff*obj.coeff + obj.increment) : 0;
	}

	return val;
}

