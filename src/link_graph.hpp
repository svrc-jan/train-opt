#pragma once

#include <set>
#include <limits>

#include "utils/unord_pair.hpp"
#include "utils/flag.hpp"
#include "preprocess.hpp"


class Link_graph
{
public:
	struct Chunk;
	struct Link;
	struct Map_entry;

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;

	std::vector<Chunk> chunks = {};

	Link_graph(const Preprocess& prepr);
	~Link_graph();

	void print_chains(bool force=false);
	void extend_chain(std::set<std::pair<idx_t, idx_t>>& chain, 
		const std::pair<idx_t, idx_t>& chunks, const std::pair<uint8_t, uint8_t>& force);

	void set_op_self(idx_t o);
	void set_op_succ(idx_t o, idx_t s);

private:

	std::vector<Link> chunk_links = {};

	void make_chunks();
	void make_links();

	
};

// size_t link_chunk_size = sizeof(Link_graph::Chunk);

struct Link_graph::Link
{
	idx_t res = IDX_MAX;
	idx_t chunk = IDX_MAX;
	uint8_t active = false;

	bool operator<(idx_t x) const { return chunk < x; }
	bool operator==(idx_t x) const { return chunk == x ; }
};


struct Link_graph::Chunk
{
	Array<Link> fwd;
	Array<Link> bkw;
	const Preprocess::Chunk* prepr = nullptr;
};
