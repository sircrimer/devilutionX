/**
 * @file lighting.cpp
 *
 * Implementation of light and vision.
 */
#include "lighting.h"

#include "automap.h"
#include "diablo.h"
#include "engine/load_file.hpp"
#include "player.h"

namespace devilution {

LightStruct VisionList[MAXVISION];
int VisionCount;
int VisionId;
LightStruct Lights[MAXLIGHTS];
uint8_t ActiveLights[MAXLIGHTS];
int ActiveLightCount;
char LightsMax;
std::array<uint8_t, LIGHTSIZE> LightTables;
bool DisableLighting;
bool UpdateLighting;

/**
 * CrawlTable specifies X- and Y-coordinate deltas from a missile target coordinate.
 *
 * n=4
 *
 *    y
 *    ^
 *    |  1
 *    | 3#4
 *    |  2
 *    +-----> x
 *
 * n=16
 *
 *    y
 *    ^
 *    |  314
 *    | B7 8C
 *    | F # G
 *    | D9 AE
 *    |  526
 *    +-------> x
 */
const int8_t CrawlTable[2749] = {
	// clang-format off
	1, // Table 0, offset 0
	  0,   0,
	4, // Table 1, offset 3
	 0,    1,    0,  -1,   -1,  0,    1,  0,
	16, // Table 2, offset 12
	  0,   2,    0,  -2,   -1,  2,    1,  2,
	 -1,  -2,    1,  -2,   -1,  1,    1,  1,
	 -1,  -1,    1,  -1,   -2,  1,    2,  1,
	 -2,  -1,    2,  -1,   -2,  0,    2,  0,
	24, // Table 3, offset 45
	  0,   3,    0,  -3,   -1,  3,    1,  3,
	 -1,  -3,    1,  -3,   -2,  3,    2,  3,
	 -2,  -3,    2,  -3,   -2,  2,    2,  2,
	 -2,  -2,    2,  -2,   -3,  2,    3,  2,
	 -3,  -2,    3,  -2,   -3,  1,    3,  1,
	 -3,  -1,    3,  -1,   -3,  0,    3,  0,
	32, // Table 4, offset 94
	  0,   4,    0,  -4,   -1,  4,    1,  4,
	 -1,  -4,    1,  -4,   -2,  4,    2,  4,
	 -2,  -4,    2,  -4,   -3,  4,    3,  4,
	 -3,  -4,    3,  -4,   -3,  3,    3,  3,
	 -3,  -3,    3,  -3,   -4,  3,    4,  3,
	 -4,  -3,    4,  -3,   -4,  2,    4,  2,
	 -4,  -2,    4,  -2,   -4,  1,    4,  1,
	 -4,  -1,    4,  -1,   -4,  0,    4,  0,
	40, // Table 5, offset 159
	  0,   5,    0,  -5,   -1,  5,    1,  5,
	 -1,  -5,    1,  -5,   -2,  5,    2,  5,
	 -2,  -5,    2,  -5,   -3,  5,    3,  5,
	 -3,  -5,    3,  -5,   -4,  5,    4,  5,
	 -4,  -5,    4,  -5,   -4,  4,    4,  4,
	 -4,  -4,    4,  -4,   -5,  4,    5,  4,
	 -5,  -4,    5,  -4,   -5,  3,    5,  3,
	 -5,  -3,    5,  -3,   -5,  2,    5,  2,
	 -5,  -2,    5,  -2,   -5,  1,    5,  1,
	 -5,  -1,    5,  -1,   -5,  0,    5,  0,
	48, // Table 6, offset 240
	  0,   6,    0,  -6,   -1,  6,    1,  6,
	 -1,  -6,    1,  -6,   -2,  6,    2,  6,
	 -2,  -6,    2,  -6,   -3,  6,    3,  6,
	 -3,  -6,    3,  -6,   -4,  6,    4,  6,
	 -4,  -6,    4,  -6,   -5,  6,    5,  6,
	 -5,  -6,    5,  -6,   -5,  5,    5,  5,
	 -5,  -5,    5,  -5,   -6,  5,    6,  5,
	 -6,  -5,    6,  -5,   -6,  4,    6,  4,
	 -6,  -4,    6,  -4,   -6,  3,    6,  3,
	 -6,  -3,    6,  -3,   -6,  2,    6,  2,
	 -6,  -2,    6,  -2,   -6,  1,    6,  1,
	 -6,  -1,    6,  -1,   -6,  0,    6,  0,
	56, // Table 7, offset 337
	  0,   7,    0,  -7,   -1,  7,    1,  7,
	 -1,  -7,    1,  -7,   -2,  7,    2,  7,
	 -2,  -7,    2,  -7,   -3,  7,    3,  7,
	 -3,  -7,    3,  -7,   -4,  7,    4,  7,
	 -4,  -7,    4,  -7,   -5,  7,    5,  7,
	 -5,  -7,    5,  -7,   -6,  7,    6,  7,
	 -6,  -7,    6,  -7,   -6,  6,    6,  6,
	 -6,  -6,    6,  -6,   -7,  6,    7,  6,
	 -7,  -6,    7,  -6,   -7,  5,    7,  5,
	 -7,  -5,    7,  -5,   -7,  4,    7,  4,
	 -7,  -4,    7,  -4,   -7,  3,    7,  3,
	 -7,  -3,    7,  -3,   -7,  2,    7,  2,
	 -7,  -2,    7,  -2,   -7,  1,    7,  1,
	 -7,  -1,    7,  -1,   -7,  0,    7,  0,
	64, // Table 8, offset 450
	  0,   8,    0,  -8,   -1,  8,    1,  8,
	 -1,  -8,    1,  -8,   -2,  8,    2,  8,
	 -2,  -8,    2,  -8,   -3,  8,    3,  8,
	 -3,  -8,    3,  -8,   -4,  8,    4,  8,
	 -4,  -8,    4,  -8,   -5,  8,    5,  8,
	 -5,  -8,    5,  -8,   -6,  8,    6,  8,
	 -6,  -8,    6,  -8,   -7,  8,    7,  8,
	 -7,  -8,    7,  -8,   -7,  7,    7,  7,
	 -7,  -7,    7,  -7,   -8,  7,    8,  7,
	 -8,  -7,    8,  -7,   -8,  6,    8,  6,
	 -8,  -6,    8,  -6,   -8,  5,    8,  5,
	 -8,  -5,    8,  -5,   -8,  4,    8,  4,
	 -8,  -4,    8,  -4,   -8,  3,    8,  3,
	 -8,  -3,    8,  -3,   -8,  2,    8,  2,
	 -8,  -2,    8,  -2,   -8,  1,    8,  1,
	 -8,  -1,    8,  -1,   -8,  0,    8,  0,
	72, // Table 9, offset 579
	  0,   9,    0,  -9,   -1,  9,    1,  9,
	 -1,  -9,    1,  -9,   -2,  9,    2,  9,
	 -2,  -9,    2,  -9,   -3,  9,    3,  9,
	 -3,  -9,    3,  -9,   -4,  9,    4,  9,
	 -4,  -9,    4,  -9,   -5,  9,    5,  9,
	 -5,  -9,    5,  -9,   -6,  9,    6,  9,
	 -6,  -9,    6,  -9,   -7,  9,    7,  9,
	 -7,  -9,    7,  -9,   -8,  9,    8,  9,
	 -8,  -9,    8,  -9,   -8,  8,    8,  8,
	 -8,  -8,    8,  -8,   -9,  8,    9,  8,
	 -9,  -8,    9,  -8,   -9,  7,    9,  7,
	 -9,  -7,    9,  -7,   -9,  6,    9,  6,
	 -9,  -6,    9,  -6,   -9,  5,    9,  5,
	 -9,  -5,    9,  -5,   -9,  4,    9,  4,
	 -9,  -4,    9,  -4,   -9,  3,    9,  3,
	 -9,  -3,    9,  -3,   -9,  2,    9,  2,
	 -9,  -2,    9,  -2,   -9,  1,    9,  1,
	 -9,  -1,    9,  -1,   -9,  0,    9,  0,
	80, // Table 10, offset 724
	  0,  10,    0, -10,   -1, 10,    1, 10,
	 -1, -10,    1, -10,   -2, 10,    2, 10,
	 -2, -10,    2, -10,   -3, 10,    3, 10,
	 -3, -10,    3, -10,   -4, 10,    4, 10,
	 -4, -10,    4, -10,   -5, 10,    5, 10,
	 -5, -10,    5, -10,   -6, 10,    6, 10,
	 -6, -10,    6, -10,   -7, 10,    7, 10,
	 -7, -10,    7, -10,   -8, 10,    8, 10,
	 -8, -10,    8, -10,   -9, 10,    9, 10,
	 -9, -10,    9, -10,   -9,  9,    9,  9,
	 -9,  -9,    9,  -9,  -10,  9,   10,  9,
	-10,  -9,   10,  -9,  -10,  8,   10,  8,
	-10,  -8,   10,  -8,  -10,  7,   10,  7,
	-10,  -7,   10,  -7,  -10,  6,   10,  6,
	-10,  -6,   10,  -6,  -10,  5,   10,  5,
	-10,  -5,   10,  -5,  -10,  4,   10,  4,
	-10,  -4,   10,  -4,  -10,  3,   10,  3,
	-10,  -3,   10,  -3,  -10,  2,   10,  2,
	-10,  -2,   10,  -2,  -10,  1,   10,  1,
	-10,  -1,   10,  -1,  -10,  0,   10,  0,
	88, // Table 11, offset 885
	  0,  11,    0, -11,   -1, 11,    1, 11,
	 -1, -11,    1, -11,   -2, 11,    2, 11,
	 -2, -11,    2, -11,   -3, 11,    3, 11,
	 -3, -11,    3, -11,   -4, 11,    4, 11,
	 -4, -11,    4, -11,   -5, 11,    5, 11,
	 -5, -11,    5, -11,   -6, 11,    6, 11,
	 -6, -11,    6, -11,   -7, 11,    7, 11,
	 -7, -11,    7, -11,   -8, 11,    8, 11,
	 -8, -11,    8, -11,   -9, 11,    9, 11,
	 -9, -11,    9, -11,  -10, 11,   10, 11,
	-10, -11,   10, -11,  -10, 10,   10, 10,
	-10, -10,   10, -10,  -11, 10,   11, 10,
	-11, -10,   11, -10,  -11,  9,   11,  9,
	-11,  -9,   11,  -9,  -11,  8,   11,  8,
	-11,  -8,   11,  -8,  -11,  7,   11,  7,
	-11,  -7,   11,  -7,  -11,  6,   11,  6,
	-11,  -6,   11,  -6,  -11,  5,   11,  5,
	-11,  -5,   11,  -5,  -11,  4,   11,  4,
	-11,  -4,   11,  -4,  -11,  3,   11,  3,
	-11,  -3,   11,  -3,  -11,  2,   11,  2,
	-11,  -2,   11,  -2,  -11,  1,   11,  1,
	-11,  -1,   11,  -1,  -11,  0,   11,  0,
	96, // Table 12, offset 1062
	  0,  12,    0, -12,   -1, 12,    1, 12,
	 -1, -12,    1, -12,   -2, 12,    2, 12,
	 -2, -12,    2, -12,   -3, 12,    3, 12,
	 -3, -12,    3, -12,   -4, 12,    4, 12,
	 -4, -12,    4, -12,   -5, 12,    5, 12,
	 -5, -12,    5, -12,   -6, 12,    6, 12,
	 -6, -12,    6, -12,   -7, 12,    7, 12,
	 -7, -12,    7, -12,   -8, 12,    8, 12,
	 -8, -12,    8, -12,   -9, 12,    9, 12,
	 -9, -12,    9, -12,  -10, 12,   10, 12,
	-10, -12,   10, -12,  -11, 12,   11, 12,
	-11, -12,   11, -12,  -11, 11,   11, 11,
	-11, -11,   11, -11,  -12, 11,   12, 11,
	-12, -11,   12, -11,  -12, 10,   12, 10,
	-12, -10,   12, -10,  -12,  9,   12,  9,
	-12,  -9,   12,  -9,  -12,  8,   12,  8,
	-12,  -8,   12,  -8,  -12,  7,   12,  7,
	-12,  -7,   12,  -7,  -12,  6,   12,  6,
	-12,  -6,   12,  -6,  -12,  5,   12,  5,
	-12,  -5,   12,  -5,  -12,  4,   12,  4,
	-12,  -4,   12,  -4,  -12,  3,   12,  3,
	-12,  -3,   12,  -3,  -12,  2,   12,  2,
	-12,  -2,   12,  -2,  -12,  1,   12,  1,
	-12,  -1,   12,  -1,  -12,  0,   12,  0,
	104, // Table 13, offset 1255
	  0,  13,    0, -13,   -1, 13,    1, 13,
	 -1, -13,    1, -13,   -2, 13,    2, 13,
	 -2, -13,    2, -13,   -3, 13,    3, 13,
	 -3, -13,    3, -13,   -4, 13,    4, 13,
	 -4, -13,    4, -13,   -5, 13,    5, 13,
	 -5, -13,    5, -13,   -6, 13,    6, 13,
	 -6, -13,    6, -13,   -7, 13,    7, 13,
	 -7, -13,    7, -13,   -8, 13,    8, 13,
	 -8, -13,    8, -13,   -9, 13,    9, 13,
	 -9, -13,    9, -13,  -10, 13,   10, 13,
	-10, -13,   10, -13,  -11, 13,   11, 13,
	-11, -13,   11, -13,  -12, 13,   12, 13,
	-12, -13,   12, -13,  -12, 12,   12, 12,
	-12, -12,   12, -12,  -13, 12,   13, 12,
	-13, -12,   13, -12,  -13, 11,   13, 11,
	-13, -11,   13, -11,  -13, 10,   13, 10,
	-13, -10,   13, -10,  -13,  9,   13,  9,
	-13,  -9,   13,  -9,  -13,  8,   13,  8,
	-13,  -8,   13,  -8,  -13,  7,   13,  7,
	-13,  -7,   13,  -7,  -13,  6,   13,  6,
	-13,  -6,   13,  -6,  -13,  5,   13,  5,
	-13,  -5,   13,  -5,  -13,  4,   13,  4,
	-13,  -4,   13,  -4,  -13,  3,   13,  3,
	-13,  -3,   13,  -3,  -13,  2,   13,  2,
	-13,  -2,   13,  -2,  -13,  1,   13,  1,
	-13,  -1,   13,  -1,  -13,  0,   13,  0,
	112, // Table 14, offset 1464
	  0,  14,    0, -14,   -1, 14,    1, 14,
	 -1, -14,    1, -14,   -2, 14,    2, 14,
	 -2, -14,    2, -14,   -3, 14,    3, 14,
	 -3, -14,    3, -14,   -4, 14,    4, 14,
	 -4, -14,    4, -14,   -5, 14,    5, 14,
	 -5, -14,    5, -14,   -6, 14,    6, 14,
	 -6, -14,    6, -14,   -7, 14,    7, 14,
	 -7, -14,    7, -14,   -8, 14,    8, 14,
	 -8, -14,    8, -14,   -9, 14,    9, 14,
	 -9, -14,    9, -14,  -10, 14,   10, 14,
	-10, -14,   10, -14,  -11, 14,   11, 14,
	-11, -14,   11, -14,  -12, 14,   12, 14,
	-12, -14,   12, -14,  -13, 14,   13, 14,
	-13, -14,   13, -14,  -13, 13,   13, 13,
	-13, -13,   13, -13,  -14, 13,   14, 13,
	-14, -13,   14, -13,  -14, 12,   14, 12,
	-14, -12,   14, -12,  -14, 11,   14, 11,
	-14, -11,   14, -11,  -14, 10,   14, 10,
	-14, -10,   14, -10,  -14,  9,   14,  9,
	-14,  -9,   14,  -9,  -14,  8,   14,  8,
	-14,  -8,   14,  -8,  -14,  7,   14,  7,
	-14,  -7,   14,  -7,  -14,  6,   14,  6,
	-14,  -6,   14,  -6,  -14,  5,   14,  5,
	-14,  -5,   14,  -5,  -14,  4,   14,  4,
	-14,  -4,   14,  -4,  -14,  3,   14,  3,
	-14,  -3,   14,  -3,  -14,  2,   14,  2,
	-14,  -2,   14,  -2,  -14,  1,   14,  1,
	-14,  -1,   14,  -1,  -14,  0,   14,  0,
	120, // Table 15, offset 1689
	  0,  15,    0, -15,   -1, 15,    1, 15,
	 -1, -15,    1, -15,   -2, 15,    2, 15,
	 -2, -15,    2, -15,   -3, 15,    3, 15,
	 -3, -15,    3, -15,   -4, 15,    4, 15,
	 -4, -15,    4, -15,   -5, 15,    5, 15,
	 -5, -15,    5, -15,   -6, 15,    6, 15,
	 -6, -15,    6, -15,   -7, 15,    7, 15,
	 -7, -15,    7, -15,   -8, 15,    8, 15,
	 -8, -15,    8, -15,   -9, 15,    9, 15,
	 -9, -15,    9, -15,  -10, 15,   10, 15,
	-10, -15,   10, -15,  -11, 15,   11, 15,
	-11, -15,   11, -15,  -12, 15,   12, 15,
	-12, -15,   12, -15,  -13, 15,   13, 15,
	-13, -15,   13, -15,  -14, 15,   14, 15,
	-14, -15,   14, -15,  -14, 14,   14, 14,
	-14, -14,   14, -14,  -15, 14,   15, 14,
	-15, -14,   15, -14,  -15, 13,   15, 13,
	-15, -13,   15, -13,  -15, 12,   15, 12,
	-15, -12,   15, -12,  -15, 11,   15, 11,
	-15, -11,   15, -11,  -15, 10,   15, 10,
	-15, -10,   15, -10,  -15,  9,   15,  9,
	-15,  -9,   15,  -9,  -15,  8,   15,  8,
	-15,  -8,   15,  -8,  -15,  7,   15,  7,
	-15,  -7,   15,  -7,  -15,  6,   15,  6,
	-15,  -6,   15,  -6,  -15,  5,   15,  5,
	-15,  -5,   15,  -5,  -15,  4,   15,  4,
	-15,  -4,   15,  -4,  -15,  3,   15,  3,
	-15,  -3,   15,  -3,  -15,  2,   15,  2,
	-15,  -2,   15,  -2,  -15,  1,   15,  1,
	-15,  -1,   15,  -1,  -15,  0,   15,  0,
	(char)128, // Table 16, offset 1930
	  0,  16,    0, -16,   -1, 16,    1, 16,
	 -1, -16,    1, -16,   -2, 16,    2, 16,
	 -2, -16,    2, -16,   -3, 16,    3, 16,
	 -3, -16,    3, -16,   -4, 16,    4, 16,
	 -4, -16,    4, -16,   -5, 16,    5, 16,
	 -5, -16,    5, -16,   -6, 16,    6, 16,
	 -6, -16,    6, -16,   -7, 16,    7, 16,
	 -7, -16,    7, -16,   -8, 16,    8, 16,
	 -8, -16,    8, -16,   -9, 16,    9, 16,
	 -9, -16,    9, -16,  -10, 16,   10, 16,
	-10, -16,   10, -16,  -11, 16,   11, 16,
	-11, -16,   11, -16,  -12, 16,   12, 16,
	-12, -16,   12, -16,  -13, 16,   13, 16,
	-13, -16,   13, -16,  -14, 16,   14, 16,
	-14, -16,   14, -16,  -15, 16,   15, 16,
	-15, -16,   15, -16,  -15, 15,   15, 15,
	-15, -15,   15, -15,  -16, 15,   16, 15,
	-16, -15,   16, -15,  -16, 14,   16, 14,
	-16, -14,   16, -14,  -16, 13,   16, 13,
	-16, -13,   16, -13,  -16, 12,   16, 12,
	-16, -12,   16, -12,  -16, 11,   16, 11,
	-16, -11,   16, -11,  -16, 10,   16, 10,
	-16, -10,   16, -10,  -16,  9,   16,  9,
	-16,  -9,   16,  -9,  -16,  8,   16,  8,
	-16,  -8,   16,  -8,  -16,  7,   16,  7,
	-16,  -7,   16,  -7,  -16,  6,   16,  6,
	-16,  -6,   16,  -6,  -16,  5,   16,  5,
	-16,  -5,   16,  -5,  -16,  4,   16,  4,
	-16,  -4,   16,  -4,  -16,  3,   16,  3,
	-16,  -3,   16,  -3,  -16,  2,   16,  2,
	-16,  -2,   16,  -2,  -16,  1,   16,  1,
	-16,  -1,   16,  -1,  -16,  0,   16,  0,
	(char)136, // Table 16, offset 2187
	  0,  17,    0, -17,   -1, 17,    1, 17,
	 -1, -17,    1, -17,   -2, 17,    2, 17,
	 -2, -17,    2, -17,   -3, 17,    3, 17,
	 -3, -17,    3, -17,   -4, 17,    4, 17,
	 -4, -17,    4, -17,   -5, 17,    5, 17,
	 -5, -17,    5, -17,   -6, 17,    6, 17,
	 -6, -17,    6, -17,   -7, 17,    7, 17,
	 -7, -17,    7, -17,   -8, 17,    8, 17,
	 -8, -17,    8, -17,   -9, 17,    9, 17,
	 -9, -17,    9, -17,  -10, 17,   10, 17,
	-10, -17,   10, -17,  -11, 17,   11, 17,
	-11, -17,   11, -17,  -12, 17,   12, 17,
	-12, -17,   12, -17,  -13, 17,   13, 17,
	-13, -17,   13, -17,  -14, 17,   14, 17,
	-14, -17,   14, -17,  -15, 17,   15, 17,
	-15, -17,   15, -17,  -16, 17,   16, 17,
	-16, -17,   16, -17,  -16, 16,   16, 16,
	-16, -16,   16, -16,  -17, 16,   17, 16,
	-17, -16,   17, -16,  -17, 15,   17, 15,
	-17, -15,   17, -15,  -17, 14,   17, 14,
	-17, -14,   17, -14,  -17, 13,   17, 13,
	-17, -13,   17, -13,  -17, 12,   17, 12,
	-17, -12,   17, -12,  -17, 11,   17, 11,
	-17, -11,   17, -11,  -17, 10,   17, 10,
	-17, -10,   17, -10,  -17,  9,   17,  9,
	-17,  -9,   17,  -9,  -17,  8,   17,  8,
	-17,  -8,   17,  -8,  -17,  7,   17,  7,
	-17,  -7,   17,  -7,  -17,  6,   17,  6,
	-17,  -6,   17,  -6,  -17,  5,   17,  5,
	-17,  -5,   17,  -5,  -17,  4,   17,  4,
	-17,  -4,   17,  -4,  -17,  3,   17,  3,
	-17,  -3,   17,  -3,  -17,  2,   17,  2,
	-17,  -2,   17,  -2,  -17,  1,   17,  1,
	-17,  -1,   17,  -1,  -17,  0,   17,  0,
	(char)144, // Table 16, offset 2460
	  0,  18,    0, -18,   -1, 18,    1, 18,
	 -1, -18,    1, -18,   -2, 18,    2, 18,
	 -2, -18,    2, -18,   -3, 18,    3, 18,
	 -3, -18,    3, -18,   -4, 18,    4, 18,
	 -4, -18,    4, -18,   -5, 18,    5, 18,
	 -5, -18,    5, -18,   -6, 18,    6, 18,
	 -6, -18,    6, -18,   -7, 18,    7, 18,
	 -7, -18,    7, -18,   -8, 18,    8, 18,
	 -8, -18,    8, -18,   -9, 18,    9, 18,
	 -9, -18,    9, -18,  -10, 18,   10, 18,
	-10, -18,   10, -18,  -11, 18,   11, 18,
	-11, -18,   11, -18,  -12, 18,   12, 18,
	-12, -18,   12, -18,  -13, 18,   13, 18,
	-13, -18,   13, -18,  -14, 18,   14, 18,
	-14, -18,   14, -18,  -15, 18,   15, 18,
	-15, -18,   15, -18,  -16, 18,   16, 18,
	-16, -18,   16, -18,  -17, 18,   17, 18,
	-17, -18,   17, -18,  -17, 17,   17, 17,
	-17, -17,   17, -17,  -18, 17,   18, 17,
	-18, -17,   18, -17,  -18, 16,   18, 16,
	-18, -16,   18, -16,  -18, 15,   18, 15,
	-18, -15,   18, -15,  -18, 14,   18, 14,
	-18, -14,   18, -14,  -18, 13,   18, 13,
	-18, -13,   18, -13,  -18, 12,   18, 12,
	-18, -12,   18, -12,  -18, 11,   18, 11,
	-18, -11,   18, -11,  -18, 10,   18, 10,
	-18, -10,   18, -10,  -18,  9,   18,  9,
	-18,  -9,   18,  -9,  -18,  8,   18,  8,
	-18,  -8,   18,  -8,  -18,  7,   18,  7,
	-18,  -7,   18,  -7,  -18,  6,   18,  6,
	-18,  -6,   18,  -6,  -18,  5,   18,  5,
	-18,  -5,   18,  -5,  -18,  4,   18,  4,
	-18,  -4,   18,  -4,  -18,  3,   18,  3,
	-18,  -3,   18,  -3,  -18,  2,   18,  2,
	-18,  -2,   18,  -2,  -18,  1,   18,  1,
	-18,  -1,   18,  -1,  -18,  0,   18,  0,
	// clang-format on
};

/*
 * X- Y-coordinate offsets of lighting visions.
 * The last entry-pair is only for alignment.
 */
const uint8_t VisionCrawlTable[23][30] = {
	// clang-format off
	{ 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0, 9, 0, 10,  0, 11,  0, 12,  0, 13,  0, 14,  0, 15,  0 },
	{ 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 1, 9, 1, 10,  1, 11,  1, 12,  1, 13,  1, 14,  1, 15,  1 },
	{ 1, 0, 2, 0, 3, 0, 4, 1, 5, 1, 6, 1, 7, 1, 8, 1, 9, 1, 10,  1, 11,  1, 12,  2, 13,  2, 14,  2, 15,  2 },
	{ 1, 0, 2, 0, 3, 1, 4, 1, 5, 1, 6, 1, 7, 1, 8, 2, 9, 2, 10,  2, 11,  2, 12,  2, 13,  3, 14,  3, 15,  3 },
	{ 1, 0, 2, 1, 3, 1, 4, 1, 5, 1, 6, 2, 7, 2, 8, 2, 9, 3, 10,  3, 11,  3, 12,  3, 13,  4, 14,  4,  0,  0 },
	{ 1, 0, 2, 1, 3, 1, 4, 1, 5, 2, 6, 2, 7, 3, 8, 3, 9, 3, 10,  4, 11,  4, 12,  4, 13,  5, 14,  5,  0,  0 },
	{ 1, 0, 2, 1, 3, 1, 4, 2, 5, 2, 6, 3, 7, 3, 8, 3, 9, 4, 10,  4, 11,  5, 12,  5, 13,  6, 14,  6,  0,  0 },
	{ 1, 1, 2, 1, 3, 2, 4, 2, 5, 3, 6, 3, 7, 4, 8, 4, 9, 5, 10,  5, 11,  6, 12,  6, 13,  7,  0,  0,  0,  0 },
	{ 1, 1, 2, 1, 3, 2, 4, 2, 5, 3, 6, 4, 7, 4, 8, 5, 9, 6, 10,  6, 11,  7, 12,  7, 12,  8, 13,  8,  0,  0 },
	{ 1, 1, 2, 2, 3, 2, 4, 3, 5, 4, 6, 5, 7, 5, 8, 6, 9, 7, 10,  7, 10,  8, 11,  8, 12,  9,  0,  0,  0,  0 },
	{ 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 5, 7, 6, 8, 7, 9, 8, 10,  9, 11,  9, 11, 10,  0,  0,  0,  0,  0,  0 },
	{ 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9,  9, 10,  9, 11, 10, 11,  0,  0,  0,  0,  0,  0 },
	{ 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 5, 7, 6, 8, 7, 9,  7, 10,  8, 10,  8, 11,  9, 12,  0,  0,  0,  0 },
	{ 1, 1, 1, 2, 2, 3, 2, 4, 3, 5, 4, 6, 4, 7, 5, 8, 6, 9,  6, 10,  7, 11,  7, 12,  8, 12,  8, 13,  0,  0 },
	{ 1, 1, 1, 2, 2, 3, 2, 4, 3, 5, 3, 6, 4, 7, 4, 8, 5, 9,  5, 10,  6, 11,  6, 12,  7, 13,  0,  0,  0,  0 },
	{ 0, 1, 1, 2, 1, 3, 2, 4, 2, 5, 3, 6, 3, 7, 3, 8, 4, 9,  4, 10,  5, 11,  5, 12,  6, 13,  6, 14,  0,  0 },
	{ 0, 1, 1, 2, 1, 3, 1, 4, 2, 5, 2, 6, 3, 7, 3, 8, 3, 9,  4, 10,  4, 11,  4, 12,  5, 13,  5, 14,  0,  0 },
	{ 0, 1, 1, 2, 1, 3, 1, 4, 1, 5, 2, 6, 2, 7, 2, 8, 3, 9,  3, 10,  3, 11,  3, 12,  4, 13,  4, 14,  0,  0 },
	{ 0, 1, 0, 2, 1, 3, 1, 4, 1, 5, 1, 6, 1, 7, 2, 8, 2, 9,  2, 10,  2, 11,  2, 12,  3, 13,  3, 14,  3, 15 },
	{ 0, 1, 0, 2, 0, 3, 1, 4, 1, 5, 1, 6, 1, 7, 1, 8, 1, 9,  1, 10,  1, 11,  2, 12,  2, 13,  2, 14,  2, 15 },
	{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 1, 8, 1, 9,  1, 10,  1, 11,  1, 12,  1, 13,  1, 14,  1, 15 },
	{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0, 9,  0, 10,  0, 11,  0, 12,  0, 13,  0, 14,  0, 15 },
	// clang-format on
};

namespace {

uint8_t lightradius[16][128];
bool dovision;
uint8_t lightblock[64][16][16];

/** RadiusAdj maps from VisionCrawlTable index to lighting vision radius adjustment. */
const BYTE RadiusAdj[23] = { 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 4, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0 };

void RotateRadius(int *x, int *y, int *dx, int *dy, int *lx, int *ly, int *bx, int *by)
{
	*bx = 0;
	*by = 0;

	int swap = *dx;
	*dx = 7 - *dy;
	*dy = swap;
	swap = *lx;
	*lx = 7 - *ly;
	*ly = swap;

	*x = *dx - *lx;
	*y = *dy - *ly;

	if (*x < 0) {
		*x += 8;
		*bx = 1;
	}
	if (*y < 0) {
		*y += 8;
		*by = 1;
	}
}

void SetLight(Point position, char v)
{
	if (LoadingMapObjects)
		dPreLight[position.x][position.y] = v;
	else
		dLight[position.x][position.y] = v;
}

char GetLight(Point position)
{
	if (LoadingMapObjects)
		return dPreLight[position.x][position.y];

	return dLight[position.x][position.y];
}

void DoUnLight(int nXPos, int nYPos, int nRadius)
{
	nRadius++;

	int minX = nXPos - nRadius;
	int maxX = nXPos + nRadius;
	int minY = nYPos - nRadius;
	int maxY = nYPos + nRadius;

	minX = std::max(minX, 0);
	maxX = std::max(maxX, MAXDUNX);
	minY = std::max(minY, 0);
	maxY = std::max(maxY, MAXDUNY);

	for (int y = minY; y < maxY; y++) {
		for (int x = minX; x < maxX; x++) {
			if (x >= 0 && x < MAXDUNX && y >= 0 && y < MAXDUNY)
				dLight[x][y] = dPreLight[x][y];
		}
	}
}

} // namespace

void DoLighting(Point position, int nRadius, int lnum)
{
	int xoff = 0;
	int yoff = 0;
	int lightX = 0;
	int lightY = 0;
	int blockX = 0;
	int blockY = 0;

	if (lnum >= 0) {
		xoff = Lights[lnum].position.offset.x;
		yoff = Lights[lnum].position.offset.y;
		if (xoff < 0) {
			xoff += 8;
			position -= { 1, 0 };
		}
		if (yoff < 0) {
			yoff += 8;
			position -= { 0, 1 };
		}
	}

	int distX = xoff;
	int distY = yoff;

	int minX = 15;
	if (position.x - 15 < 0) {
		minX = position.x + 1;
	}
	int maxX = 15;
	if (position.x + 15 > MAXDUNX) {
		maxX = MAXDUNX - position.x;
	}
	int minY = 15;
	if (position.y - 15 < 0) {
		minY = position.y + 1;
	}
	int maxY = 15;
	if (position.y + 15 > MAXDUNY) {
		maxY = MAXDUNY - position.y;
	}

	if (position.x >= 0 && position.x < MAXDUNX && position.y >= 0 && position.y < MAXDUNY) {
		if (currlevel < 17) {
			SetLight(position, 0);
		} else if (GetLight(position) > lightradius[nRadius][0]) {
			SetLight(position, lightradius[nRadius][0]);
		}
	}

	int mult = xoff + 8 * yoff;
	for (int y = 0; y < minY; y++) {
		for (int x = 1; x < maxX; x++) {
			int radiusBlock = lightblock[mult][y][x];
			if (radiusBlock < 128) {
				Point temp = position + Displacement { x, y };
				int8_t v = lightradius[nRadius][radiusBlock];
				if (temp.x >= 0 && temp.x < MAXDUNX && temp.y >= 0 && temp.y < MAXDUNY)
					if (v < GetLight(temp))
						SetLight(temp, v);
			}
		}
	}
	RotateRadius(&xoff, &yoff, &distX, &distY, &lightX, &lightY, &blockX, &blockY);
	mult = xoff + 8 * yoff;
	for (int y = 0; y < maxY; y++) {
		for (int x = 1; x < maxX; x++) {
			int radiusBlock = lightblock[mult][y + blockY][x + blockX];
			if (radiusBlock < 128) {
				Point temp = position + Displacement { y, -x };
				int8_t v = lightradius[nRadius][radiusBlock];
				if (temp.x >= 0 && temp.x < MAXDUNX && temp.y >= 0 && temp.y < MAXDUNY)
					if (v < GetLight(temp))
						SetLight(temp, v);
			}
		}
	}
	RotateRadius(&xoff, &yoff, &distX, &distY, &lightX, &lightY, &blockX, &blockY);
	mult = xoff + 8 * yoff;
	for (int y = 0; y < maxY; y++) {
		for (int x = 1; x < minX; x++) {
			int radiusBlock = lightblock[mult][y + blockY][x + blockX];
			if (radiusBlock < 128) {
				Point temp = position - Displacement { x, y };
				int8_t v = lightradius[nRadius][radiusBlock];
				if (temp.x >= 0 && temp.x < MAXDUNX && temp.y >= 0 && temp.y < MAXDUNY)
					if (v < GetLight(temp))
						SetLight(temp, v);
			}
		}
	}
	RotateRadius(&xoff, &yoff, &distX, &distY, &lightX, &lightY, &blockX, &blockY);
	mult = xoff + 8 * yoff;
	for (int y = 0; y < minY; y++) {
		for (int x = 1; x < minX; x++) {
			int radiusBlock = lightblock[mult][y + blockY][x + blockX];
			if (radiusBlock < 128) {
				Point temp = position + Displacement { -y, x };
				int8_t v = lightradius[nRadius][radiusBlock];
				if (temp.x >= 0 && temp.x < MAXDUNX && temp.y >= 0 && temp.y < MAXDUNY)
					if (v < GetLight(temp))
						SetLight(temp, v);
			}
		}
	}
}

void DoUnVision(Point position, int nRadius)
{
	nRadius++;
	nRadius++; // increasing the radius even further here prevents leaving stray vision tiles behind and doesn't seem to affect monster AI - applying new vision happens in the same tick
	int y1 = position.y - nRadius;
	int y2 = position.y + nRadius;
	int x1 = position.x - nRadius;
	int x2 = position.x + nRadius;

	if (y1 < 0) {
		y1 = 0;
	}
	if (y2 > MAXDUNY) {
		y2 = MAXDUNY;
	}
	if (x1 < 0) {
		x1 = 0;
	}
	if (x2 > MAXDUNX) {
		x2 = MAXDUNX;
	}

	for (int i = x1; i < x2; i++) {
		for (int j = y1; j < y2; j++) {
			dFlags[i][j] &= ~(BFLAG_VISIBLE | BFLAG_LIT);
		}
	}
}

void DoVision(Point position, int nRadius, bool doautomap, bool visible)
{

	if (position.x >= 0 && position.x <= MAXDUNX && position.y >= 0 && position.y <= MAXDUNY) {
		if (doautomap) {
			if (dFlags[position.x][position.y] != 0) {
				SetAutomapView(position);
			}
			dFlags[position.x][position.y] |= BFLAG_EXPLORED;
		}
		if (visible) {
			dFlags[position.x][position.y] |= BFLAG_LIT;
		}
		dFlags[position.x][position.y] |= BFLAG_VISIBLE;
	}

	for (int v = 0; v < 4; v++) {
		for (int j = 0; j < 23; j++) {
			bool nBlockerFlag = false;
			int nLineLen = 2 * (nRadius - RadiusAdj[j]);
			for (int k = 0; k < nLineLen && !nBlockerFlag; k += 2) {
				int x1adj = 0;
				int x2adj = 0;
				int y1adj = 0;
				int y2adj = 0;
				int nCrawlX = 0;
				int nCrawlY = 0;
				switch (v) {
				case 0:
					nCrawlX = position.x + VisionCrawlTable[j][k];
					nCrawlY = position.y + VisionCrawlTable[j][k + 1];
					if (VisionCrawlTable[j][k] > 0 && VisionCrawlTable[j][k + 1] > 0) {
						x1adj = -1;
						y2adj = -1;
					}
					break;
				case 1:
					nCrawlX = position.x - VisionCrawlTable[j][k];
					nCrawlY = position.y - VisionCrawlTable[j][k + 1];
					if (VisionCrawlTable[j][k] > 0 && VisionCrawlTable[j][k + 1] > 0) {
						y1adj = 1;
						x2adj = 1;
					}
					break;
				case 2:
					nCrawlX = position.x + VisionCrawlTable[j][k];
					nCrawlY = position.y - VisionCrawlTable[j][k + 1];
					if (VisionCrawlTable[j][k] > 0 && VisionCrawlTable[j][k + 1] > 0) {
						x1adj = -1;
						y2adj = 1;
					}
					break;
				case 3:
					nCrawlX = position.x - VisionCrawlTable[j][k];
					nCrawlY = position.y + VisionCrawlTable[j][k + 1];
					if (VisionCrawlTable[j][k] > 0 && VisionCrawlTable[j][k + 1] > 0) {
						y1adj = -1;
						x2adj = 1;
					}
					break;
				}
				if (nCrawlX >= 0 && nCrawlX < MAXDUNX && nCrawlY >= 0 && nCrawlY < MAXDUNY) {
					nBlockerFlag = nBlockTable[dPiece[nCrawlX][nCrawlY]];
					if ((x1adj + nCrawlX >= 0 && x1adj + nCrawlX < MAXDUNX && y1adj + nCrawlY >= 0 && y1adj + nCrawlY < MAXDUNY
					        && !nBlockTable[dPiece[x1adj + nCrawlX][y1adj + nCrawlY]])
					    || (x2adj + nCrawlX >= 0 && x2adj + nCrawlX < MAXDUNX && y2adj + nCrawlY >= 0 && y2adj + nCrawlY < MAXDUNY
					        && !nBlockTable[dPiece[x2adj + nCrawlX][y2adj + nCrawlY]])) {
						if (doautomap) {
							if (dFlags[nCrawlX][nCrawlY] != 0) {
								SetAutomapView({ nCrawlX, nCrawlY });
							}
							dFlags[nCrawlX][nCrawlY] |= BFLAG_EXPLORED;
						}
						if (visible) {
							dFlags[nCrawlX][nCrawlY] |= BFLAG_LIT;
						}
						dFlags[nCrawlX][nCrawlY] |= BFLAG_VISIBLE;
						if (!nBlockerFlag) {
							int8_t nTrans = dTransVal[nCrawlX][nCrawlY];
							if (nTrans != 0) {
								TransList[nTrans] = true;
							}
						}
					}
				}
			}
		}
	}
}

void MakeLightTable()
{
	uint8_t *tbl = LightTables.data();
	int shade = 0;
	int lights = 15;

	for (int i = 0; i < lights; i++) {
		*tbl++ = 0;
		for (int j = 0; j < 8; j++) {
			uint8_t col = 16 * j + shade;
			uint8_t max = 16 * j + 15;
			for (int k = 0; k < 16; k++) {
				if (k != 0 || j != 0) {
					*tbl++ = col;
				}
				if (col < max) {
					col++;
				} else {
					max = 0;
					col = 0;
				}
			}
		}
		for (int j = 16; j < 20; j++) {
			uint8_t col = 8 * j + (shade >> 1);
			uint8_t max = 8 * j + 7;
			for (int k = 0; k < 8; k++) {
				*tbl++ = col;
				if (col < max) {
					col++;
				} else {
					max = 0;
					col = 0;
				}
			}
		}
		for (int j = 10; j < 16; j++) {
			uint8_t col = 16 * j + shade;
			uint8_t max = 16 * j + 15;
			for (int k = 0; k < 16; k++) {
				*tbl++ = col;
				if (col < max) {
					col++;
				} else {
					max = 0;
					col = 0;
				}
				if (col == 255) {
					max = 0;
					col = 0;
				}
			}
		}
		shade++;
	}

	for (int i = 0; i < 256; i++) {
		*tbl++ = 0;
	}

	if (leveltype == DTYPE_HELL) {
		BYTE blood[16];
		tbl = LightTables.data();
		for (int i = 0; i < lights; i++) {
			int l1 = lights - i;
			int l2 = l1;
			int div = lights / l1;
			int rem = lights % l1;
			int cnt = 0;
			blood[0] = 0;
			uint8_t col = 1;
			for (int j = 1; j < 16; j++) {
				blood[j] = col;
				l2 += rem;
				if (l2 > l1 && j < 15) {
					j++;
					blood[j] = col;
					l2 -= l1;
				}
				cnt++;
				if (cnt == div) {
					col++;
					cnt = 0;
				}
			}
			*tbl++ = 0;
			for (int j = 1; j <= 15; j++) {
				*tbl++ = blood[j];
			}
			for (int j = 15; j > 0; j--) {
				*tbl++ = blood[j];
			}
			*tbl++ = 1;
			tbl += 224;
		}
		*tbl++ = 0;
		for (int j = 0; j < 31; j++) {
			*tbl++ = 1;
		}
		tbl += 224;
	}
	if (currlevel >= 17) {
		tbl = LightTables.data();
		for (int i = 0; i < lights; i++) {
			*tbl++ = 0;
			for (int j = 1; j < 16; j++)
				*tbl++ = j;
			tbl += 240;
		}
		*tbl++ = 0;
		for (int j = 1; j < 16; j++)
			*tbl++ = 1;
		tbl += 240;
	}

	LoadFileInMem("PlrGFX\\Infra.TRN", tbl, 256);
	tbl += 256;

	LoadFileInMem("PlrGFX\\Stone.TRN", tbl, 256);
	tbl += 256;

	for (int i = 0; i < 8; i++) {
		for (uint8_t col = 226; col < 239; col++) {
			if (i != 0 || col != 226) {
				*tbl++ = col;
			} else {
				*tbl++ = 0;
			}
		}
		*tbl++ = 0;
		*tbl++ = 0;
		*tbl++ = 0;
	}
	for (int i = 0; i < 4; i++) {
		uint8_t col = 224;
		for (int j = 224; j < 239; j += 2) {
			*tbl++ = col;
			col += 2;
		}
	}
	for (int i = 0; i < 6; i++) {
		for (uint8_t col = 224; col < 239; col++) {
			*tbl++ = col;
		}
		*tbl++ = 0;
	}

	for (int j = 0; j < 16; j++) {
		for (int i = 0; i < 128; i++) {
			if (i > (j + 1) * 8) {
				lightradius[j][i] = 15;
			} else {
				double fs = (double)15 * i / ((double)8 * (j + 1));
				lightradius[j][i] = (BYTE)(fs + 0.5);
			}
		}
	}

	if (currlevel >= 17) {
		for (int j = 0; j < 16; j++) {
			double fa = (sqrt((double)(16 - j))) / 128;
			fa *= fa;
			for (int i = 0; i < 128; i++) {
				lightradius[15 - j][i] = 15 - (BYTE)(fa * (double)((128 - i) * (128 - i)));
				if (lightradius[15 - j][i] > 15)
					lightradius[15 - j][i] = 0;
				lightradius[15 - j][i] = lightradius[15 - j][i] - (BYTE)((15 - j) / 2);
				if (lightradius[15 - j][i] > 15)
					lightradius[15 - j][i] = 0;
			}
		}
	}
	for (int j = 0; j < 8; j++) {
		for (int i = 0; i < 8; i++) {
			for (int k = 0; k < 16; k++) {
				for (int l = 0; l < 16; l++) {
					int a = (8 * l - j);
					int b = (8 * k - i);
					lightblock[j * 8 + i][k][l] = static_cast<uint8_t>(sqrt(a * a + b * b));
				}
			}
		}
	}
}

#ifdef _DEBUG
void ToggleLighting()
{
	DisableLighting = !DisableLighting;

	if (DisableLighting) {
		memset(dLight, 0, sizeof(dLight));
		return;
	}

	memcpy(dLight, dPreLight, sizeof(dLight));
	for (const auto &player : Players) {
		if (player.plractive && player.plrlevel == currlevel) {
			DoLighting(player.position.tile, player._pLightRad, -1);
		}
	}
}
#endif

void InitLightMax()
{
	LightsMax = 15;
}

void InitLighting()
{
	ActiveLightCount = 0;
	UpdateLighting = false;
	DisableLighting = false;

	for (int i = 0; i < MAXLIGHTS; i++) {
		ActiveLights[i] = i;
	}
}

int AddLight(Point position, int r)
{
	int lid;

	if (DisableLighting) {
		return NO_LIGHT;
	}

	lid = NO_LIGHT;

	if (ActiveLightCount < MAXLIGHTS) {
		lid = ActiveLights[ActiveLightCount++];
		Lights[lid].position.tile = position;
		Lights[lid]._lradius = r;
		Lights[lid].position.offset = { 0, 0 };
		Lights[lid]._ldel = false;
		Lights[lid]._lunflag = false;
		UpdateLighting = true;
	}

	return lid;
}

void AddUnLight(int i)
{
	if (DisableLighting || i == NO_LIGHT) {
		return;
	}

	Lights[i]._ldel = true;
	UpdateLighting = true;
}

void ChangeLightRadius(int i, int r)
{
	if (DisableLighting || i == NO_LIGHT) {
		return;
	}

	Lights[i]._lunflag = true;
	Lights[i].position.old = Lights[i].position.tile;
	Lights[i].oldRadius = Lights[i]._lradius;
	Lights[i]._lradius = r;
	UpdateLighting = true;
}

void ChangeLightXY(int i, Point position)
{
	if (DisableLighting || i == NO_LIGHT) {
		return;
	}

	Lights[i]._lunflag = true;
	Lights[i].position.old = Lights[i].position.tile;
	Lights[i].oldRadius = Lights[i]._lradius;
	Lights[i].position.tile = position;
	UpdateLighting = true;
}

void ChangeLightOffset(int i, Point position)
{
	if (DisableLighting || i == NO_LIGHT) {
		return;
	}

	Lights[i]._lunflag = true;
	Lights[i].position.old = Lights[i].position.tile;
	Lights[i].oldRadius = Lights[i]._lradius;
	Lights[i].position.offset = position;
	UpdateLighting = true;
}

void ChangeLight(int i, Point position, int r)
{
	if (DisableLighting || i == NO_LIGHT) {
		return;
	}

	Lights[i]._lunflag = true;
	Lights[i].position.old = Lights[i].position.tile;
	Lights[i].oldRadius = Lights[i]._lradius;
	Lights[i].position.tile = position;
	Lights[i]._lradius = r;
	UpdateLighting = true;
}

void ProcessLightList()
{
	if (DisableLighting) {
		return;
	}

	if (UpdateLighting) {
		for (int i = 0; i < ActiveLightCount; i++) {
			int j = ActiveLights[i];
			if (Lights[j]._ldel) {
				DoUnLight(Lights[j].position.tile.x, Lights[j].position.tile.y, Lights[j]._lradius);
			}
			if (Lights[j]._lunflag) {
				DoUnLight(Lights[j].position.old.x, Lights[j].position.old.y, Lights[j].oldRadius);
				Lights[j]._lunflag = false;
			}
		}
		for (int i = 0; i < ActiveLightCount; i++) {
			int j = ActiveLights[i];
			if (!Lights[j]._ldel) {
				DoLighting(Lights[j].position.tile, Lights[j]._lradius, j);
			}
		}
		int i = 0;
		while (i < ActiveLightCount) {
			if (Lights[ActiveLights[i]]._ldel) {
				ActiveLightCount--;
				BYTE temp = ActiveLights[ActiveLightCount];
				ActiveLights[ActiveLightCount] = ActiveLights[i];
				ActiveLights[i] = temp;
			} else {
				i++;
			}
		}
	}

	UpdateLighting = false;
}

void SavePreLighting()
{
	memcpy(dPreLight, dLight, sizeof(dPreLight));
}

void InitVision()
{
	VisionCount = 0;
	dovision = false;
	VisionId = 1;

	for (int i = 0; i < TransVal; i++) {
		TransList[i] = false;
	}
}

int AddVision(Point position, int r, bool mine)
{
	int vid = -1; // BUGFIX: if VisionCount >= MAXVISION behavior is undefined (fixed)

	if (VisionCount < MAXVISION) {
		VisionList[VisionCount].position.tile = position;
		VisionList[VisionCount]._lradius = r;
		vid = VisionId++;
		VisionList[VisionCount]._lid = vid;
		VisionList[VisionCount]._ldel = false;
		VisionList[VisionCount]._lunflag = false;
		VisionList[VisionCount]._lflags = mine;
		VisionCount++;
		dovision = true;
	}

	return vid;
}

void ChangeVisionRadius(int id, int r)
{
	for (int i = 0; i < VisionCount; i++) {
		if (VisionList[i]._lid == id) {
			VisionList[i]._lunflag = true;
			VisionList[i].position.old = VisionList[i].position.tile;
			VisionList[i].oldRadius = VisionList[i]._lradius;
			VisionList[i]._lradius = r;
			dovision = true;
		}
	}
}

void ChangeVisionXY(int id, Point position)
{
	for (int i = 0; i < VisionCount; i++) {
		if (VisionList[i]._lid == id) {
			VisionList[i]._lunflag = true;
			VisionList[i].position.old = VisionList[i].position.tile;
			VisionList[i].oldRadius = VisionList[i]._lradius;
			VisionList[i].position.tile = position;
			dovision = true;
		}
	}
}

void ProcessVisionList()
{
	if (dovision) {
		for (int i = 0; i < VisionCount; i++) {
			if (VisionList[i]._ldel) {
				DoUnVision(VisionList[i].position.tile, VisionList[i]._lradius);
			}
			if (VisionList[i]._lunflag) {
				DoUnVision(VisionList[i].position.old, VisionList[i].oldRadius);
				VisionList[i]._lunflag = false;
			}
		}
		for (int i = 0; i < TransVal; i++) {
			TransList[i] = false;
		}
		for (int i = 0; i < VisionCount; i++) {
			if (!VisionList[i]._ldel) {
				DoVision(
				    VisionList[i].position.tile,
				    VisionList[i]._lradius,
				    VisionList[i]._lflags,
				    VisionList[i]._lflags);
			}
		}
		bool delflag;
		do {
			delflag = false;
			for (int i = 0; i < VisionCount; i++) {
				if (VisionList[i]._ldel) {
					VisionCount--;
					if (VisionCount > 0 && i != VisionCount) {
						VisionList[i] = VisionList[VisionCount];
					}
					delflag = true;
				}
			}
		} while (delflag);
	}

	dovision = false;
}

void lighting_color_cycling()
{
	if (leveltype != DTYPE_HELL) {
		return;
	}

	uint8_t *tbl = LightTables.data();

	for (int j = 0; j < 16; j++) {
		tbl++;
		uint8_t col = *tbl;
		for (int i = 0; i < 30; i++) {
			tbl[0] = tbl[1];
			tbl++;
		}
		*tbl = col;
		tbl += 225;
	}
}

} // namespace devilution
