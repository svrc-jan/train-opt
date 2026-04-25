template<typename T>
struct Tracked
{
	T curr;
	T old;
	
	Tracked() {}
	Tracked(const T& x) : curr(x), old(x) {}
	Tracked(const T& curr, const T& old) : curr(curr), old(old) {}

	inline T get_change() {}
	inline void snap() { old = curr; }
	inline T get_change() const { return curr - old; }
	inline bool changed() const { return curr != old; }

	inline operator T() const { return curr; }
	inline operator T&() { return curr; }

	inline T& operator=(const T& x) { curr = x; return curr; }
	inline auto operator<=>(const T& x) { return curr <=> x; }
	inline bool operator!=(const T& x) { return curr != x; }
};

