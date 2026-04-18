#pragma once

#include "range.hpp"

template<typename T=size_t>
struct Interval
{	
	T start;
	T end;

	auto range() const { return Range<T>(start, end); }
	auto range_inc() const { return Range<T>(start, end + 1); }
};





