#pragma once

#include <algorithm>
#include <vector>

template<typename T>
struct Array
{
	T* ptr = nullptr;
	size_t size = 0;

	Array(void *ptr_=nullptr, size_t size_=0);
	~Array() {}

	inline void set_ptr(void *ptr_) { this->ptr = (T*)ptr_; }

	inline void push_back(const T& x) { this->ptr[this->size++] = x; }

	inline T* begin() { return this->ptr; }
	inline T* end() { return this->ptr + this->size; }
	inline T& back() { return this->ptr[this->size - 1]; }
	inline T& operator[](size_t idx) { return this->ptr[idx]; }

	inline const T* begin() const { return this->ptr; }
	inline const T* end() const { return this->ptr + this->size; }
	inline const T& back() const { return this->ptr[this->size - 1]; }
	inline const T& operator[](size_t idx) const { return this->ptr[idx]; }

	inline size_t n_bytes() const { return this->size*sizeof(T); }

	void sort() { std::sort(this->begin(), this->end()); }
	bool is_asc() const;

	template<typename X>
	ssize_t find(const X& x) const;

	template<typename X>
	ssize_t find_sorted(const X& x) const;

	void assign_ptr(const std::vector<T>& vec, size_t& idx);
	void assign_ptr(const Array<T>& arr, size_t& idx);
};

template<typename T>
Array<T>::Array(void *ptr_, size_t size_)
	: ptr((T*)ptr_), size(size_) {}


template<typename T>
bool Array<T>::is_asc() const
{
	for(size_t i = 1; i < this->size; i++) {
		if (this->ptr[i-1] > this->ptr[i]) {
			return false;
		}
	}

	return true;
}


template<typename T>
template<typename X>
ssize_t Array<T>::find(const X& x) const
{
	for (size_t i = 0; i < this->size; i++) {
		if (this->ptr[i] == x) {
			return i;
		}
	}

	return -1;
}


template<typename T>
template<typename X>
ssize_t Array<T>::find_sorted(const X& x) const
{
	if (this->size == 0) {
		return -1;
	}

	int l = 0;
	int r = this->size - 1;

	while (l <= r) {
		int m = l + (r - l)/2;

		if (this->ptr[m] == x) {
			return m;
		}

		if (this->ptr[m] < x) {
			l = m + 1;
		}
		else {
			r = m - 1;
		}
	}

	return -1;
}


template<typename T>
void Array<T>::assign_ptr(const std::vector<T>& vec, size_t& idx)
{
	if (this->size > 0) {
		this->ptr = (T*)vec.data() + idx;
		idx += this->size;
	}
}


template<typename T>
void Array<T>::assign_ptr(const Array<T>& arr, size_t& idx)
{
	if (this->size > 0) {
		this->ptr = arr.ptr + idx;
		idx += this->size;
	}
}
