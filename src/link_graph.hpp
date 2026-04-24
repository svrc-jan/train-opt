#pragma once

#include <limits>
#include <unordered_map>

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

	static constexpr lnk_t FWD_MSK = ((uint32_t)1 << 31);
	static constexpr lnk_t BKW_MSK = ((uint32_t)1 << 30);
	static constexpr lnk_t BTH_MSK = FWD_MSK | BKW_MSK;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr lnk_t LNK_MAX = (std::numeric_limits<lnk_t>::max() & ~BTH_MSK);

	std::vector<Chunk> chunks;
	std::vector<Conf> confs;

	Flag chunk_conf_dirty;

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

	void print_chains();

private:
	std::vector<Link> chunk_link;
	std::vector<Link> chunk_conf;
	std::vector<Link> conf_link;

	void make_chunks();
	void make_chunk_links();
	void make_confs();
	void make_conf_links();

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

class Link_graph::Link
{
public:
	Link(const lnk_t & value=LNK_MAX) : value(value & LNK_MAX) {}
	inline lnk_t idx() const { return (value & LNK_MAX); }

	inline bool active() const { return ((value & BTH_MSK) != 0); }
	inline bool is_fwd() const { return ((value & FWD_MSK) != 0); }
	inline bool is_bkw() const { return ((value & BKW_MSK) != 0); }
	inline bool is_both() const { return (value >= BTH_MSK); }

	inline void set_false() { value &= ~BTH_MSK; }
	inline void set_true() { value |= BTH_MSK; }
	inline void set_fwd() { value |= FWD_MSK; }
	inline void set_bkw() { value |= BKW_MSK; }

	template<typename T>
	bool operator<(const T& x) const { return idx() < x; }

	template<typename T>
	bool operator==(const T& x) const { return idx() == x; }

private:
	lnk_t value = IDX_MAX;
};

struct Link_graph::Conf
{
	lnk_t idx = LNK_MAX;
	Unord_pair<idx_t> chunk;

	std::set<lnk_t> par;
	std::set<lnk_t> opp;

	bool operator<(const Unord_pair<idx_t>& x) const { return chunk < x; }
	bool operator==(const Unord_pair<idx_t>& x) const { return chunk == x; }

	bool operator<(const Conf& x) const { return chunk < x.chunk; }
	bool operator==(const Conf& x) const { return chunk == x.chunk; }
};

