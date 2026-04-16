#include "disjoint_set.hpp"

#include <cassert>
#include <map>

Disjoint_set::Disjoint_set(const idx_t n_items) 
	: n_items(n_items), n_sets(n_items)
{
	this->parent.resize(n_items);
	this->size.resize(n_items);

	for (idx_t i = 0; i < n_items; i++) {
		parent[i] = i;
		size[i] = 1;
	}
}


Disjoint_set::idx_t Disjoint_set::find_set(idx_t v) const
{
	while (v != this->parent[v]) {
		this->parent[v] = this->parent[this->parent[v]];
		v = this->parent[v];
	}

	return v;
}


void Disjoint_set::union_set(idx_t a, idx_t b)
{
	a = find_set(a);
	b = find_set(b);
	if (a != b) {
		if (size[a] < size[b]) {
			std::swap(a, b);
		}

		this->parent[b] = a;
		this->size[a] += this->size[b];

		this->n_sets -= 1;
	}
}

std::vector<Disjoint_set::idx_t> Disjoint_set::get_result() const
{
	std::vector<idx_t> set_idx(this->n_items);

	std::map<idx_t, idx_t> idx_map;

	for (idx_t i = 0; i < this->n_items; i++) {
		idx_t s = this->find_set(i);

		if (!idx_map.contains(s)) {
			idx_map[s] = idx_map.size();
		}
	}

	for (idx_t i = 0; i < n_items; i++) {
		set_idx[i] = idx_map[this->find_set(i)];
	}

	assert((idx_t)idx_map.size() == this->n_sets);

	return set_idx;
}
