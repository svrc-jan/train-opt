#pragma once

#define LEX_LT2(a1, a2, b1, b2) ((a1 < b1) || ((a1 == b1) && (a2 < b2)))
#define LEX_EQ2(a1, a2, b1, b2) ((a1 == b1) && (a2 == b2))

#define LEX_LT3(a1, a2, a3, b1, b2, b3) ((a1 < b1) || ((a1 == b1) && LEX_LT2(a2, a3, b2, b3)))
#define LEX_EQ3(a1, a2, a3, b1, b2, b3) ((a1 == b1) && (a2 == b2) && (a3 == b3))

#define LEX_LT4(a1, a2, a3, a4, b1, b2, b3, b4) ((a1 < b1) || ((a1 == b1) && LEX_LT3(a2, a3, a4, b2, b3, b4)))
#define LEX_EQ4(a1, a2, a3, a4, b1, b2, b3, b4) ((a1 == b1) && (a2 == b2) && (a3 == b3) && (a4 == b4))
