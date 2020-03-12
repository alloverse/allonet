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
	m.m11 = rcos + u * u * (1 - rcos);
	m.m12 = w * rsin + v * u * (1 - rcos);
	m.m13 = -v * rsin + w * u * (1 - rcos);
	m.m21 = -w * rsin + u * v * (1 - rcos);
	m.m22 = rcos + v * v * (1 - rcos);
	m.m23 = u * rsin + w * v * (1 - rcos);
	m.m31 = v * rsin + u * w * (1 - rcos);
	m.m32 = -u * rsin + v * w * (1 - rcos);
	m.m33 = rcos + w * w * (1 - rcos);

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

allo_vector allo_m4x4_transform(allo_m4x4 l, allo_vector r)
{
	return (allo_vector) {
		rc(l, 0, 0)* r.x + rc(l, 0, 1) * r.y + rc(l, 0, 2) * r.z + rc(l, 0, 3),
		rc(l, 1, 0)* r.x + rc(l, 1, 1) * r.y + rc(l, 1, 2) * r.z + rc(l, 1, 3),
		rc(l, 2, 0)* r.x + rc(l, 2, 1) * r.y + rc(l, 2, 2) * r.z + rc(l, 2, 3)
	};
}