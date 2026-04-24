#include "link_graph.hpp"




using namespace std;


Link_graph::Link_graph(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{	
	cout << "Link_graph" << endl;
	this->make_chunks();

	this->make_chunk_links();
	cout << "  chunk links: " << this->chunk_link.size() << endl;

	this->make_confs();
	cout << "  chunk confs: " << this->confs.size() << endl;

	this->make_conf_links();
	cout << "  conf links:  " << this->conf_link.size() << endl;
}


void Link_graph::make_chunks()
{
	this->chunks.resize(this->prepr.n_chunks());

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

	assert(link_idx < LNK_MAX);
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


void Link_graph::make_confs()
{
	this->confs.clear();
	

	size_t conf_idx = 0;
	for (auto r : this->inst.res_range()) {
		auto& res_chunks = this->prepr.res_chunks[r];

		for (auto a = res_chunks.begin(); a < res_chunks.end(); a++) {
			auto& chunk_a = this->chunks[*a];

			for (auto b = a + 1; b < res_chunks.end(); b++) {
				auto& chunk_b = this->chunks[*b];

				assert(chunk_a.idx < chunk_b.idx);

				if (chunk_a.train == chunk_b.train) { continue; }

				bool found = false;
				for (auto lk_a : chunk_a.all) {
					idx_t r_a = this->chunks[lk_a.idx()].res;
					// assert(r_a != r);
					// if (r_a == r) { continue; }

					for (auto lk_b : chunk_b.all) {
						idx_t r_b = this->chunks[lk_b.idx()].res;
						// assert(r_b != r);
						// if (r_b == r) { continue; }

						if (r_a == r_b) {
							found = true;
							break;
						}
					}

					if (found) { break; }
				}

				assert(chunk_a.train < chunk_b.train);

				if (found) {
					this->confs.push_back({
						.idx = (lnk_t)this->confs.size(),
						.res = r,
						.train = {chunk_a.train, chunk_b.train},
						.chunk = {chunk_a.idx, chunk_b.idx},
						.par = {nullptr, nullptr},
						.opp = {nullptr, nullptr},
						.all = {nullptr, nullptr}
					});

					chunk_a.confs.increment_size(1);
					chunk_b.confs.increment_size(1);

					conf_idx += 2;
				}
			}
		}
	}

	
	this->confs.shrink_to_fit();

	sort(this->confs.begin(), this->confs.end(), 
		[](const Conf& a, const Conf& b){ return (a.chunk < b.chunk); });

	size_t n_confs = this->confs.size();
	for (size_t k = 0; k + 1 < n_confs; k++) {
		assert(this->confs[k].chunk != this->confs[k + 1].chunk);
	}

	this->chunk_conf.resize(conf_idx);

	conf_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.confs.assign_offset(this->chunk_conf, conf_idx, true);
	}
	assert(conf_idx == this->confs.size()*2);

	conf_idx = 0;
	for (auto& conf : this->confs) {
		conf.idx = conf_idx;
	
		this->chunks[conf.chunk.first].confs.push_back(conf.idx);
		this->chunks[conf.chunk.second].confs.push_back(conf.idx);

		conf_idx += 1;
	}

	assert(conf_idx == this->confs.size());
}

void make_chains(std::vector<Chain>& chains, 
	const Array<Link>& links_first,
	const Array<Link>& links_second);



void Link_graph::make_conf_links()
{
	set<lnk_t> link_par;
	set<lnk_t> link_opp;

	set<lnk_t> helper1;
	set<lnk_t> helper2;
	set<lnk_t> helper_res;

	for (auto& conf : this->confs) {
		link_par.clear();
		link_opp.clear();

		this->make_conf_link_set<true, true>(link_par, conf, helper1, helper2, helper_res);
		this->make_conf_link_set<true, false>(link_opp, conf, helper1, helper2, helper_res);

		this->make_conf_link_set<false, true>(link_opp, conf, helper1, helper2, helper_res);
		this->make_conf_link_set<false, false>(link_par, conf, helper1, helper2, helper_res);

		// fwd <-> fwd = par

		conf.par.set_size(link_par.size());
		conf.opp.set_size(link_opp.size());

		for (auto x : link_par) {
			this->conf_link.push_back(x);
		}

		for (auto x : link_opp) {
			this->conf_link.push_back(x);
		}
	}

	this->conf_link.shrink_to_fit();

	size_t link_idx = 0;
	for (auto& conf : this->confs) {
		conf.all.set_size(conf.par.size() + conf.par.size());
		if (!conf.all.empty()) {
			conf.all.set_begin(&this->chunk_link[link_idx]);
		}
		
		conf.par.assign_offset(this->chunk_link, link_idx, false);
		conf.opp.assign_offset(this->chunk_link, link_idx, false);
	}

	assert(link_idx == this->conf_link.size());

}

template <bool first_fwd, bool second_fwd>
void Link_graph::make_conf_link_set(set<lnk_t>& link_set, const Conf& conf,
	set<lnk_t>& helper1, set<lnk_t>& helper2, set<lnk_t>& helper_res)
{
	
	const Chunk& chunk_first = this->chunks[conf.chunk.first];
	const Chunk& chunk_second = this->chunks[conf.chunk.second];
	
	const Array<Link>* links_first = nullptr;
	const Array<Link>* links_second = nullptr;

	if constexpr(first_fwd) {
		links_first = &chunk_first.fwd;
	}
	else {
		links_first = &chunk_first.bkw;
	}

	if constexpr(first_fwd) {
		links_second = &chunk_second.fwd;
	}
	else {
		links_second = &chunk_second.bkw;
	}


	helper1.clear();
	helper2.clear();
	helper_res.clear();

	for (auto& lnk : *links_first) {
		auto& chunk_lnk = this->chunks[lnk.idx()];
		assert(chunk_first.train == chunk_lnk.train);
		
		for (auto k : chunk_lnk.confs) {
			const Conf& conf_lnk = this->confs[k];
			if (conf.train != conf_lnk.train) { continue; }
			helper1.insert(k);
			helper_res.insert(conf_lnk.res);
		}
	}

	for (auto& lnk : *links_second) {
		auto& chunk_lnk = this->chunks[lnk.idx()];
		assert(chunk_second.train == chunk_lnk.train);
		
		for (auto k : chunk_lnk.confs) {
			const Conf& conf_lnk = this->confs[k];
			if (conf.train != conf_lnk.train) { continue; }
			if (!helper_res.contains(conf_lnk.res)) { continue; }
			helper2.insert(k);
		}
	}

	set_intersection(
		helper1.begin(), helper1.end(),
		helper2.begin(), helper2.end(),
		inserter(link_set, link_set.end()));
}


size_t conf_size = sizeof(Link_graph::Conf);


// void Link_graph::build_conf_links(Conf& conf)
// {
// 	auto& chunk_first = this->chunks[conf.chunk.first];
// 	auto& chunk_second = this->chunks[conf.chunk.second];

// 	for (auto x : conf.par) {
// 		this->confs[x].par.erase(conf.idx);
// 	}

// 	for (auto x : conf.opp) {
// 		this->confs[x].opp.erase(conf.idx);
// 	}
	
// 	conf.par.clear();
// 	conf.opp.clear();

// 	this->add_conf_link_match(conf.par, chunk_first.fwd, chunk_second.fwd);
// 	this->add_conf_link_match(conf.opp, chunk_first.fwd, chunk_second.bkw);
// 	this->add_conf_link_match(conf.opp, chunk_first.bkw, chunk_second.fwd);
// 	this->add_conf_link_match(conf.par, chunk_first.bkw, chunk_second.bkw);

// 	for (auto x : conf.par) {
// 		this->confs[x].par.insert(conf.idx);
// 	}

// 	for (auto x : conf.opp) {
// 		this->confs[x].opp.insert(conf.idx);
// 	}
// }



// void Link_graph::build_all_conf_links()
// {
// 	for (auto& conf : this->confs) {
// 		this->build_conf_links(conf);
// 	}

// 	size_t count = 0;
// 	for (auto& conf : this->confs) {
// 		count += conf.par.size() + conf.opp.size();
// 	}

// 	cout << "conf links: " << count << endl;
// }


void Link_graph::add_conf_link_match(set<lnk_t>& res,
	const Array<Link>& first, const Array<Link>& second)
{
	for (auto& link_first : first) {
		if (!link_first.active()) { continue; }
		auto& chunk_first_link = this->chunks[link_first.idx()];

		for (auto& link_second : second) {
			if (!link_second.active()) { continue; }
			auto& chunk_second_link = this->chunks[link_second.idx()];

			if (chunk_first_link.res != chunk_second_link.res) { continue; };

			auto ret = this->find_conf({chunk_first_link.idx, chunk_second_link.idx});
			if (ret == nullptr) { continue; }

			res.insert(ret->idx);
		}
	}
}


void Link_graph::link_chunks(idx_t c_from, idx_t c_to)
{
	auto link_from = this->chunks[c_from].fwd.find_asc(c_to);
	auto link_to = this->chunks[c_to].bkw.find_asc(c_from);

	assert(link_from != nullptr && link_to != nullptr);

	link_from->set_true();
	link_to->set_true();
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


void Link_graph::clear_chunk(idx_t c)
{
	for (auto link : this->chunks[c].fwd) {
		auto x = link.idx();

		auto& other = this->chunks[x];
		auto other_link = other.bkw.find_asc(c);
		
		assert (other_link != nullptr);
		
		link.set_false();
		other_link->set_false();
	}

	
	for (auto link : this->chunks[c].bkw) {
		auto x = link.idx();

		auto& other = this->chunks[x];
		auto other_link = other.fwd.find_asc(c);
		
		assert (other_link != nullptr);

		link.set_false();
		other_link->set_false();
	}
}


Link_graph::Conf* Link_graph::find_conf(const Unord_pair<idx_t>& chunk)
{
	int low = 0;
	int high = this->confs.size() - 1;

	while (low <= high) {
		int mid = low + (high - low)/2;
		auto& conf = this->confs[mid];

		if (this->confs[mid] == chunk) {
			return &conf;
		}

		if (conf < chunk) {
			low = mid + 1;
		}
		else {
			high = mid - 1;
		}
	}
	
	return nullptr;
}

// void Link_graph::print_chains()
// {
// 	Flag conf_done(this->confs.size());

// 	vector<lnk_t> chain;

// 	size_t chain_idx = 0;

// 	for (auto& conf : this->confs) {
// 		if (conf_done[conf.idx]) { continue; }

// 		chain.clear();
// 		this->queue_.push(conf.idx);
// 		while (!this->queue_.empty()) {
// 			lnk_t k = this->queue_.front(); this->queue_.pop();

// 			chain.push_back(k);

// 			for (auto x : this->confs[k].par) {
// 				if (!conf_done[x]) {
// 					conf_done += x;
// 					this->queue_.push(x);
// 				}
// 			}

// 			for (auto x : this->confs[k].opp) {
// 				if (!conf_done[x]) {
// 					conf_done += x;
// 					this->queue_.push(x);
// 				}
// 			}
// 		}

// 		if (chain.size() == 1) {
// 			continue;
// 		}

// 		sort(chain.begin(), chain.end());

// 		idx_t t_a = this->chunks[conf.chunk.first].train;
// 		idx_t t_b = this->chunks[conf.chunk.second].train;

// 		cout << "chain " << ++chain_idx << " =";
// 		for (auto k : chain) {
// 			auto& curr_conf = this->confs[k];

// 			auto& a = this->chunks[curr_conf.chunk.first];
// 			auto& b = this->chunks[curr_conf.chunk.second];

// 			assert(t_a == a.train && t_b == b.train);

// 			cout << " " << a.idx << ":" << b.idx;
// 		}

// 		cout << endl;
// 	}
// }
