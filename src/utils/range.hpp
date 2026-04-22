#pragma once

template<typename T=size_t>
struct Range
{
	struct Iter;

	T _start;
	T _end;

	Range(T start_, T end_) : _start(start_), _end(end_) {}
	Range(T end_) : _start(0), _end(end_) {}

	Iter begin() const { return Iter(_start); }
	T end() const { return _end; }
};


template<typename T>
struct Range<T>::Iter
{
	T state;

	inline T operator*() const { return state; }
	inline void operator+=(const T& x) { state += x; }
	inline void operator-=(const T& x) { state -= x; }

	inline void operator++() { *this += 1; }
	inline void operator--() { *this -= 1; }

	inline bool operator==(const Iter& other) const { return state == other.state; }
	inline bool operator!=(const Iter& other) const { return state < other.state; }
	inline bool operator<(const Iter& other) const { return state < other.state; }

	inline auto operator==(const T& other) const { return state == other; }
	inline bool operator!=(const T& other) const { return state < other; }
	inline bool operator<(const T& other) const { return state < other; }
};
