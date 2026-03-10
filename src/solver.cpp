#include "solver.hpp"


using namespace std;

Solver::Solver(Schedule& sched)
	:  inst(sched.inst), prepr(sched.prepr), graph(sched.graph), sched(sched)
{
	level_train.resize(this->prepr.n_levels());
	for (int t = 0; t < this->prepr.n_trains(); t++) {
		auto& train = this->prepr.trains[t];

		for (int l = train.level_start; l < train.level_end(); l++) {
			this->level_train[l] = t;
		}
	}
}


bool Solver::solve_with_train_prio(const vector<double>& prio)
{
	int iter = 0;

	while (true) {
		iter +=  1;
		Schedule::Res_edges res_edges;
		
		if (this->sched.process_from_start(res_edges)) {
			break;
		}

		int t1 = this->level_train[res_edges.first.vertex_from];
		int t2 = this->level_train[res_edges.second.vertex_from];

		Graph::Edge edge;
		if (prio[t1] > prio[t2]) {
			edge = res_edges.first;
		}
		else {
			edge = res_edges.second;
		}

#if defined STATIC_LOG_LEVEL && STATIC_LOG_LEVEL >= 100
		println("iter {} - edge: {} -> {}",
			iter,
			edge.vertex_from,
			edge.vertex_to
		);
#endif
		if (!this->graph.add_edge(edge)) {
			println("ERROR: cycle detected");
			return false;
		}
	}

	println("solved in {} iter", iter);


	return true;
}


