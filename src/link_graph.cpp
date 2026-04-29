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


void Link_graph::op_change(const Batch<idx_t, int16_t>& op_change, const Batch<idx_pr, int16_t>& op_succ_change)
{
	for (auto o : op_change) {
		this->prepr.get_op_links(this->links_hlpr, o.idx);
		for (auto x : this->links_hlpr) {
			this->links_change.push_back({x, o.value});
		}
	}

	for (auto& o_s : op_succ_change) {
		this->prepr.get_op_succ_links(this->links_hlpr, o_s.idx);
		for (auto x : this->links_hlpr) {
			this->links_change.push_back({x, o_s.value});
		}
	}
}


void Link_graph::sync()
{
	this->links_change.aggregate();
	for (auto& x : this->links_change) {
		assert(x.idx.first != x.idx.second);
		this->update_link(x);
	}
}



void Link_graph::update_link(const Batch<idx_pr, int16_t>::Item& item)
{
	if (item.value == 0) {
		return;
	}

	idx_t c1 = item.idx.first;
	idx_t c2 = item.idx.second;


	auto lnk_fwd = this->chunks[c1].fwd.find_asc(c2);
	auto lnk_bkw = this->chunks[c2].bkw.find_asc(c1);

	assert(lnk_fwd != nullptr && lnk_bkw != nullptr);
 
	lnk_fwd->count += item.value;
	lnk_bkw->count += item.value;

	assert(lnk_fwd->count >= 0 && lnk_bkw->count >= 0);
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
	vector<idx_pr> link_pairs = {};

	for (auto& chunk : this->chunks) {
		this->prepr.get_chunk_link_set(link_set, *chunk.prepr);
		
		chunk.fwd.set_size(link_set.size());

		for (auto x : link_set) {
			assert(x != chunk.prepr->idx);

			idx_t rx = this->prepr.chunks[x].res;
			assert(rx != chunk.prepr->res);

			link_pairs.push_back({chunk.prepr->idx, x});
			this->chunks[x].bkw.increment_size(1);

			link_idx += 1;
		}
	}

	this->chunk_links.resize(2*link_idx);

	link_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.bkw.assign_offset(this->chunk_links, link_idx, true);
		chunk.fwd.assign_offset(this->chunk_links, link_idx, true);
	}

	assert(link_idx == this->chunk_links.size());

	link_idx = 0;
	for (auto x : link_pairs) {
		auto& chunk_first = this->chunks[x.first];
		auto& chunk_second = this->chunks[x.second];

		chunk_second.bkw.push_back({chunk_first.prepr->res, x.first, 0});
		chunk_first.fwd.push_back({chunk_second.prepr->res, x.second, 0});
		
		link_idx += 1;
	}
	assert(2*link_idx == this->chunk_links.size());
}



void Link_graph::print_chains(bool force)
{
	Flag par_done(this->prepr.n_chunks());
	Flag opp_done(this->prepr.n_chunks());

	idx_pr curr;
	set<idx_pr> chain;

	vector<idx_t> par_lengths = {};
	vector<idx_t> opp_lengths = {};

	for (auto& train1 : this->prepr.trains) {
		for (idx_t t2 = train1.idx + 1; t2 < this->inst.n_trains(); t2++) {
			auto& train2 = this->prepr.trains[t2];

			par_done.clear();
			opp_done.clear();

			par_lengths.clear();
			opp_lengths.clear();

			for (auto r : this->inst.res_range()) {
				for (auto& chunk1 : train1.chunks[r]) {
					for (auto& chunk2 : train2.chunks[r]) {
						size_t length = 0;
						
						if (force) {
							length = this->get_chain_length<PARALLEL, FRC_BOTH>(
								{chunk1.idx, chunk2.idx}, chain, par_done);
						}
						else {
							length = this->get_chain_length<PARALLEL, FRC_NONE>(
								{chunk1.idx, chunk2.idx}, chain, par_done);
						}

						if (length > 1) {
							par_lengths.push_back(length);
						}

						if (force) {
							length = this->get_chain_length<OPPOSITE, FRC_BOTH>(
								{chunk1.idx, chunk2.idx}, chain, opp_done);
						}
						else {
							length = this->get_chain_length<OPPOSITE, FRC_NONE>(
								{chunk1.idx, chunk2.idx}, chain, opp_done);
						}

						if (length > 1) {
							opp_lengths.push_back(length);
						}
					}
				}
			}

			if (par_lengths.empty() && opp_lengths.empty()) { continue; }

			cout << train1.idx << " : " << train2.idx;

			if (!opp_lengths.empty()) {
				cout << ", opp: " << opp_lengths; 
			}

			if (!par_lengths.empty()) {
				cout << ", par: " << par_lengths; 
			}
			

			cout << endl;
		}
	}
}

template<Link_graph::Chain_dir chain_dir, Link_graph::Force_opt force>
size_t Link_graph::get_chain_length(idx_pr chunks,	set<idx_pr>& chain,	Flag& done)
{
	if (done[chunks.first] || done[chunks.second]) {
		return 0;
	}

	this->get_chain<chain_dir, force>(chain, chunks);
	for (auto x : chain) {
		done += x.first;
		done += x.second;
	}

	return chain.size();
}


void Link_graph::get_chain_conf(std::set<idx_pr>& chain, const idx_pr& chunks)
{
	this->get_chain<EITHER, FRC_NONE>(chain, chunks);
}


template<Link_graph::Chain_dir chain_dir, Link_graph::Force_opt force>
void Link_graph::get_chain(set<idx_pr>& chain, const idx_pr& chunks)
{
	chain.clear();
	chain.insert(chunks);
	this->extend_chain<chain_dir, force>(chain, chunks);
}


template<Link_graph::Chain_dir chain_dir, Link_graph::Force_opt force>
void Link_graph::extend_chain(set<idx_pr>& chain, const idx_pr& curr_chunks)
{
	if constexpr (chain_dir == PARALLEL || chain_dir == EITHER) {
		this->extend_chain_in_dir<chain_dir, force, FORWARD, FORWARD>(chain, curr_chunks);
		this->extend_chain_in_dir<chain_dir, force, BACKWARD, BACKWARD>(chain, curr_chunks);
	}

	if constexpr (chain_dir == OPPOSITE || chain_dir == EITHER) {
		this->extend_chain_in_dir<chain_dir, force, FORWARD, BACKWARD>(chain, curr_chunks);
		this->extend_chain_in_dir<chain_dir, force, BACKWARD, FORWARD>(chain, curr_chunks);
	}
}


template<Link_graph::Chain_dir chain_dir, Link_graph::Force_opt force, 
	Link_graph::Link_dir first_dir, Link_graph::Link_dir second_dir>
void Link_graph::extend_chain_in_dir(set<idx_pr>& chain, const idx_pr& curr_chunks)
{
	Array<Link>* links_first = nullptr;
	Array<Link>* links_second = nullptr;

	if constexpr (first_dir == FORWARD) {
		links_first = &this->chunks[curr_chunks.first].fwd;
	}
	else {
		links_first = &this->chunks[curr_chunks.first].bkw;
	}

	if constexpr (second_dir == FORWARD) {
		links_second = &this->chunks[curr_chunks.second].fwd;
	}
	else {
		links_second = &this->chunks[curr_chunks.second].bkw;
	}

	for (auto& lnk_first : *links_first) {
		if constexpr (force == FRC_NONE || force == FRC_SECOND) {
			if (!lnk_first.active()) { continue; }
		}

		for (auto& lnk_second : *links_second) {
			if constexpr (force == FRC_NONE || force == FRC_FIRST) {
				if (!lnk_second.active()) { continue; }
			}

			if (lnk_first.res != lnk_second.res) { continue; }

			idx_pr x = {lnk_first.chunk, lnk_second.chunk};
			auto ret = chain.insert(x);
			
			if (ret.second) {
				this->extend_chain<chain_dir, force>(chain, x);
			}
		}
	}
}



