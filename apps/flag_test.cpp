
#include <cstdlib>
#include <cassert>
#include <vector>
#include <chrono>

#include "utils/interval.hpp"
#include "utils/flag.hpp"
#include "iostream"

using namespace std;

bool same_vec(const vector<size_t>& a, const vector<size_t>& b)
{
	size_t size = a.size();
	if (size != b.size()) {
		return false;
	}

	for (size_t i = 0; i < size; i++) {
		if (a[i] != b[i]) {
			return false;
		}
	}
	
	return true;
}


int main()
{
	size_t I = 1000;
	size_t N = 100000;
	
	vector<Flag> flags(I);

	vector<size_t> a;
	vector<size_t> b;

	for (auto i : Range<>(I)) {
		cout << "iter " << i+1 << "/" << I << "\r" << flush;
		flags[i].set_n_items(N);

		for (auto n : Range<>(N)) {
			flags[i] += (rand() % N);
		}

		a = flags[i].get_true_list();
		b.clear();
		for (auto x : flags[i]) {
			b.push_back(x);
			assert(b.size() < N);
		}

		assert(same_vec(a, b));
	}

	cout << endl;

	using namespace chrono;

	size_t a_check = 0;
	auto t1 = high_resolution_clock::now();
	for (auto i : Range<>(I)) {
		for (auto x : flags[i].get_true_list()) {
			a_check ^= x;
		}
	}
	auto t2 = high_resolution_clock::now();

	size_t a_dur = duration_cast<milliseconds>(t2 - t1).count();

	size_t b_check = 0;
	t1 = high_resolution_clock::now();
	for (auto i : Range<>(I)) {
		for (auto x : flags[i]) {
			b_check ^= x;
		}
	}
	t2 = high_resolution_clock::now();

	size_t b_dur = duration_cast<milliseconds>(t2 - t1).count();

	cout << a_dur << " | " << b_dur << endl;

	assert(a_check == b_check);
}
