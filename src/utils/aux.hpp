#pragma once

#include <vector>
#include <algorithm>

template<typename T>
void make_unique(std::vector<T> x)
{
	std::sort(x.begin(), x.end());
	x.erase(std::unique(x.begin(), x.end()), x.end());
}
