/**

 * @file    SimplexNoise.cpp

 * @brief   A Perlin Simplex Noise C++ Implementation (1D, 2D, 3D).

 *

 * Copyright (c) 2014-2018 Sebastien Rombauts (sebastien.rombauts@gmail.com)

 *

 * This C++ implementation is based on the speed-improved Java version 2012-03-09

 * by Stefan Gustavson (original Java source code in the public domain).

 * http://webstaff.itn.liu.se/~stegu/simplexnoise/SimplexNoise.java:

 * - Based on example code by Stefan Gustavson (stegu@itn.liu.se).

 * - Optimisations by Peter Eastman (peastman@drizzle.stanford.edu).

 * - Better rank ordering method by Stefan Gustavson in 2012.

 *

 * This implementation is "Simplex Noise" as presented by

 * Ken Perlin at a relatively obscure and not often cited course

 * session "Real-Time Shading" at Siggraph 2001 (before real

 * time shading actually took on), under the title "hardware noise".

 * The 3D function is numerically equivalent to his Java reference

 * code available in the PDF course notes, although I re-implemented

 * it from scratch to get more readable code. The 1D, 2D and 4D cases

 * were implemented from scratch by me from Ken Perlin's text.

 *

 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt

 * or copy at http://opensource.org/licenses/MIT)

 */

//source:
//https://github.com/SRombauts/SimplexNoise/blob/master/src/SimplexNoise.cpp

#include "SimplexNoise.h"
//random
#include <stdlib.h>



 /**

  * Computes the largest integer value not greater than the fp one

  *

  * This method is faster than using (int32_t)std::floor(fp).

  *

  * I measured it to be approximately twice as fast:

  *  fp:  ~18.4ns instead of ~39.6ns on an AMD APU),

  *  fp: ~20.6ns instead of ~36.6ns on an AMD APU),

  * Reference: http://www.codeproject.com/Tips/700780/Fast-floor-ceiling-functions

  *

  * @param[in] fp    fp input value

  *

  * @return largest integer value not greater than fp

  */

/*static inline int32_t fastfloor(fp fpingpoint) {

	int32_t i = static_cast<int32_t>(fpingpoint);

	return (fpingpoint < i) ? (i - 1) : (i);

}*/



SimplexNoise::SimplexNoise(int seed) 
{
	srand(seed);
	//fill lookup table
	for (int i = 0; i < NOISE_REPEAT; i++)
	{
		perm[i] = static_cast<uint8_t> (i);
	}
	//shuffle array
	//source:
	//http://www.cplusplus.com/reference/algorithm/shuffle/
	for (int i = NOISE_REPEAT - 1; i > 0; i--)
	{
		uint8_t current = perm[i];
		int index = rand() % (i + 1);
		perm[i] = perm[index];
		perm[index] = current;
	}
}



/**

 * Helper function to hash an integer using the above permutation table

 *

 *  This inline function costs around 1ns, and is called N+1 times for a noise of N dimension.

 *

 *  Using a real hash function would be better to improve the "repeatability of 256" of the above permutation table,

 * but fast integer Hash functions uses more time and have bad random properties.

 *

 * @param[in] i Integer value to hash

 *

 * @return 8-bits hashed value

 */

inline uint8_t SimplexNoise::hash(int32_t i) {
#if NOISE_REPEAT == 0x100
	return perm[static_cast<uint8_t>(i)];
	//return perm[i & 0xFF];
#else
	return perm[i % NOISE_REPEAT];
#endif
}



/* NOTE Gradient table to test if lookup-table are more efficient than calculs

static const fp gradients1D[16] = {

		-8.f, -7.f, -6.f, -5.f, -4.f, -3.f, -2.f, -1.f,

		 1.f,  2.f,  3.f,  4.f,  5.f,  6.f,  7.f,  8.f

};

*/



/**

 * Helper function to compute gradients-dot-residual vectors (1D)

 *

 * @note that these generate gradients of more than unit length. To make

 * a close match with the value range of classic Perlin noise, the final

 * noise values need to be rescaled to fit nicely within [-1,1].

 * (The simplex noise functions as such also have different scaling.)

 * Note also that these noise functions are the most practical and useful

 * signed version of Perlin noise.

 *

 * @param[in] hash  hash value

 * @param[in] x     distance to the corner

 *

 * @return gradient value

 */

static fp grad(int32_t hash, fp x) {

	const int32_t h = hash & 0x0F;  // Convert low 4 bits of hash code

	fp grad = 1.0f + (h & 7);    // Gradient value 1.0, 2.0, ..., 8.0

	if ((h & 8) != 0) grad = -grad; // Set a random sign for the gradient

//  fp grad = gradients1D[h];    // NOTE : Test of Gradient look-up table instead of the above

	return (grad * x);              // Multiply the gradient with the distance

}



/**

 * Helper functions to compute gradients-dot-residual vectors (2D)

 *

 * @param[in] hash  hash value

 * @param[in] x     x coord of the distance to the corner

 * @param[in] y     y coord of the distance to the corner

 *

 * @return gradient value

 */

static fp grad(int32_t hash, fp x, fp y) {

	const int32_t h = hash & 0x3F;  // Convert low 3 bits of hash code

	const fp u = h < 4 ? x : y;  // into 8 simple gradient directions,

	const fp v = h < 4 ? y : x;

	return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v); // and compute the dot product with (x,y).

}



/**

 * Helper functions to compute gradients-dot-residual vectors (3D)

 *

 * @param[in] hash  hash value

 * @param[in] x     x coord of the distance to the corner

 * @param[in] y     y coord of the distance to the corner

 * @param[in] z     z coord of the distance to the corner

 *

 * @return gradient value

 */

static fp grad(int32_t hash, fp x, fp y, fp z) {

	int h = hash & 15;     // Convert low 4 bits of hash code into 12 simple

	fp u = h < 8 ? x : y; // gradient directions, and compute dot product.

	fp v = h < 4 ? y : h == 12 || h == 14 ? x : z; // Fix repeats at h = 12 to 15

	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);

}



//1D Perlin simplex noise
//Takes around 74ns on an AMD APU.
//return: Noise value in the range[-1; 1], value of 0 on all integer coordinates.
fp SimplexNoise::noise1(fp x) 
{
	fp n0, n1;   // Noise contributions from the two "corners"
	// No need to skew the input space in 1D

	// Corners coordinates (nearest integer values):

	int32_t i0 = fastfloor(x);

	int32_t i1 = i0 + 1;

	// Distances to corners (between 0 and 1):

	fp x0 = x - i0;

	fp x1 = x0 - 1.0f;

	// Calculate the contribution from the first corner

	fp t0 = 1.0f - x0 * x0;

	//  if(t0 < 0.0f) t0 = 0.0f; // not possible

	t0 *= t0;
	n0 = t0 * t0 * grad(hash(i0), x0);

	// Calculate the contribution from the second corner

	fp t1 = 1.0f - x1 * x1;

	//  if(t1 < 0.0f) t1 = 0.0f; // not possible

	t1 *= t1;

	n1 = t1 * t1 * grad(hash(i1), x1);

	// The maximum value of this noise is 8*(3/4)^4 = 2.53125
	// A factor of 0.395 scales to fit exactly within [-1,1]

	return 0.395f* (n0 + n1);

}



//2D Perlin simplex noise
//Takes around 150ns on an AMD APU.
//return: Noise value in the range[-1; 1], value of 0 on all integer coordinates.
fp SimplexNoise::noise2(fp x, fp y) {

	fp n0, n1, n2;   // Noise contributions from the three corners

	// Skewing/Unskewing factors for 2D

	static const fp F2 = 0.366025403f;  // F2 = (sqrt(3) - 1) / 2

	static const fp G2 = 0.211324865f;  // G2 = (3 - sqrt(3)) / 6   = F2 / (1 + 2 * K)

	// Skew the input space to determine which simplex cell we're in

	const fp s = (x + y) * F2;  // Hairy factor for 2D

	const fp xs = x + s;

	const fp ys = y + s;

	const int32_t i = fastfloor(xs);

	const int32_t j = fastfloor(ys);

	// Unskew the cell origin back to (x,y) space

	const fp t = static_cast<fp>(i + j) * G2;

	const fp X0 = i - t;

	const fp Y0 = j - t;

	const fp x0 = x - X0;  // The x,y distances from the cell origin

	const fp y0 = y - Y0;

	// For the 2D case, the simplex shape is an equilateral triangle.

	// Determine which simplex we are in.

	int32_t i1, j1;  // Offsets for second (middle) corner of simplex in (i,j) coords

	if (x0 > y0) {   // lower triangle, XY order: (0,0)->(1,0)->(1,1)
		i1 = 1;
		j1 = 0;
	}
	else {   // upper triangle, YX order: (0,0)->(0,1)->(1,1)
		i1 = 0;
		j1 = 1;
	}

	// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and

	// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where

	// c = (3-sqrt(3))/6

	const fp x1 = x0 - i1 + G2;            // Offsets for middle corner in (x,y) unskewed coords

	const fp y1 = y0 - j1 + G2;

	const fp x2 = x0 - 1.0f + 2.0f * G2;   // Offsets for last corner in (x,y) unskewed coords

	const fp y2 = y0 - 1.0f + 2.0f * G2;

	// Work out the hashed gradient indices of the three simplex corners

	cint gi0 = hash(i + hash(j));

	cint gi1 = hash(i + i1 + hash(j + j1));

	cint gi2 = hash(i + 1 + hash(j + 1));



	// Calculate the contribution from the first corner

	fp t0 = 0.5f - x0 * x0 - y0 * y0;

	if (t0 < 0.0f) {
		n0 = 0.0f;
	}
	else {
		t0 *= t0;
		n0 = t0 * t0 * grad(gi0, x0, y0);
	}

	// Calculate the contribution from the second corner

	fp t1 = 0.5f - x1 * x1 - y1 * y1;

	if (t1 < 0.0f) 
	{
		n1 = 0.0f;
	}
	else
	{
		t1 *= t1;
		n1 = t1 * t1 * grad(gi1, x1, y1);
	}



	// Calculate the contribution from the third corner

	fp t2 = 0.5f - x2 * x2 - y2 * y2;

	if (t2 < 0.0f) {

		n2 = 0.0f;

	}
	else {

		t2 *= t2;

		n2 = t2 * t2 * grad(gi2, x2, y2);

	}



	// Add contributions from each corner to get the final noise value.

	// The result is scaled to return values in the interval [-1,1].

	return 45.23065f* (n0 + n1 + n2);

}





//3D Perlin simplex noise
//return: Noise value in the range[-1; 1], value of 0 on all integer coordinates.

fp SimplexNoise::noise3(cfp& x, cfp& y, cfp& z) {

	fp n0, n1, n2, n3; // Noise contributions from the four corners

	// Skewing/Unskewing factors for 3D

	static const fp F3 = 1.0f / 3.0f;

	static const fp G3 = 1.0f / 6.0f;



	// Skew the input space to determine which simplex cell we're in

	cfp s = (x + y + z) * F3; // Very nice and simple skew factor for 3D

	cint i = fastfloor(x + s);

	cint j = fastfloor(y + s);

	cint k = fastfloor(z + s);

	cfp t = (i + j + k) * G3;

	cfp X0 = i - t; // Unskew the cell origin back to (x,y,z) space

	cfp Y0 = j - t;

	cfp Z0 = k - t;

	cfp x0 = x - X0; // The x,y,z distances from the cell origin

	cfp y0 = y - Y0;

	cfp z0 = z - Z0;



	// For the 3D case, the simplex shape is a slightly irregular tetrahedron.

	// Determine which simplex we are in.

	int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords

	int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords

	if (x0 >= y0) {

		if (y0 >= z0) {

			i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; // X Y Z order

		}
		else if (x0 >= z0) {

			i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; // X Z Y order

		}
		else {

			i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; // Z X Y order

		}

	}
	else { // x0<y0

		if (y0 < z0) {

			i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; // Z Y X order

		}
		else if (x0 < z0) {

			i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; // Y Z X order

		}
		else {

			i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; // Y X Z order

		}

	}



	// A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),

	// a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and

	// a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where

	// c = 1/6.

	cfp x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords

	cfp y1 = y0 - j1 + G3;

	cfp z1 = z0 - k1 + G3;

	cfp x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords

	cfp y2 = y0 - j2 + 2.0f * G3;

	cfp z2 = z0 - k2 + 2.0f * G3;

	cfp x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords

	cfp y3 = y0 - 1.0f + 3.0f * G3;

	cfp z3 = z0 - 1.0f + 3.0f * G3;



	// Work out the hashed gradient indices of the four simplex corners

	cint gi0 = hash(i + hash(j + hash(k)));

	cint gi1 = hash(i + i1 + hash(j + j1 + hash(k + k1)));

	cint gi2 = hash(i + i2 + hash(j + j2 + hash(k + k2)));

	cint gi3 = hash(i + 1 + hash(j + 1 + hash(k + 1)));



	// Calculate the contribution from the four corners

	fp t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;

	if (t0 < 0) {

		n0 = 0.0;

	}
	else {

		t0 *= t0;

		n0 = t0 * t0 * grad(gi0, x0, y0, z0);

	}

	fp t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;

	if (t1 < 0) {

		n1 = 0.0;

	}
	else {

		t1 *= t1;

		n1 = t1 * t1 * grad(gi1, x1, y1, z1);

	}

	fp t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;

	if (t2 < 0) {

		n2 = 0.0;

	}
	else {

		t2 *= t2;

		n2 = t2 * t2 * grad(gi2, x2, y2, z2);

	}

	fp t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;

	if (t3 < 0) {

		n3 = 0.0;

	}
	else {

		t3 *= t3;

		n3 = t3 * t3 * grad(gi3, x3, y3, z3);

	}

	// Add contributions from each corner to get the final noise value.

	// The result is scaled to stay just inside [-1,1]

	return 32.0f* (n0 + n1 + n2 + n3);

}