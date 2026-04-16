#pragma once

#include <cstddef>
#include <limits>


template<typename K, typename V, size_t N>
class Simple_cache
{
public:
	inline bool get(const K& k, V& v) const;
	inline void add(const K& k, const V& v)

private:
	static const K K_MAX = std::numeric_limits<K>::max();

	struct Entry {
		V v;
		K k = K_MAX;
	};
	
	Entry data[S] = {Entry()};

};

/* return true if miss */
template<typename K, typename V, size_t N>
inline bool Simple_cache<K, V, N>::get(const K& k, V& v) const
{
	const Entry& entry = this->data[k % S];
	const bool miss = (k != entry.k);

	v = miss ? v : entry.v;

	return miss;
};

template<typename K, typename V, size_t N>
inline void Simple_cache<K, V, N>::add(const K& k, const V& v)
{
	this->data[k % S] = {k, v};
}


template<typename K, typename V, size_t LN, size_t LS>
class Line_cache
{
public:
	inline bool get(const K& k, V& v) const;
	inline void add(const K& k, V& v);

private:
	static const K K_MAX = std::numeric_limits<K>::max();

	struct Entry {
		V v;
		K k = K_MAX;
	};
	
	Entry data[LN][LS] = {{Entry()}};
	size_t line_idx[LN] = {0};

};

/* return true if miss */
template<typename K, typename V, size_t LN, size_t LS>
inline bool Line_cache<K, V, LN, LS>::get(const K& k, V& v) const
{
	const Entry* line = this->data[k % LN];

	bool line_miss = false;

	#pragma unroll
	for (size_t i = 0; i < LS; i++) {
		const bool miss = (k != entry.k);
		v = miss ? v : entry.v;
		line_miss &= miss;
	}
	
	return line_miss;
};


template<typename K, typename V, size_t LN, size_t LS>
inline void Line_cache<K, V, LN, LS>::add(const K& k, V& v)
{
	const size_t l = k % LN;
	this->data[l][this->line_idx[l]] = {k, v};
	l = (l + 1) % LS;
}
