#include "link_graph.hpp"

#include <map>

#include "utils/aux.hpp"
#include "utils/stl_print.hpp"


using namespace std;


Link_graph::Link_graph(const Preprocess& prepr, bool verify)
	: inst(prepr.inst), prepr(prepr)
{	
	cout << "Link_graph" << endl;

	this->make_chunks();
	this->make_links();
	cout << "  links: " << this->links.size() << endl;

	if (verify) {
		this->verify_links();
	}

	this->make_op_links();
	
	this->make_confs();
	cout << "  confs: " << this->confs.size() << endl;

	this->make_conf_links();
	cout << "  c lnk: " << this->conf_links.size() << endl;

	this->make_chunk_confs();
}


Link_graph::~Link_graph()
{

}


void Link_graph::op_change(const Flag& op_change_flag)
{
	op_change_flag.get_true_list(this->flag_list);
	for (auto o : this->flag_list) {
		for (auto l : this->op_links[o]) {
			this->link_dirty += l;
		}
	}
}


void Link_graph::sync_links(const Flag& op_active)
{
	this->link_dirty.get_true_list(this->flag_list);
	for (auto l : this->flag_list) {
		this->link_active -= l;
		for (auto o : this->links[l].ops) {
			if (op_active[o.first] && op_active[o.second]) {
				this->link_active += l;
				break;
			}
		}
	}

	this->link_dirty.clear();
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

	this->chunk_req.set_n_items(this->prepr.n_chunks());
	this->chunk_max_chain.resize(this->prepr.n_chunks());
}

void Link_graph::make_links()
{
	for (auto& chunk : this->chunks) {
		this->add_links(chunk);
		chunk.fwd.clear();
		chunk.bkw.clear();
	}

	this->links.shrink_to_fit();
	this->link_ops.shrink_to_fit();

	size_t op_idx = 0;
	size_t link_idx = 0;
	for (auto& link : this->links) {
		link.ops.assign_offset(this->link_ops, op_idx, false);

		this->chunks[link.chunk.first].fwd.increment_size(1);
		this->chunks[link.chunk.second].bkw.increment_size(1);

		link_idx += 2;
	}

	this->chunk_links.resize(link_idx);

	link_idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.fwd.assign_offset(this->chunk_links, link_idx, true);
		chunk.bkw.assign_offset(this->chunk_links, link_idx, true);
	}

	assert(link_idx == this->chunk_links.size());

	for (link_idx = 0; link_idx < this->links.size(); link_idx++) {
		auto& link = this->links[link_idx];

		assert(link.chunk.first < this->prepr.n_chunks());
		assert(link.chunk.second < this->prepr.n_chunks());

		this->chunks[link.chunk.first].fwd.push_back(link_idx);
		this->chunks[link.chunk.second].bkw.push_back(link_idx);
	}

	this->link_active.set_n_items(this->links.size());
	this->link_dirty.set_n_items(this->links.size());
}


void Link_graph::add_links(Chunk& chunk)
{
	map<idx_t, vector<idx_pr>>  link_map;

	for (auto o : chunk.prepr->ops) {
		auto& op = this->prepr.ops[o];
		for (auto c : op.chunks) {
			if (c == chunk.prepr->idx) {
				continue;
			}

			link_map[c].push_back({o, o});
		}

		for (auto s : op.inst->succ) {
			auto& succ = this->prepr.ops[s];
			for (auto c : succ.chunks) {
				if (c == chunk.prepr->idx) {
					continue;
				}

				if (op.chunks.find_asc(c) != nullptr) {
					continue;
				}

				link_map[c].push_back({o, s});
			}
		}
	}

	for (auto it = link_map.begin(); it != link_map.end(); it++) {
		Link link = {
			.res = {chunk.prepr->res, this->prepr.chunks[it->first].res},
			.chunk = {chunk.prepr->idx, it->first},
			.ops = {nullptr, nullptr},
		};

		link.ops.set_size(it->second.size());

		this->links.push_back(link);
		for (auto x : it->second) {
			this->link_ops.push_back(x);
		}
	}
}


void Link_graph::make_op_links()
{
	this->op_links.resize(this->inst.n_ops(), {nullptr, nullptr});

	size_t op_link_idx = 0;
	for (lnk_t l = 0; l < this->links.size(); l++) {
		for (auto x : this->links[l].ops) {
			this->op_links[x.first].increment_size(1);
			op_link_idx++;

			if (x.first == x.second) {
				continue;
			}

			this->op_links[x.second].increment_size(1);
			op_link_idx++;
		}
	}

	this->op_links_data.resize(op_link_idx);

	op_link_idx = 0;
	for (auto o : this->inst.ops_range()) {
		this->op_links[o].assign_offset(this->op_links_data, op_link_idx, true);
	}

	for (lnk_t l = 0; l < this->links.size(); l++) {
		for (auto x : this->links[l].ops) {
			this->op_links[x.first].push_back(l);
			op_link_idx++;

			if (x.first == x.second) {
				continue;
			}

			this->op_links[x.second].push_back(l);
			op_link_idx++;
		}
	}
}


void Link_graph::verify_links()
{
	
}


void Link_graph::make_confs()
{
	vector<idx_pr> pairs;

	for (auto r : this->inst.res_range()) {
		auto& chunks = this->prepr.res_chunks[r];

		for (auto a = chunks.begin(); a < chunks.end(); a++) {
			for (auto b = a + 1; b < chunks.end(); b++) {
				assert(*a < *b);
				
				if (this->conf_has_possible_link({*a, *b})) {
					pairs.push_back({*a, *b});
				}
			}
		}
	}

	make_unique(pairs);

	auto pred = pairs[0];
	for (auto x : pairs | views::drop(1)) {
		assert(pred < x);
		pred = x;
	}

	this->confs.reserve(pairs.size());

	for (auto x : pairs) {
		Conflict conf = {
			.idx = (lnk_t)this->confs.size(),
			.chunk = x,
			.links = {nullptr, nullptr}
		};

		this->conf_map[CONF_IDX(conf.chunk.first, conf.chunk.second)] = conf.idx;

		this->confs.push_back(conf);
	}

	assert(this->confs.size() == this->conf_map.size());

	this->conf_chain_len.resize(this->confs.size());
}


bool Link_graph::conf_has_possible_link(const idx_pr& chunk)
{
	auto& chunk_a = this->chunks[chunk.first];
	auto& chunk_b = this->chunks[chunk.second];

	assert(chunk_a.prepr->res == chunk_b.prepr->res);

	if (chunk_a.prepr->train == chunk_b.prepr->train) {
		return false;
	}

	if (this->conf_has_possible_link_dir(chunk, {true, true})) {
		return true;
	}

	if (this->conf_has_possible_link_dir(chunk, {true, false})) {
		return true;
	}

	if (this->conf_has_possible_link_dir(chunk, {false, true})) {
		return true;
	}

	if (this->conf_has_possible_link_dir(chunk, {false, false})) {
		return true;
	}

	return false;
}


bool Link_graph::conf_has_possible_link_dir(const idx_pr& chunk, const pair<int8_t, int8_t>& fwd)
{
	auto& chunk_a = this->chunks[chunk.first];
	auto& chunk_b = this->chunks[chunk.second];

	for (auto l_a : (fwd.first ? chunk_a.fwd : chunk_a.bkw)) {
		auto& lnk_a = this->links[l_a];
		idx_t r_a = fwd.first ? lnk_a.res.second : lnk_a.res.first;

		for (auto l_b : (fwd.second ? chunk_b.fwd : chunk_b.bkw)) {
			auto& lnk_b = this->links[l_b];
			idx_t r_b = fwd.second ? lnk_b.res.second : lnk_b.res.first;

			if (r_a == r_b) {
				return true;
			}
		}
	}

	return false;
}

void Link_graph::make_conf_links()
{
	for (auto& conf : this->confs) {
		this->add_conf_links(conf);
	}

	this->conf_links.shrink_to_fit();

	size_t idx = 0;
	for (auto& conf : this->confs) {
		conf.links.assign_offset(this->conf_links, idx, false);
	}

	this->conf_done.set_n_items(this->confs.size());
}


void Link_graph::add_conf_links(Conflict& conf)
{
	conf.links.clear();
	this->add_conf_links_dir(conf, {true, true});
	this->add_conf_links_dir(conf, {true, false});
	this->add_conf_links_dir(conf, {false, true});
	this->add_conf_links_dir(conf, {false, false});
}


void Link_graph::add_conf_links_dir(Conflict& conf, const std::pair<int8_t, int8_t>& fwd)
{
	auto& chunk_a = this->chunks[conf.chunk.first];
	auto& chunk_b = this->chunks[conf.chunk.second];

	for (auto l_a : (fwd.first ? chunk_a.fwd : chunk_a.bkw)) {
		auto& lnk_a = this->links[l_a];
		idx_t r_a = fwd.first ? lnk_a.res.second : lnk_a.res.first;
		idx_t c_a = fwd.first ? lnk_a.chunk.second : lnk_a.chunk.first;

		for (auto l_b : (fwd.second ? chunk_b.fwd : chunk_b.bkw)) {
			auto& lnk_b = this->links[l_b];
			idx_t r_b = fwd.second ? lnk_b.res.second : lnk_b.res.first;
			idx_t c_b = fwd.second ? lnk_b.chunk.second : lnk_b.chunk.first;

			if (r_a == r_b) {
				conf.links.increment_size(1);
				this->conf_links.push_back({
					.idx = this->conf_map[CONF_IDX(c_a, c_b)],
					.link = {l_a, l_b},
					.opp = (fwd.first != fwd.second)
				});
			}
		}
	}
}


void Link_graph::make_chunk_confs()
{
	for (auto& conf : this->confs) {
		this->chunks[conf.chunk.first].confs.increment_size(1);
		this->chunks[conf.chunk.second].confs.increment_size(1);
	}

	this->chunk_confs.resize(2*this->confs.size());

	size_t idx = 0;
	for (auto& chunk : this->chunks) {
		chunk.confs.assign_offset(this->chunk_confs, idx, true);
	}

	for (auto& conf : this->confs) {
		this->chunks[conf.chunk.first].confs.push_back(conf.idx);
		this->chunks[conf.chunk.second].confs.push_back(conf.idx);
	}
}


void Link_graph::update_max_chain()
{
	this->conf_done.clear();
	size_t n_confs = this->confs.size();

	for (size_t k = 0; k < n_confs; k++) {
		this->conf_chain_len[k] = 0;
	}

	for (size_t k = 0; k < n_confs; k++) {
		this->chain.clear();

		this->chain_search(k);

		if (this->chain.empty()) {
			continue;
		}

		size_t chain_len = this->chain.size();
		assert(chain_len < 255);

		for (auto x : this->chain) {
			this->conf_chain_len[x] = (uint8_t)chain_len;
		}
	}

	for (auto c : this->prepr.chunks_range()) {
		chunk_max_chain[c] = 0;
		for (auto k : this->chunks[c].confs) {
			chunk_max_chain[c] = MAX(chunk_max_chain[c], conf_chain_len[k]);
		}
	}
}


void Link_graph::chain_search(lnk_t k)
{
	if (this->conf_done[k]) {
		return;
	}

	this->conf_done += k;
	this->chain.push_back(k);

	auto& conf = this->confs[k];

	for (auto& link : conf.links) {
		if (this->conf_done[link.idx]) {
			continue;
		}

		if (!link.opp) {
			continue;
		}

		if (!this->link_active[link.link.first] || !this->link_active[link.link.second]) {
			continue;
		}

		this->chain_search(link.idx);
	}
}

Link_graph::idx_t Link_graph::median_chain()
{
	std::vector<idx_t> vals;
	vals.reserve(this->chunks.size());

	for (auto c : this->prepr.chunks_range()) {
		idx_t x = this->chunk_max_chain[c];

		if (x > 1) {
			vals.push_back(x);
		}
	}

	sort(vals.begin(), vals.end());

	return vals[vals.size()/2];
}

