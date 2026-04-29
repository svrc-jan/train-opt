#pragma once

#include <type_traits>
#include <limits>
#include <algorithm>
#include <vector>

template<typename I, typename C>
class Batch
{
public:
	// static_assert(std::is_signed<C>::value);

	C CNT_MAX = std::numeric_limits<C>::max();
	C CNT_MIN = std::numeric_limits<C>::min();
	
	struct Item;

	void clear() { this->data.clear(); }
	void reserve(size_t n) { this->data.reserve(n); }

	inline void push_back(const Item& x) { this->data.push_back(x); }
	void sort() { std::sort(this->data.begin(), this->data.end()); }
	void aggregate();
	
	inline auto begin() { return this->data.begin(); }
	inline auto end() { return this->data.end(); }

	inline auto begin() const { return this->data.cbegin(); }
	inline auto end() const { return this->data.cend(); }

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
	C value;

	Item(const I& idx, const C& value=0) : idx(idx), value(value) {}

	bool operator<(const Item& x) const { return idx < x.idx; }
	bool operator==(const Item& x) const { return (idx == x.idx) && (value == x.value); }
};


template<typename I, typename C>
void Batch<I, C>::aggregate()
{
	this->sort();

	size_t n = this->data.size();
	size_t i = 0;
	
	for (size_t j = 1; j < n; j++) {
		if (this->data[i].idx == this->data[j].idx) {
			this->data[i].value += this->data[j].value;
			this->data[j].value = 0;
		}
		else {
			i = j;
		}
	}
	
	auto zero_value = [](const Item& x){ return (x.value == 0); };

	this->data.erase(std::remove_if(
		this->data.begin(), this->data.end(),
		zero_value), this->data.end());
}