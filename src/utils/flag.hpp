#pragma once

#include <vector>
#include <bit>
#include <cstdint>

#define FLAG_MASK(n) ((uint64_t)1 << n)

class Flag
{
public:
	struct Iter
	{
		const Flag& flag;
		size_t idx = 0;
		size_t i = 0;
		size_t j = 0;

		Iter(const Flag& flag) : flag(flag), idx(0), i(0), j(0) { roll_to_next(); };

		inline size_t operator*() { return idx; };
		inline bool operator!=(size_t x) { return idx != x; }
		inline void operator++() { roll_to_next(); }

		void roll_to_next();
	};

	size_t n_items = 0;

	Flag(const size_t n_items=0);
	~Flag();

	void set_n_items(const size_t n);

	void fill(const bool value);
	inline void clear() { this->fill(false); }

	inline bool get(size_t idx) const { return (this->data[idx/64] & FLAG_MASK(idx % 64)) != 0; }
	inline void set_true(size_t idx) { this->data[idx/64] |= FLAG_MASK(idx % 64); }
	inline void set_false(size_t idx) { this->data[idx/64] &= ~FLAG_MASK(idx % 64); }

	inline bool operator[](size_t idx) const { return this->get(idx); }
	inline void operator+=(size_t idx) { this->set_true(idx); }
	inline void operator-=(size_t idx) { this->set_false(idx); }
	
	void set(const Flag& other);
	void set_true(const Flag& other);
	void set_false(const Flag& other);
	void mask(const Flag& other);

	Iter begin() const { return Iter(*this); }
	size_t end() const { return this->n_items; }

	inline bool empty() const { return this->get_true_count() == 0; } 
	size_t get_true_count() const;
	std::vector<size_t> get_true_list() const;

	template<typename T, bool check_count=true>
	void get_true_list(T& ret) const;

private:
	uint64_t* data = nullptr;
	size_t capacity = 0;
	
	void alloc_data(const size_t n_items);

	inline size_t req_size() const
	{ return Flag::get_required_size(this->n_items); };

	inline static size_t get_required_size(const size_t n)
	{ return (n == 0) ? 0 : (n - 1)/64 + 1;}
};


template<typename T, bool check_count>
void Flag::get_true_list(T& ret) const
{
	ret.clear();
	if constexpr (check_count) {
		size_t true_count = this->get_true_count();
		if (true_count == 0) {
			return;
		}
	}

	size_t size = this->req_size();
	for (size_t i = 0; i < size; i++) {
		uint64_t item = this->data[i];
		for (size_t j = 0; item != 0 && j < 64; j++) {
			if (item & (uint64_t)1) {
				ret.push_back(i*64 + j);
			}
			item >>= 1;
		}
	}
}

