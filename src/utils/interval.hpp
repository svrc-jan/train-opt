#pragma once

#include "range.hpp"
#include "lex_comp.hpp"

template<typename T=size_t>
struct Interval
{	
	T start;
	T end;

	inline auto range() const { return Range<T>(start, end); }
	inline auto range_inc() const { return Range<T>(start, end + 1); }
	inline auto range_drop() const { return Range<T>(start + 1, end); }

	bool operator<(const Interval<T>& x) const { return LEX_LT2(start, end, x.start, x.end); }
	bool operator==(const Interval<T>& x) const { return LEX_EQ2(start, end, x.start, x.end); }
};





