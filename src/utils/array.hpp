#pragma once

#include <algorithm>
#include <vector>
#include <iostream>

template<typename T>
class Array
{
private:
	T* _begin = nullptr;
	T* _end = nullptr;

public:
	Array(void* const begin=nullptr, size_t size=0);
	Array(void* const begin, void* const end);
	~Array() {}


	inline size_t size() const { return this->_end - this->_begin; }
	bool empty() const { return this->_end <= this->_begin; }

	inline T* begin() { return this->_begin; }
	inline T* end() { return this->_end; }
	inline T& back() { return this->_end[-1]; }
	inline T& operator[](const size_t idx) { return this->_begin[idx]; }

	inline const T* begin() const { return this->_begin; }
	inline const T* end() const { return this->_end; }
	inline const T& back() const { return this->_end[-1]; }
	inline const T& operator[](const size_t idx) const { return this->_begin[idx]; }

	inline void set_begin(void* const ptr, bool shift_end=true);
	Array<T>& operator=(void* const ptr) { this->set_begin(ptr); return *this; }

	inline void set_size(const size_t value) { this->_end = this->_begin + value; };
	inline void increment_size(const size_t value) { this->_end += value; }
	inline void clear() { this->_end = this->_begin; }
	
	inline void push_back(const T& x) { *(this->_end++) = x; }
	Array<T>& operator<<(const T& x) { this->push_back(x); return *this; }

	void copy_from(const Array<T>& other);
	inline void copy_to(Array<T>& other) const { other.copy_from(*this); }

	
	inline size_t n_bytes() const { return this->size()*sizeof(T); }

	void sort() { std::sort(this->begin(), this->end()); }
	bool is_asc() const;

	template<typename X>
	const T* find(const X& x) const;

	template<typename X>
	const T* find_asc(const X& x) const;

	template<typename C, typename I>
	void assign_offset(C& container, I& idx, const bool clear=false);

	void print(std::ostream& os) const;
};


template<typename T>
Array<T>::Array(void* const ptr, size_t size)
	: _begin((T*)ptr), _end((T*)ptr + size)
{

}


template<typename T>
Array<T>::Array(void* const begin, void* const end)
	: _begin((T*)begin), _end((T*)end)
{
	if (this->_begin != nullptr && this->_end == nullptr) {
		this->clear();
	}
}


template<typename T>
void Array<T>::set_begin(void* const ptr, bool shift_end)
{
	if (shift_end) {
		size_t curr_size = this->size();
		this->_end = (T*)ptr + curr_size;
	}
	this->_begin = (T*)ptr;
}


template<typename T>
void Array<T>::copy_from(const Array<T>& other)
{
	this->clear();
	for (auto& x : other) {
		this->push_back(x);
	}
}


template<typename T>
bool Array<T>::is_asc() const
{
	const T* it_prev = this->_begin;
	for(const T* it = this->_begin + 1; it < this->_end; it++) {
		if (*it < *it_prev) {
			return false;
		}
		it_prev = it;
	}

	return true;
}


template<typename T>
template<typename X>
const T* Array<T>::find(const X& x) const
{
	for(const T* it = this->_begin; it < this->_end; it++) {
		if (*it == x) {
			return it;
		}
	}

	return nullptr;
}


template<typename T>
template<typename X>
const T* Array<T>::find_asc(const X& x) const
{
	if (this->empty()) {
		return nullptr;
	}

	const T* l = this->_begin;
	const T* r = this->_end - 1;

	const T* ret = nullptr;

	do {
		const T* m = l + (r - l)/2;

		ret = (*m == x) ? m : ret;

		bool mid_greater = *m < x;
		l = (mid_greater) ? m + 1 : l;
		r = (mid_greater) ? r : m - 1;
	}
	while (ret == nullptr && l <= r);

	return ret;
}


template<typename T>
template<typename C, typename I>
void Array<T>::assign_offset(C& container, I& idx, const bool clear)
{
	if (!this->empty()) {
		this->set_begin(&container[idx]);
		idx += this->size();
		if (clear) {
			this->clear();
		}
	}
}


template<typename T>
void Array<T>::print(std::ostream& os) const
{
	bool is_first = true;
	for (const T& x : *this) {
		os << (is_first ? "[" : ", ");
		os << x;
		is_first = false;
	}
	os << (is_first ? "[ ]" : "]");
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const Array<T>& array)
{
	array.print(os);
	return os;
}
