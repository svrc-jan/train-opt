#pragma once

#include <unordered_map>
#include <limits>

#include "utils/flag.hpp"
#include "utils/tracked.hpp"
#include "utils/batch.hpp"

#include "preprocess.hpp"

#define CONF_HASH(a, b) (a < b) ? (((lnk_t)a << 16) | (lnk_t)b) : (((lnk_t)b << 16) | (lnk_t)a)


class Link_graph
{
public:

	struct Chunk;
	struct Link;
	struct Conflict;
	struct Conf_link;

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;
	typedef Preprocess::idx_pr idx_pr;
	typedef uint32_t lnk_t;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr lnk_t CNF_MAX = std::numeric_limits<lnk_t>::max();

	std::vector<Chunk> chunks = {};
	std::vector<Conflict> confs = {};

	Link_graph(const Preprocess& prepr, bool verbose=false, bool verify=false);
	~Link_graph();

	void op_change(const Flag& op_change_flag);
	void sync_links(const Flag& op_active);

	void get_chain_len(const std::vector<idx_pr>& confs, std::vector<idx_t>& len);

	idx_t median_chain();

private:
	
	std::vector<Link> links = {};
	std::vector<idx_pr> link_ops = {};
	
	std::vector<lnk_t> chunk_links = {};
	std::vector<Array<idx_t>> op_links = {};
	std::vector<idx_t> op_links_data = {};

	std::vector<Conf_link> conf_links = {};
	std::vector<lnk_t> chunk_confs = {};

	std::unordered_map<lnk_t, lnk_t> conf_map;

	Flag link_active;
	Flag link_dirty;

	Flag conf_done;
	std::vector<lnk_t> conf_to_do;
	std::vector<lnk_t> chain;
	std::vector<uint16_t> conf_chain_len;

	std::vector<idx_t> flag_list;

	void make_chunks();
	void make_links();
	void make_op_links();
	void make_confs();
	void make_conf_links();
	void make_chunk_confs();

	void add_links(Chunk& chunk);
	bool conf_has_possible_link(const idx_pr& chunk);
	bool conf_has_possible_link_dir(const idx_pr& chunk, const std::pair<int8_t, int8_t>& fwd);
	void add_conf_links(Conflict& conf);
	void add_conf_links_dir(Conflict& conf, const std::pair<int8_t, int8_t>& fwd);

	void verify_links();

	void chain_search(lnk_t k, bool need_opp=false);
};

// size_t link_chunk_size = sizeof(Link_graph::Chunk);


struct Link_graph::Chunk
{
	Array<lnk_t> fwd;
	Array<lnk_t> bkw;
	Array<lnk_t> confs;
	const Preprocess::Chunk* prepr = nullptr;
};


struct Link_graph::Link
{
	idx_pr res = {IDX_MAX, IDX_MAX};
	idx_pr chunk = {IDX_MAX, IDX_MAX};
	Array<idx_pr> ops;
};


struct Link_graph::Conflict
{
	lnk_t idx = CNF_MAX;
	idx_pr chunk = {IDX_MAX, IDX_MAX};
	Array<Conf_link> links;
};

struct Link_graph::Conf_link
{
	lnk_t idx = CNF_MAX;
	std::pair<lnk_t, lnk_t> link;
	int8_t opp = 0;
	int8_t active = 0;
};