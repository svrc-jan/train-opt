// #define USE_TIME_DOUBLE 

#ifdef USE_TIME_DOUBLE
	#define TIME_T double
	#define TIME_EPS 1e-6

	#define TIME_EQUAL(a, b) (abs(a - b) < TIME_EPS)
	#define TIME_GREATER(a, b) (a > b + TIME_EPS)
#else
	#define TIME_T int

	#define TIME_EQUAL(a, b) (a == b)
	#define TIME_GREATER(a, b) (a > b)
#endif
