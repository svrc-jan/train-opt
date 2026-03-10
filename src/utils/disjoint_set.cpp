#include "disjoint_set.hpp"

#include <cassert>
#include <map>

Disjoint_set::Disjoint_set(int n_items) 
	: n_items(n_items), n_sets(n_items)
{
	this->parent.resize(n_items);
	this->size.resize(n_items);

	for (int i = 0; i < n_items; i++) {
		parent[i] = i;
		size[i] = 1;
	}
}


int Disjoint_set::find_set(int v) const
{
	while (v != this->parent[v]) {
		this->parent[v] = this->parent[this->parent[v]];
		v = this->parent[v];
	}

	return v;
}


void Disjoint_set::union_set(int a, int b)
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


std::vector<int> Disjoint_set::get_result() const
{
	std::vector<int> set_idx(this->n_items);

	std::map<int, int> idx_map;

	for (int i = 0; i < this->n_items; i++) {
		if (i == this->parent[i]) {
			idx_map[i] = idx_map.size();
		}
	}

	for (int i = 0; i < n_items; i++) {
		set_idx[i] = idx_map[this->find_set(i)];
	}

	assert((int)idx_map.size() == this->n_sets);

	return set_idx;
}
