#pragma once

#include <limits>
#include <vector>
#include <cstdint>
#include "utils/array.hpp"

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
	inline size_t len(const size_t idx);

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
inline size_t Block_list<T, S>::len(const size_t idx)
{
	return this->blocks[idx / S].len(idx % S);
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
	inline size_t len(const size_t idx);

private:
	struct Range {
		uint32_t idx = 0;
		uint32_t size = 0;
	};

	std::vector<T> entries = {};
	Range ranges[S];
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
	size_t k = r.idx + r.size;
	
	if (i == S - 1) {
		if (k < this->entries.size()) {
			this->entries[k] = entry;
		}
		else {
			this->entries.push_back(entry);
		}
	}
	else {
		Range& r_next = this->ranges[i + 1];
		if (k < r_next.idx) {
			this->entries[k] = entry;
		}
		else {
			size_t shift = std::max(r.size, (uint32_t)1U);
			if (r.size == 1) {
				this->entries.insert(this->entries.begin() + k, entry);
			}
			else {
				this->entries.insert(this->entries.begin() + k, shift, T());
				this->entries[k] = entry;
			}

			#pragma GCC ivdep
			for (size_t j = i + 1; j < S; j++) {
				this->ranges[j].idx += shift;
			}
		}
	}

	r.size++;
}

template<typename T, size_t S>
inline Array<T> Block_list<T, S>::Block::get(const size_t i)
{
	const Range& r = this->ranges[i];
	return {this->entries.data() + r.idx, r.size};
}


template<typename T, size_t S>
inline size_t Block_list<T, S>::Block::len(const size_t i)
{
	return this->ranges[i].size;
}



