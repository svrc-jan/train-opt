#pragma once

#include <type_traits>
#include <limits>
#include <algorithm>
#include <vector>

template<typename I, typename C>
class Batch
{
public:
	static_assert(std::is_signed<C>::value);

	C CNT_MAX = std::numeric_limits<C>::max();
	C CNT_MIN = std::numeric_limits<C>::min();
	
	struct Item;

	void clear() { this->data.clear(); }
	void reserve(size_t n) { this->data.reserve(n); }

	inline void push_back(const Item& x);
	void aggregate();
	
	inline auto begin() { return this->data.begin(); }
	inline auto end() { return this->data.end(); }

	inline Batch<I, C>& operator<<(const Item& x) { this->push_back(x); return *this; }
	inline Batch<I, C>& operator+=(const I& x) { this->push_back({x, 1}); return *this; }
	inline Batch<I, C>& operator-=(const I& x) { this->push_back({x, -1}); return *this; }

private:
	std::vector<Item> data = {};
};

template<typename I, typename C>
struct Batch<I, C>::Item
{
	I idx;
	C count;

	Item(const I& idx, const C& count=0) : idx(idx), count(count) {}

	bool operator<(const Item& x) const { return idx < x.idx; }
	bool operator==(const Item& x) const { return (idx == x.idx) && (count == x.count); }
};

template<typename I, typename C>
void Batch<I, C>::push_back(const Item& x)
{
	if (x.count == 0) {
		return;
	}
	this->data.push_back(x);
}


template<typename I, typename C>
void Batch<I, C>::aggregate()
{
	std::sort(this->data.begin(), this->data.end());

	size_t n = this->data.size();
	size_t i = 0;
	
	for (size_t j = 1; j < n; j++) {
		if (this->data[i].idx == this->data[j].idx) {
			this->data[i].count += this->data[j].count;
			this->data[j].count = 0;
		}
		else {
			i = j;
		}
	}
	
	auto zero_count = [](const Item& x){ return (x.count == 0); };

	this->data.erase(std::remove_if(
		this->data.begin(), this->data.end(),
		zero_count), this->data.end());
}