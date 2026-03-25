#pragma once

#define METHOD_N(name) inline auto n_##name() const { return this->name.size(); }
#define METHOD_AFTER(name) inline auto name##_after() const { return this->name##_first + this->name##s.size(); }
#define METHOD_LAST(name) inline auto name##_last() const { return this->name##_first + this->name##s.size() - 1; }
