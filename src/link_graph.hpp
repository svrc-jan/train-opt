#pragma once

#include <set>
#include <limits>

#include "utils/unord_pair.hpp"
#include "utils/flag.hpp"
#include "preprocess.hpp"


class Link_graph
{
public:
	struct Entry;
	struct Link;
	struct Chunk;

	enum Link_dir { FORWARD, BACKWARD };
	enum Chain_dir { PARALLEL, OPPOSITE, EITHER };

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;

	std::vector<Chunk> chunks;

	Link_graph(const Preprocess& prepr);


	void make_link_map();

	void link_chunks(idx_t c_from, idx_t c_to);
	void link_op_self(idx_t o);
	void link_op_succ(idx_t o, idx_t s);
	void clear_chunk(idx_t c);

	void get_linked_confs();

	void print_chains();

private:
	std::vector<Link> chunk_link;
	std::vector<Link> chunk_conf;
	std::vector<Link> conf_link;

	void get_link_set(std::set<idx_t>& link_set, const Preprocess::Chunk& chunk);

	void make_chunks();
	void make_chunk_links();

	Flag visited;
	std::queue<Unord_pair<idx_t>> queue_;

	template<Chain_dir>
	std::set<Unord_pair<idx_t>> get_chain(
		std::pair<idx_t, idx_t> chunk, std::pair<uint8_t, uint8_t> force);

	template<Link_graph::Link_dir dir>
	bool is_linked(idx_t c_from, idx_t c_to, bool force) const;
};


struct Link_graph::Chunk
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t res = IDX_MAX;
	Array<Link> fwd;
	Array<Link> bkw;
	Array<Link> all;

	const Preprocess::Chunk* prepr = nullptr;
};

// size_t link_chunk_size = sizeof(Link_graph::Chunk);

struct Link_graph::Link
{
public:
	idx_t idx = IDX_MAX;
	uint8_t active = false;
	uint8_t forward = true;

	Link(const idx_t& idx=IDX_MAX) : idx(idx), active(false), forward(true) {}

	template<typename T>
	bool operator<(const T& x) const { return idx < x; }

	template<typename T>
	bool operator==(const T& x) const { return idx == x; }

	template<typename T>
	bool operator<=(const T& x) const { return idx <= x; }

	bool operator<(const Link& x) const { return idx < x.idx; }
	bool operator==(const Link& x) const { return idx == x.idx; }
	bool operator<=(const Link& x) const { return idx <= x.idx; }
};

