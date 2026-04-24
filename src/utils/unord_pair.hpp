#pragma once

#include "lex_comp.hpp"

template<typename T>
struct Unord_pair
{
	T first;
	T second;

	Unord_pair(const T& a, const T& b) 
		: first((a < b) ? a : b), second((a < b) ? b : a) {}
	
	Unord_pair() {}

	inline bool operator<(const Unord_pair& x) const
	{ return LEX_LT2(first, second, x.first, x.second); }

	inline bool operator==(const Unord_pair& x) const
	{ return LEX_EQ2(first, second, x.first, x.second); }

	inline bool operator<=(const Unord_pair& x) const
	{ return (*this == x) || (*this < x); }
	
};