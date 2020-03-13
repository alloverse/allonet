#include "allonet/math.h"
#include <math.h>

allo_m4x4 allo_m4x4_identity()
{
	return (allo_m4x4) {
		1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
	};
}

allo_m4x4 allo_m4x4_translate(allo_vector t)
{
	return (allo_m4x4) {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		t.x, t.y, t.z, 1
	};
}

allo_m4x4 allo_m4x4_rotate(double phi, allo_vector axis)
{
	// http://www.opengl-tutorial.org/assets/faq_quaternions/index.html#Q26
	allo_m4x4 m = allo_m4x4_identity();
	double rcos = cos(phi);
	double rsin = sin(phi);
	double u = axis.x, v = axis.y, w = axis.z;
	m.c1r1 = rcos + u * u * (1 - rcos);
	m.c1r2 = w * rsin + v * u * (1 - rcos);
	m.c1r3 = -v * rsin + w * u * (1 - rcos);
	m.c2r1 = -w * rsin + u * v * (1 - rcos);
	m.c2r2 = rcos + v * v * (1 - rcos);
	m.c2r3 = u * rsin + w * v * (1 - rcos);
	m.c3r1 = v * rsin + u * w * (1 - rcos);
	m.c3r2 = -u * rsin + v * w * (1 - rcos);
	m.c3r3 = rcos + w * w * (1 - rcos);

	return m;
}

#define rc(m, r, c) m.v[r*4 + c]

allo_m4x4 allo_m4x4_concat(allo_m4x4 a, allo_m4x4 b)
{
	allo_m4x4 m = allo_m4x4_identity();
	for (int r = 0; r <= 3; r++) {
		for (int c = 0; c <= 3; c++) {
			rc(m, r, c) =
				rc(a, r, 0) * rc(b, 0, c) +
				rc(a, r, 1) * rc(b, 1, c) +
				rc(a, r, 2) * rc(b, 2, c) +
				rc(a, r, 3) * rc(b, 3, c);
		}
	}
	return m;
}

allo_vector allo_m4x4_transform(allo_m4x4 l, allo_vector r, bool positional)
{
	return (allo_vector) {
		l.c1r1 * r.x + l.c2r1 * r.y + l.c3r1 * r.z + l.c4r1 * positional,
		l.c1r2 * r.x + l.c2r2 * r.y + l.c3r2 * r.z + l.c4r2 * positional,
		l.c1r3 * r.x + l.c2r3 * r.y + l.c3r3 * r.z + l.c4r3 * positional
	};
}