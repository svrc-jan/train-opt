#pragma once

#include <vector>
#include <bit>
#include <cstdint>

#define FLAG_MASK(n) (1U << n)

class Flag
{
public:

	Flag(const size_t n_items=0);
	~Flag();

	void set_n_items(const size_t n_items);

	void fill(const bool value);
	inline void clear() { this->fill(false); }

	inline bool get(size_t idx) const { return (this->data[idx/64] & FLAG_MASK(idx % 64)) != 0; }
	inline void set_true(size_t idx) { this->data[idx/64] |= FLAG_MASK(idx % 64); }
	inline void set_false(size_t idx) { this->data[idx/64] &= ~FLAG_MASK(idx % 64); }

	inline bool operator[](size_t idx) { return this->get(idx); }
	inline void operator+=(size_t idx) { this->set_true(idx); }
	inline void operator-=(size_t idx) { this->set_false(idx); }
	
	void or_equal(const Flag& other);
	inline void operator|=(const Flag& other) { this->or_equal(other); }

	inline size_t get_true_count() const;
	std::vector<size_t> get_true_list() const;

private:
	uint64_t* data = nullptr;
	size_t size = 0;
	size_t n_items = 0;
	
	void alloc_data(const size_t n_items);
	inline static size_t get_required_size(const size_t n_items);
};


inline size_t Flag::get_true_count() const
{
	size_t count = 0;
	for (size_t i = 0; i < this->size; i++) {
		count += std::popcount(this->data[i]);
	}
	return count;
}



inline size_t Flag::get_required_size(const size_t n_items)
{
	return (n_items - 1)/64 + 1;
}
