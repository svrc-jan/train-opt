#include "link_graph.hpp"


#include "utils/stl_print.hpp"


using namespace std;


Link_graph::Link_graph(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{	
	cout << "Link_graph" << endl;

	this->make_chunks();
	this->make_links();

	cout << "  links: " << this->chunk_links.size()/2 << endl;

}


Link_graph::~Link_graph()
{

}

void Link_graph::make_chunks()
{
	this->chunks.resize(this->prepr.n_chunks());

	for (auto c : this->prepr.chunks_range()) {
		auto& chunk = this->chunks[c];
		chunk.fwd.clear();
		chunk.bkw.clear();
		chunk.prepr = &this->prepr.chunks[c];
	}
}

void Link_graph::make_links()
{
	set<idx_t> link_set;

	size_t link_idx = 0;
	for (auto& chunk : this->chunks) {
		this->prepr.get_link_set(link_set, *chunk.prepr);
		
		chunk.fwd.set_size(link_set.size());
		link_idx += link_set.size();

		idx_t prev_r = IDX_MAX;
		for (auto x : link_set) {
			idx_t r = this->prepr.chunks[x].res;
			assert(prev_r <= r || prev_r == IDX_MAX);
		
			this->chunk_links.push_back({r, x});
			this->chunks[x].bkw.increment_size(1);

			prev_r = r;
		}
	}

	this->chunk_links.resize(2*link_idx);

	link_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.fwd.assign_offset(this->chunk_links, link_idx, false);
	}

	for (auto& chunk : this->chunks) {
		chunk.bkw.assign_offset(this->chunk_links, link_idx, true);
	}

	assert(link_idx == this->chunk_links.size());

	for (auto& chunk : this->chunks) {
		idx_t r = chunk.prepr->res;

		for (auto& lnk : chunk.fwd) {
			this->chunks[lnk.chunk].bkw.push_back({r, chunk.prepr->idx});
		}
	}
}


void Link_graph::print_chains(bool force)
{
	Flag chunk_done(this->prepr.n_chunks());

	pair<idx_t, idx_t> curr;
	set<pair<idx_t, idx_t>> chain;

	for (auto& train1 : this->prepr.trains) {
		for (idx_t t2 = train1.idx + 1; t2 < this->inst.n_trains(); t2++) {
			auto& train2 = this->prepr.trains[t2];

			chunk_done.clear();

			bool start_printed = false;

			for (auto r : this->inst.res_range()) {
				for (auto& chunk1 : train1.chunks[r]) {
					if (chunk_done[chunk1.idx]) { continue; }

					for (auto& chunk2 : train2.chunks[r]) {
						if (chunk_done[chunk2.idx]) { continue; }

						chain.clear();

						pair<idx_t, idx_t> x = {chunk1.idx, chunk2.idx};
						chain.insert(x);
						this->extend_chain(chain, x, {force, force});

						for (auto k : chain) {
							chunk_done += k.first;
							chunk_done += k.second;
						}

						if (chain.size() > 1) {
							if (!start_printed) {
								cout << endl << train1.idx << " / " << t2 << " chain lengths:";
								start_printed = true;
							}
							cout << " " << chain.size();
						}
					}
				}
			}
		}
	}

	cout << endl;
}


void Link_graph::extend_chain(set<pair<idx_t, idx_t>>& chain, 
	const pair<idx_t, idx_t>& curr, const pair<uint8_t, uint8_t>& force)
{
	auto& chunk_first = this->chunks[curr.first];
	auto& chunk_second = this->chunks[curr.second];

	for (auto& lnk_first : chunk_first.fwd) {
		if (!lnk_first.active && !force.first) { continue; }

		for (auto& lnk_second : chunk_second.bkw) {
			if (!lnk_second.active && !force.second) { continue; }

			if (lnk_first.res != lnk_second.res) { continue; }

			pair<idx_t, idx_t> x = {lnk_first.chunk, lnk_second.chunk};
			auto ret = chain.insert(x);
			
			if (ret.second) {
				this->extend_chain(chain, x, force);
			}
		}
	}

	for (auto& lnk_first : chunk_first.bkw) {
		if (!lnk_first.active && !force.first) { continue; }
		
		for (auto& lnk_second : chunk_second.fwd) {
			if (!lnk_second.active && !force.second) { continue; }

			if (lnk_first.res != lnk_second.res) { continue; }

			pair<idx_t, idx_t> x = {lnk_first.chunk, lnk_second.chunk};
			auto ret = chain.insert(x);
			
			if (ret.second) {
				this->extend_chain(chain, x, force);
			}
		}
	}
}


void Link_graph::set_op_succ(idx_t o, idx_t s)
{
	for (auto c_o : this->prepr.ops[o].chunks) {
		auto chunk_o = this->chunks[c_o];

		for (auto c_s : this->prepr.ops[s].chunks) {
			auto chunk_s = this->chunks[c_s];

			if (c_o == c_s) { continue; }

			auto lnk_o = chunk_o.fwd.find_asc(c_s);
			auto lnk_s = chunk_s.bkw.find_asc(c_o);

			assert(lnk_o != nullptr && lnk_s != nullptr);
			
			lnk_o->active = true;
			lnk_s->active = true;
		}
	}
}
