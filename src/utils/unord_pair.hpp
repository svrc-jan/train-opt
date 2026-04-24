#pragma once

#include <utility>
#include <iostream>

#include "lex_comp.hpp"

template<typename T>
struct Unord_pair
{
	T first;
	T second;

	Unord_pair(const T& a, const T& b) 
	  : first((a < b) ? a : b), second((a < b) ? b : a) {}

	Unord_pair(const std::pair<T, T>& x) 
	  : first((x.first < x.second) ? x.first : x.second), 
		second((x.first < x.second) ? x.second : x.first) {}
	
	Unord_pair() {}

	inline bool operator<(const Unord_pair& x) const
	{ return LEX_LT2(first, second, x.first, x.second); }

	inline bool operator==(const Unord_pair& x) const
	{ return LEX_EQ2(first, second, x.first, x.second); }

	inline bool operator<=(const Unord_pair& x) const
	{ return (*this == x) || (*this < x); }
	
	void print(std::ostream& os) const;
};


template<typename T>
void Unord_pair<T>::print(std::ostream& os) const
{
	os << "{" << this->first << ", " << this->second << ")";
}


template<typename T>
std::ostream& operator<<(std::ostream& os, const Unord_pair<T>& x)
{
	x.print(os);
	return x;
}
