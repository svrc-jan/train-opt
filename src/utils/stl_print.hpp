#pragma once

#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec);

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& lst);

template<typename Ta, typename Tb>
std::ostream& operator<<(std::ostream& os, const std::pair<Ta, Tb>& pr);

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& st);


template<typename Tk, typename Tv>
std::ostream& operator<<(std::ostream& os, const std::map<Tk, Tv>& mp);

template<typename A, typename B, typename C>
std::ostream& operator<<(std::ostream& os, const std::tuple<A, B, C>& tpl);


template<typename T>
void print_vec(std::ostream& os, const std::vector<T>& vec)
{
	os << "[";
	for (size_t i = 0; i < vec.size(); i++) {
		os << vec[i] << (i + 1 < vec.size() ? ", " : "");

#if defined VEC_TRUNCATE_SIZE && VEC_TRUNCATE_SIZE > 0 
		if (i >= VEC_TRUNCATE_SIZE) {
			os << "...";
			break;
		}
#endif

	}
	os << "]";
}


template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec)
{
	print_vec(os, vec);
	return os;
}


template<typename T>
void print_list(std::ostream& os, const std::list<T>& lst)
{
	os << "[";
	int i = 0;
	for (auto it = lst.begin(); it != lst.end(); it++) {
		os << *it << (i + 1 < lst.size() ? ", " : "");

#if defined VEC_TRUNCATE_SIZE && VEC_TRUNCATE_SIZE > 0 
		if (i >= VEC_TRUNCATE_SIZE) {
			os << "...";
			break;
		}
#endif
		i++;
	}
	os << "]";
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& lst)
{
	print_list(os, lst);
	return os;
}


template<typename T>
void print_set(std::ostream& os, const std::set<T>& st)
{
	os << "{";
	size_t i = 0;
	for (auto it = st.begin(); it != st.end(); it++) {
		os << *it << (i + 1 < st.size() ? ", " : "");

#if defined VEC_TRUNCATE_SIZE && VEC_TRUNCATE_SIZE > 0 
		if (i >= VEC_TRUNCATE_SIZE) {
			os << "...";
			break;
		}
#endif
		i++;
	}
	os << "}";
}


template<typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& st)
{
	print_set(os, st);
	return os;
}


template<typename Tk, typename Tv>
void print_map(std::ostream& os, const std::map<Tk, Tv>& mp)
{
	os << "{";
	size_t i = 0;
	for (auto it = mp.begin(); it != mp.end(); it++) {
		os << it->first << ":" << it->second << (i + 1 < mp.size() ? ", " : "");

#if defined VEC_TRUNCATE_SIZE && VEC_TRUNCATE_SIZE > 0 
		if (i >= VEC_TRUNCATE_SIZE) {
			os << "...";
			break;
		}
#endif
		i++;
	}
	os << "}";
}




template<typename Tk, typename Tv>
std::ostream& operator<<(std::ostream& os, const std::map<Tk, Tv>& mp)
{
	print_map(os, mp);
	return os;
}


template<typename Tk, typename Tv>
void print_umap(std::ostream& os, const std::unordered_map<Tk, Tv>& mp)
{
	os << "{";
	size_t i = 0;
	for (auto it = mp.begin(); it != mp.end(); it++) {
		os << it->first << ":" << it->second << (i + 1 < mp.size() ? ", " : "");

#if defined VEC_TRUNCATE_SIZE && VEC_TRUNCATE_SIZE > 0 
		if (i >= VEC_TRUNCATE_SIZE) {
			os << "...";
			break;
		}
#endif
		i++;
	}
	os << "}";
}


template<typename Tk, typename Tv>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<Tk, Tv>& mp)
{
	print_umap(os, mp);
	return os;
}


template<typename Ta, typename Tb>
void print_pair(std::ostream& os, const std::pair<Ta, Tb>& pr)
{
	os << "(" << pr.first << ", " << pr.second << ")";
}


template<typename Ta, typename Tb>
std::ostream& operator<<(std::ostream& os, const std::pair<Ta, Tb>& pr)
{
	print_pair(os, pr);
	return os;
}


template<typename A, typename B, typename C>
void print_tuple(std::ostream& os, const std::tuple<A, B, C>& tpl)
{
	os << "(" << get<0>(tpl) << ", " << get<1>(tpl) << ", " << get<2>(tpl) << ")";
}

template<typename A, typename B, typename C>
std::ostream& operator<<(std::ostream& os, const std::tuple<A, B, C>& tpl)
{
	print_tuple(tpl);
	return os;
}