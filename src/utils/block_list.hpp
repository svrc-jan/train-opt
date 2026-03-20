#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <vector>
#include <algorithm>
#include "array.hpp"

// #define MAX(x) std::numeric_limits<x>::max()
#define MIN_BLOCK_CAP 64

template<typename T, size_t S=64>
class Block_list
{
public:
	Block_list();
	Block_list(const size_t num_lists);
	~Block_list();

	void set_num_lists(const size_t num_lists);
	void clear();
	inline void add(const size_t idx, const T& entry);
	inline Array<T> get(const size_t idx);

private:
	struct Block;

	std::vector<Block> blocks;
};


template<typename T, size_t S>
Block_list<T, S>::Block_list()
{

}


template<typename T, size_t S>
Block_list<T, S>::Block_list(const size_t num_lists)
{
	this->set_num_lists(num_lists);
}


template<typename T, size_t S>
Block_list<T, S>::~Block_list()
{
	
}


template<typename T, size_t S>
void Block_list<T, S>::set_num_lists(const size_t num_lists)
{
	const size_t num_blocks = (num_lists + S - 1)/S;
	this->blocks.resize(num_blocks, Block());
}

template<typename T, size_t S>
void Block_list<T, S>::clear()
{
	for (auto& block : this->blocks) {
		block.clear();
	}
}


template<typename T, size_t S>
inline void Block_list<T, S>::add(const size_t idx, const T& entry)
{
	this->blocks[idx / S].add(idx % S, entry);
}


template<typename T, size_t S>
inline Array<T> Block_list<T, S>::get(const size_t idx)
{
	return this->blocks[idx / S].get(idx % S);
}


template<typename T, size_t S>
class Block_list<T, S>::Block
{
public:
	Block();
	~Block();

	void clear();
	void add(const size_t idx, const T& entry);
	inline Array<T> get(const size_t idx);

private:
	struct Range {
		size_t idx = 0;
		size_t size = 0;
	};

	std::vector<T> entries = {};
	size_t capacity = 0;
	Range ranges[S];

	void shift(const size_t i_start, const size_t n);
	void increase_capacity();
};



template<typename T, size_t S>
Block_list<T, S>::Block::Block()
{
	for (size_t i = 0; i < S; i++) {
		this->ranges[i] = {0, 0};
	}
}


template<typename T, size_t S>
Block_list<T, S>::Block::~Block()
{
	// if (this->entries != nullptr) {
	// 	free(nullptr);
	// }
}


template<typename T, size_t S>
void Block_list<T, S>::Block::clear()
{
	for (size_t i = 0; i < S; i++) {
		this->ranges[i].size = 0;
	}
}


template<typename T, size_t S>
void Block_list<T, S>::Block::add(const size_t i, const T& entry)
{
	Range& r = this->ranges[i];
	Range& r_next = this->ranges[i + 1];

	// if (r.idx + r.size >= this->capacity) {
	// 	this->increase_capacity();
	// }
	
	// if (i < S - 1 && r.idx + r.size >= r_next.idx) {
	// 	this->shift(i + 1, std::max(r.size, (size_t)1));
	// }

	// this->entries[r.idx + r.size++] = entry;

	size_t k = r.idx + r.size;
	if ((i < S - 1 && k < r_next.idx) || (i == S && k < this->entries.size())) {
		this->entries[k] = entry;
	}
	else {
		this->entries.insert(this->entries.begin() + k, entry);
		for (size_t j = i + 1; j < S; j++) {
			this->ranges[j].idx++;
		}
	}
}

template<typename T, size_t S>
inline Array<T> Block_list<T, S>::Block::get(const size_t i)
{
	const Range& r = this->ranges[i];
	return {this->entries.data() + r.idx, r.size};
}


// template<typename T, size_t S>
// void Block_list<T, S>::Block::shift(const size_t i_start, const size_t n)
// {
// 	Range& r_start = this->ranges[i_start];
// 	Range& r_last = this->ranges[S - 1];

// 	if (r_last.idx + r_last.size + n >= this->capacity) {
// 		increase_capacity();	
// 	}

// 	for (size_t j = r_last.idx + r_last.size - 1; j >= r_start.idx; j++) {
// 		this->entries[j + n] = this->entries[j]; 
// 	}
	
// 	for (size_t i = i_start; i < S; i++) {
// 		this->ranges[i].idx += n;
// 	}
// }


// template<typename T, size_t S>
// void Block_list<T, S>::Block::increase_capacity()
// {
// 	if (this->entries == nullptr) {
// 		this->entries = (T*)malloc(sizeof(T)*MIN_BLOCK_CAP);
// 		this->capacity = MIN_BLOCK_CAP;
// 	}
// 	else {
// 		this->entries = (T*)realloc((void*)this->entries, 2*this->capacity);
// 		this->capacity = 2*this->capacity;
// 	}

// 	if (this->entries == nullptr) {
// 		throw std::bad_alloc();
// 	}
// }



