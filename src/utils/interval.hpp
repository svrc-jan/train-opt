#pragma once

#include <vector>

template<typename T>
struct Range;

template<typename T>
struct Interval
{	
	T start;
	T end;

	auto range() const { return Range<T>(start, end); }
	auto range_inc() const { return Range<T>(start, end + 1); }
};


template<typename T>
struct Range
{
	struct Iter;

	T _start;
	T _end;

	Iter begin() const { return Iter(_start); }
	Iter end() const { return Iter(_end); }
};


template<typename T>
struct Range<T>::Iter
{
	T state;

	inline const T& operator*() const { return state; }
	inline Iter& operator+=(const T& x) { state += x; return *this; }
	inline Iter& operator-=(const T& x) { state -= x; return *this; }

	inline Iter& operator++() { return *this += 1; }
	inline Iter& operator--() { return *this -= 1; }

	inline auto operator<=>(const Iter& other) const { return state <=> other.state; }
	inline bool operator!=(const Iter& other) const { return state != other.state; }
};




