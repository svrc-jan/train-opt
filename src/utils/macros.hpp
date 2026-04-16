#pragma once

#define MAX(a, b) (a >= b) ? a : b
#define MIN(a, b) (a <= b) ? a : b

#define METHOD_N(name) inline auto n_##name() const { return this->name.size(); }
#define METHOD_AFTER(name) inline auto name##_after() const { return this->name##_first + this->name##s.size(); }
#define METHOD_LAST(name) inline auto name##_last() const { return this->name##_first + this->name##s.size() - 1; }

#define FOR_REVERSE(it, array) for(auto it = array.end() - 1; it >= array.begin(); it--)

