#include "disjoint_set.hpp"

#include <cassert>

Disjoint_set::Disjoint_set(size_t n) 
{
	if (n > 0) {
		this->resize(n);
		this->reset();
	}
}


void Disjoint_set::resize(size_t n)
{
	assert(n <= IDX_MAX);
	this->n_items = n;
	
	this->parent.resize(n);
	this->size.resize(n);
	this->set_idx.resize(n);
}

void Disjoint_set::reserve(size_t n)
{
	this->parent.reserve(n);
	this->size.reserve(n);
	this->set_idx.reserve(n);
}


void Disjoint_set::reset()
{
	for (idx_t i = 0; i < this->n_items; i++) {
		parent[i] = i;
		size[i] = 1;
	}
	this->n_sets = this->n_items;
}


Disjoint_set::idx_t Disjoint_set::find_set(idx_t v) const
{
	while (v != this->parent[v]) {
		this->parent[v] = this->parent[this->parent[v]];
		v = this->parent[v];
	}
	return v;
}


void Disjoint_set::union_set(idx_t u, idx_t v)
{
	u = find_set(u);
	v = find_set(v);
	if (u != v) {
		if (size[u] < size[v]) {
			std::swap(u, v);
		}

		this->parent[v] = u;
		this->size[u] += this->size[v];
		this->n_sets -= 1;
	}
}

const std::vector<Disjoint_set::idx_t>&  Disjoint_set::get_result() const
{
	this->idx_map.clear();
	for (idx_t i = 0; i < this->n_items; i++) {
		idx_t s = this->find_set(i);

		if (!this->idx_map.contains(s)) {
			this->idx_map[s] = idx_map.size();
		}
	}

	for (idx_t i = 0; i < n_items; i++) {
		this->set_idx[i] = this->idx_map[this->find_set(i)];
	}

	assert((idx_t)this->idx_map.size() == this->n_sets);

	return set_idx;
}
