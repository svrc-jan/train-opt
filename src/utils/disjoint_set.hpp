#pragma once

#include <cstdint>
#include <limits>
#include <vector>
#include <map>

struct Disjoint_set
{
	typedef uint16_t idx_t;

	static constexpr idx_t IDX_MAX = std::numeric_limits<idx_t>::max();

	idx_t n_items = 0;
	idx_t n_sets = 0;

	mutable std::vector<idx_t> parent = {};
	std::vector<idx_t> size = {};
	mutable std::vector<idx_t> set_idx = {};
	mutable std::map<idx_t, idx_t> idx_map = {};

	Disjoint_set(size_t n=0);

	void reserve(size_t n);
	void resize(size_t n);
	void reset();

	idx_t find_set(idx_t v) const;
	void union_set(idx_t a, idx_t b);

	const std::vector<idx_t>& get_result() const;
};
