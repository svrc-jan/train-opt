#include "flag.hpp"

#include <cstdlib>

#include "macros.hpp"

using namespace std;


Flag::Flag(const size_t n_items)
{
	this->set_n_items(0);
}


Flag::~Flag()
{
	if (this->data != nullptr) {
		free(this->data);
	}
}


void Flag::set_n_items(const size_t n_items)
{
	if (n_items > 0) {
		this->alloc_data(n_items);
	}
	this->n_items = n_items;
}


void Flag::fill(const bool value)
{
	const uint64_t mask = value ? ~(uint64_t)0 : 0;
	for (size_t i = 0; i < this->size; i++) {
		this->data[i] = mask;
	}

	const size_t rem_bits = this->n_items % 64;
	const uint64_t last_mask = (rem_bits != 0) ? ~(~(uint64_t)0 << rem_bits) : ~(uint64_t)0;
	
	this->data[this->size - 1] &= last_mask;
}


void Flag::or_equal(const Flag& other)
{
	const size_t min_size = MIN(this->size, other.size);
	for (size_t i = 0; i < min_size; i++) {
		this->data[i] |= other.data[i];
	}
}


vector<size_t> Flag::get_true_list() const
{
	vector<size_t> res = {};
	res.reserve(this->get_true_count());
	
	for (size_t i = 0; i < this->size; i++) {
		uint64_t item = this->data[i];
		for (size_t j = 0; j < 64 && item != 0; i++) {
			if ((item & (uint64_t)1) != 0) {
				res.push_back(i*64 + j);
			}
			item >>= 1;
		}
	}

	return res;
}



void Flag::alloc_data(const size_t n_items)
{
	size_t new_size = this->get_required_size(n_items);
	if (this->size == 0) {
		this->data = (uint64_t*)malloc(sizeof(uint64_t)*new_size);
	}
	else if (this->size < new_size) {
		this->data = (uint64_t*)realloc(this->data, new_size);
	}

	if (this->data == nullptr) {
		throw std::bad_alloc();
	}

	for (size_t i = this->size; i < new_size; i++) {
		this->data[i] = 0;
	}

	this->size = new_size;
}

