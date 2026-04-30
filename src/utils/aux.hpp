#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <ranges>

template<typename T>
void make_unique(std::vector<T>& x)
{
	std::sort(x.begin(), x.end());
	x.erase(std::unique(x.begin(), x.end()), x.end());
}



template<typename K, typename V>
std::pair<K, V> get_max_entry(const std::map<K, V>& mp)
{
	assert(mp.size() > 0);

	std::pair<K, V> max_entry = *mp.begin();

	for (auto it = mp.begin(); it != mp.end(); it++) {
		if (it->second > max_entry.second) {
			max_entry = *it;
		}
	}

	return  max_entry;
}