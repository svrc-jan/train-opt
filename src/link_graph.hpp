#pragma once

#include <set>
#include <limits>

#include "utils/unord_pair.hpp"
#include "utils/flag.hpp"
#include "utils/tracked.hpp"
#include "utils/batch.hpp"

#include "preprocess.hpp"


class Link_graph
{
public:
	enum Link_dir {FORWARD, BACKWARD};
	enum Chain_dir {PARALLEL, OPPOSITE, EITHER};
	enum Force_opt {FRC_NONE, FRC_FIRST, FRC_SECOND, FRC_BOTH};

	struct Chunk;
	struct Link;
	struct Lnk_chg;

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;
	typedef Preprocess::idx_pr idx_pr;

	Batch<idx_t, int8_t> op_change;
	Batch<idx_pr, int8_t> op_succ_change;
	Batch<idx_pr, int8_t> links_change;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;

	std::vector<Chunk> chunks = {};

	Link_graph(const Preprocess& prepr);
	~Link_graph();

	void sync(const Batch<idx_t, int8_t>& op_change, Batch<idx_pr, int8_t> const op_succ_change);

	void print_chains(bool force=false);
	
	template<Chain_dir chain_dir=EITHER, Force_opt force=FRC_NONE>
	size_t get_chain_length(idx_pr chunks, std::set<idx_pr>& chain, Flag& done);
	
	template<Chain_dir chain_dir, Force_opt force=FRC_NONE>
	void get_chain(std::set<idx_pr>& chain, const idx_pr& chunks);

	void get_chain_conf(std::set<idx_pr>& chain, const idx_pr& chunks);

private:
	std::vector<Link> chunk_links = {};
	std::vector<idx_pr> links_hlpr = {};

	void update_link(const Batch<idx_pr, int8_t>::Item& change);
	
	void make_chunks();
	void make_links();

	template<Chain_dir chain_dir, Force_opt force=FRC_NONE>
	void extend_chain(std::set<idx_pr>& chain, const idx_pr& chunks);
	
	template<Chain_dir chain_dir, Force_opt force=FRC_NONE,
		Link_dir first_dir, Link_dir second_dir>
	void extend_chain_in_dir(std::set<idx_pr>& chain, const idx_pr& chunks);
};

// size_t link_chunk_size = sizeof(Link_graph::Chunk);

struct Link_graph::Link
{
	idx_t res = IDX_MAX;
	idx_t chunk = IDX_MAX;
	Tracked<int8_t> count = 0;

	inline bool operator<(idx_t x) const { return chunk < x; }
	inline bool operator==(idx_t x) const { return chunk == x ; }
	inline bool active() const { return count.curr > 0; }
};


struct Link_graph::Chunk
{
	Array<Link> bkw;
	Array<Link> fwd;
	const Preprocess::Chunk* prepr = nullptr;
};


