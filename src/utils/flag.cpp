#include "flag.hpp"

#include <cstdlib>

#include "macros.hpp"

using namespace std;


Flag::Flag(const size_t n_items)
{
	this->set_n_items(n_items);
}


Flag::~Flag()
{
	if (this->data != nullptr) {
		free(this->data);
	}
}


void Flag::set_n_items(const size_t n)
{
	size_t old_size = this->req_size();
	this->n_items = n;
	size_t new_size = this->req_size();
	
	this->alloc_data(new_size);
	for (size_t i = old_size; i < new_size; i++) {
		this->data[i] = 0;
	}
}


void Flag::fill(const bool value)
{
	uint64_t mask = value ? ~(uint64_t)0 : 0;
	size_t size = Flag::get_required_size(this->n_items);

	for (size_t i = 0; i < size; i++) {
		this->data[i] = mask;
	}

	size_t rem_bits = this->n_items % 64;
	uint64_t last_mask = (rem_bits != 0) ? (((uint64_t)1 << rem_bits) - 1) : mask;
	
	this->data[size - 1] &= (mask & last_mask);
}


void Flag::set(const Flag& other)
{
	size_t size = other.req_size();
	this->alloc_data(size);

	for (size_t i = 0; i < size; i++) {
		this->data[i] = other.data[i];
	}
}

void Flag::set_true(const Flag& other)
{
	size_t min_size = MIN(this->req_size(), other.req_size());
	for (size_t i = 0; i < min_size; i++) {
		this->data[i] |= other.data[i];
	}
}


void Flag::set_false(const Flag& other)
{
	size_t min_size = MIN(this->req_size(), other.req_size());
	for (size_t i = 0; i < min_size; i++) {
		this->data[i] &= ~other.data[i];
	}
}


void Flag::mask(const Flag& other)
{
	size_t min_size = MIN(this->req_size(), other.req_size());
	for (size_t i = 0; i < min_size; i++) {
		this->data[i] &= other.data[i];
	}
}


size_t Flag::get_true_count() const
{
	size_t count = 0;
	size_t size = this->req_size();
	for (size_t i = 0; i < size; i++) {
		count += std::popcount(this->data[i]);
	}
	return count;
}


vector<size_t> Flag::get_true_list() const
{
	vector<size_t> ret = {};
	
	size_t true_count = this->get_true_count();
	if (true_count == 0) {
		return {};
	}

	ret.reserve(true_count);
	this->get_true_list<vector<size_t>, false>(ret);

	return ret;
}


void Flag::alloc_data(const size_t new_size)
{
	if (new_size <= this->capacity) {
		return;
	}

	if (this->data == nullptr) {
		this->data = (uint64_t*)malloc(sizeof(uint64_t)*new_size);
	}
	else {
		this->data = (uint64_t*)realloc(this->data, new_size);
	}

	if (this->data == nullptr) {
		abort();
	}

	this->capacity = new_size;
}


void Flag::Iter::roll_to_next()
{
	size_t size = this->flag.req_size();
	for (; this->i < size; this->i++) {
		uint64_t item = this->flag.data[i] >> this->j;
		for (; this->j < 64 && item != 0; this->j++) {
			if (item & (uint64_t)1) {
				this->idx = this->i*64 + this->j;
				this->j++;
				return;
			}
			item >>= 1;
		}
		this->j = 0;
	}

	this->idx = this->flag.n_items;
}


