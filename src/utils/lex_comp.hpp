#pragma once

#define LEX_LT2(ax, ay, bx, by) ((ax < bx) || ((ax == bx) && (ay < by)))
#define LEX_EQ2(ax, ay, bx, by) ((ax == bx) && (ay == by))

#define LEX_LT3(ax, ay, az, bx, by, bz) ((ax < bx) || ((ax == bx) && LEX_LT2(ay, az, by, bz)))
#define LEX_EQ3(ax, ay, az, bx, by, bz) ((ax == bx) && (ay == by) && (az == bz))
