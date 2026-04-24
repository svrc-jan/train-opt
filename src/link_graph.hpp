#pragma once

#include <set>
#include <limits>
#include <map>

#include "utils/unord_pair.hpp"
#include "utils/flag.hpp"
#include "preprocess.hpp"


class Link_graph
{
public:
	class Link;
	struct Chunk;
	struct Conf;
	
	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;
	typedef uint32_t lnk_t;

	static constexpr lnk_t ACT_MSK = ((uint32_t)1 << 31);

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr lnk_t LNK_MAX = (std::numeric_limits<lnk_t>::max() & ~ACT_MSK);

	std::vector<Chunk> chunks;

	typedef Unord_pair<Unord_pair<idx_t>> Link_conf;
	std::vector<Link_conf> par_link_confs = {};
	std::vector<Link_conf> opp_link_confs = {};

	std::map<Unord_pair<idx_t>, lnk_t> chunks_to_conf; 

	Link_graph(const Preprocess& prepr);

	void link_chunks(idx_t c_from, idx_t c_to);
	void link_op_self(idx_t o);
	void link_op_succ(idx_t o, idx_t s);
	void clear_chunk(idx_t c);

	void build_all_conf_links();
	void build_conf_links(Conf& conf);
	
	void add_conf_link_match(std::set<lnk_t>& res, 
		const Array<Link>& first, const Array<Link>& second);

	std::queue<lnk_t> queue_;

	template <bool first, bool fwd>
	void make_conf_link_set(std::set<lnk_t>& link_set, const Conf& conf,
		std::set<lnk_t>& helper1, std::set<lnk_t>& helper2,
		std::set<lnk_t>& helper_res);

	void print_chains();

private:
	std::vector<Link> chunk_link;
	std::vector<Link> chunk_conf;
	std::vector<Link> conf_link;

	void make_chunks();
	void make_chunk_links();

	template<bool first_fwd, bool second_fwd>
	void make_chunks_link_confs(const Chunk& chunk_first, const Chunk& chunk_second);

	Conf* find_conf(const Unord_pair<idx_t>& chunk);
};


struct Link_graph::Chunk
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t res = IDX_MAX;
	Array<Link> fwd;
	Array<Link> bkw;
	Array<Link> all;
	Array<lnk_t> confs;

	const Preprocess::Chunk* prepr = nullptr;
};

// size_t link_chunk_size = sizeof(Link_graph::Chunk);

struct Link_graph::Conf
{
	lnk_t idx = LNK_MAX;
	idx_t res = IDX_MAX;
	Unord_pair<idx_t> train;
	Unord_pair<idx_t> chunk;
	Array<Link> par;
	Array<Link> opp;

	bool operator<(const Unord_pair<idx_t>& x) const { return chunk < x; }
	bool operator==(const Unord_pair<idx_t>& x) const { return chunk == x; }

	bool operator<(const Conf& x) const { return chunk < x.chunk; }
	bool operator==(const Conf& x) const { return chunk == x.chunk; }
};


class Link_graph::Link
{
public:
	Link(const lnk_t & value=LNK_MAX) : value(value & LNK_MAX) {}
	inline lnk_t idx() const { return (value & LNK_MAX); }

	inline bool active() const { return ((value & ACT_MSK) != 0); }

	inline void set_false() { value &= ~ACT_MSK; }
	inline void set_true() { value |= ACT_MSK; }

	template<typename T>
	bool operator<(const T& x) const { return idx() < x; }

	template<typename T>
	bool operator==(const T& x) const { return idx() == x; }

	template<typename T>
	bool operator<=(const T& x) const { return idx() <= x; }

	bool operator<(const Link& x) const { return idx() < x.idx(); }
	bool operator==(const Link& x) const { return idx() == x.idx(); }
	bool operator<=(const Link& x) const { return idx() <= x.idx(); }


private:
	lnk_t value = IDX_MAX;
};

