#include "link_graph.hpp"




using namespace std;


Link_graph::Link_graph(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{	
	cout << "Link_graph" << endl;
	// this->make_chunks();

	// this->make_chunk_links();
	// cout << "  chunk links: " << this->chunk_link.size() << endl;

	this->make_link_map();
}


void Link_graph::make_chunks()
{
	this->chunks.resize(this->prepr.n_chunks());
	this->visited.set_n_items(this->prepr.n_sects());

	for (auto c : this->prepr.chunks_range()) {
		auto& chunk = this->chunks[c];
		chunk.idx = c;
		chunk.prepr = &this->prepr.chunks[c];
		chunk.train = chunk.prepr->train;
		chunk.res = chunk.prepr->res;
	}
}


void Link_graph::make_chunk_links()
{
	// auto res_range = this->inst.res_range();

	set<idx_t> link_set;
	
	size_t link_idx = 0;
	for (auto& chunk : this->chunks) {
		link_set.clear();

		for (auto o : chunk.prepr->ops) {
			auto& op = this->prepr.ops[o.idx];

			for (auto x : op.chunks) {
				if (x == chunk.idx) { continue; }

				link_set.insert(x);
			}

			for (auto s : op.inst->succ) {
				auto& succ = this->prepr.ops[s];

				for (auto x : succ.chunks) {
					if (x == chunk.idx) { continue; }

					link_set.insert(x);
				}
			}
		}

		chunk.fwd.set_size(link_set.size());

		for (auto x : link_set) {
			this->chunks[x].bkw.increment_size(1);
		}

		link_idx += 2*link_set.size();
	}

	this->chunk_link.resize(link_idx);

	link_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.all.set_size(chunk.fwd.size() + chunk.bkw.size());
		if (!chunk.all.empty()) {
			chunk.all.set_begin(&this->chunk_link[link_idx]);
		}
		
		chunk.fwd.assign_offset(this->chunk_link, link_idx, true);
		chunk.bkw.assign_offset(this->chunk_link, link_idx, true);
	}

	assert(link_idx == this->chunk_link.size());

	link_idx = 0;
	for (auto& chunk : this->chunks) {
		link_set.clear();

		for (auto o : chunk.prepr->ops) {
			auto& op = this->prepr.ops[o.idx];

			for (auto x : op.chunks) {
				if (x == chunk.prepr->idx) { continue; }

				link_set.insert(x);
			}

			for (auto s : op.inst->succ) {
				auto& succ = this->prepr.ops[s];

				for (auto x : succ.chunks) {
					if (x == chunk.prepr->idx) { continue; }

					link_set.insert(x);
				}
			}
		}

		assert(!link_set.contains(chunk.idx));
		for (auto x : link_set) {
			chunk.fwd.push_back(x);
			this->chunks[x].bkw.push_back(chunk.prepr->idx);
		}

		link_idx += 2*link_set.size();
	}

	assert(link_idx == this->chunk_link.size());

	for (auto& chunk : this->chunks) {
		assert(chunk.fwd.is_asc_strict());
		assert(chunk.bkw.is_asc_strict());
	}
}


void Link_graph::link_chunks(idx_t c_from, idx_t c_to)
{
	auto link_from = this->chunks[c_from].fwd.find_asc(c_to);
	auto link_to = this->chunks[c_to].bkw.find_asc(c_from);

	assert(link_from != nullptr && link_to != nullptr);

	link_from->active = true;
	link_from->forward = true;
	link_to->active = true;
	link_to->forward = false;
	
}


void Link_graph::link_op_self(idx_t o)
{
	auto& op = this->prepr.ops[o];

	for (auto a : op.chunks) {
		for (auto b : op.chunks) {
			if (a == b) { continue; }
			this->link_chunks(a, b);
		}
	}
}


void Link_graph::link_op_succ(idx_t o, idx_t s)
{
	for (auto a : this->prepr.ops[o].chunks) {
		for (auto b : this->prepr.ops[s].chunks) {
			if (a == b) { continue; }
			this->link_chunks(a, b);
		}
	}
}


void Link_graph::make_link_map()
{
	size_t n_res = this->inst.n_res;
	size_t n_trains = this->inst.n_trains();

	auto res_range = this->inst.res_range();

	Array<pair<idx_t, idx_t>>*** map = new pair<idx_t, idx_t>**[n_trains];
	pair<idx_t, idx_t>** map_l2 = new pair<idx_t, idx_t>*[n_trains*n_res];
	pair<idx_t, idx_t>* map_l3 = new pair<idx_t, idx_t>[n_trains*n_res*n_res];

	for (size_t t = 0; t < n_trains; t++) {
		map[t] = &map_l2[t*n_res];
	}

	for (size_t i = 0; i < n_trains*n_res; i++) {
		map_l2[i] = &map_l3[i*n_res];
	}

	for (size_t i = 0; i < n_trains*n_res*n_res; i++) {
		map_l3[i] = {IDX_MAX, IDX_MAX};
	}

	set<idx_t> link_set;
	for (auto& train : this->prepr.trains) {
		for (auto r : res_range) {
			for (auto& chunk : train.chunks[r]) {
				link_set.clear();
				for (auto& o : chunk.ops) {
					auto& op = this->prepr.ops[o.idx];
					for (auto x : op.chunks) {
						if (x != chunk.idx) {
							link_set.insert(x);
						}
					}

					for (auto& s : op.inst->succ) {
						for (auto x : this->prepr.ops[s].chunks) {
							if (x != chunk.idx) {
								link_set.insert(x);
							}
						}
					}
				}

				for (auto x : link_set) {
					auto rx = this->prepr.chunks[x].res;
					assert(rx != r);

					auto& entry = map[train.idx][r][rx];
					assert(entry.first == IDX_MAX && entry.second == IDX_MAX);

					entry = {chunk.idx, x};
				}
			}
		}
	}


	delete[] map;
	delete[] map_l2;
	delete[] map_l3;
}

void Link_graph::get_link_set(set<idx_t>& link_set, const Preprocess::Chunk& chunk)
{
	link_set.clear();


}



void Link_graph::clear_chunk(idx_t c)
{
	for (auto link : this->chunks[c].fwd) {
		auto x = link.idx;

		auto& other = this->chunks[x];
		auto other_link = other.bkw.find_asc(c);
		
		assert (other_link != nullptr);
		
		link.active = false;
		other_link->active = false;
	}

	
	for (auto link : this->chunks[c].bkw) {
		auto x = link.idx;

		auto& other = this->chunks[x];
		auto other_link = other.bkw.find_asc(c);
		
		assert (other_link != nullptr);
		
		link.active = false;
		other_link->active = false;
	}
}


