#pragma once

#include <cstdint>
#include <limits>
#include <vector>


struct Disjoint_set
{
	typedef uint16_t idx_t;

	static const idx_t IDX_MAX = std::numeric_limits<idx_t>::max();

	idx_t n_items;
	idx_t n_sets;

	mutable std::vector<idx_t> parent = {};
	std::vector<idx_t> size = {};

	Disjoint_set(const idx_t n_items);
	
	idx_t find_set(idx_t v) const;
	void union_set(idx_t a, idx_t b);

	std::vector<idx_t> get_result() const;
};
