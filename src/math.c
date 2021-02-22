#include "allonet/math.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "mathc.h"


allo_vector allo_vector_subtract(allo_vector l, allo_vector r)
{
    return (allo_vector){{
        l.x - r.x,
        l.y - r.y,
        l.z - r.z
    }};
}

allo_vector allo_vector_add(allo_vector l, allo_vector r)
{
    return (allo_vector) {{
        l.x + r.x,
        l.y + r.y,
        l.z + r.z
    }};
}
allo_vector allo_vector_mul(allo_vector l, allo_vector r)
{
    return (allo_vector) {{
        l.x * r.x,
        l.y * r.y,
        l.z * r.z
    }};
}
extern allo_vector allo_vector_scale(allo_vector l, double r)
{
    return (allo_vector) {{
        l.x* r,
        l.y* r,
        l.z* r
    }};
}
allo_vector allo_vector_div(allo_vector l, allo_vector r)
{
  return (allo_vector) {{
      l.x / r.x,
      l.y / r.y,
      l.z / r.z
  }};
}

allo_vector allo_vector_normalize(allo_vector l)
{
  float len = allo_vector_length(l);
  return len == 0.0 ? l : allo_vector_scale(l, 1.0 / len);
}


double allo_vector_dot(allo_vector l, allo_vector r)
{
    return l.x * r.x + l.y * r.y + l.z * r.z;
}

double allo_vector_length(allo_vector l)
{
    return sqrt(allo_vector_dot(l, l));
}
double allo_vector_angle(allo_vector l, allo_vector r)
{
    return acos(allo_vector_dot(l, r) / (allo_vector_length(l) * allo_vector_length(r)));
}

static const allo_m4x4 static_identity = {{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
}};

allo_m4x4 allo_m4x4_identity()
{
	return static_identity;
}

bool allo_m4x4_is_identity(allo_m4x4 m)
{
  for(int i = 0; i < 16; i++) {
    if(m.v[i] != static_identity.v[i])
    {
      return false;
    }
  }
  return true;
}

allo_m4x4 allo_m4x4_translate(allo_vector t)
{
	return (allo_m4x4) {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        t.x, t.y, t.z, 1
    }};
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
  mat4_multiply(m.v, a.v, b.v);
  return m;

  // this code is wrong??
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

extern allo_m4x4 allo_m4x4_add(allo_m4x4 l, allo_m4x4 r)
{
  allo_m4x4 m;
  for (int i = 0; i < 16; i++)
  {
    m.v[i] = l.v[i] + r.v[i];
  }
  return m;
}
extern allo_m4x4 allo_m4x4_scalar_multiply(allo_m4x4 l, double r)
{
  allo_m4x4 m;
  for (int i = 0; i < 16; i++)
  {
    m.v[i] = l.v[i] * r;
  }
  return m;
}

extern allo_m4x4 allo_m4x4_interpolate(allo_m4x4 l, allo_m4x4 r, double fraction)
{
  return allo_m4x4_add(allo_m4x4_scalar_multiply(l, fraction), allo_m4x4_scalar_multiply(r, 1.0 - fraction));
}

extern allo_m4x4 allo_m4x4_inverse(allo_m4x4 ma)
{
	allo_m4x4 out;
  mat4_inverse(out.v, ma.v); return out;

  // this code seems to be correct, but I'm gonna use theirs anyway.
  double *m = ma.v;
  double* inv = out.v;

  inv[0] = m[5] * m[10] * m[15] -
    m[5] * m[11] * m[14] -
    m[9] * m[6] * m[15] +
    m[9] * m[7] * m[14] +
    m[13] * m[6] * m[11] -
    m[13] * m[7] * m[10];

  inv[4] = -m[4] * m[10] * m[15] +
    m[4] * m[11] * m[14] +
    m[8] * m[6] * m[15] -
    m[8] * m[7] * m[14] -
    m[12] * m[6] * m[11] +
    m[12] * m[7] * m[10];

  inv[8] = m[4] * m[9] * m[15] -
    m[4] * m[11] * m[13] -
    m[8] * m[5] * m[15] +
    m[8] * m[7] * m[13] +
    m[12] * m[5] * m[11] -
    m[12] * m[7] * m[9];

  inv[12] = -m[4] * m[9] * m[14] +
    m[4] * m[10] * m[13] +
    m[8] * m[5] * m[14] -
    m[8] * m[6] * m[13] -
    m[12] * m[5] * m[10] +
    m[12] * m[6] * m[9];

  inv[1] = -m[1] * m[10] * m[15] +
    m[1] * m[11] * m[14] +
    m[9] * m[2] * m[15] -
    m[9] * m[3] * m[14] -
    m[13] * m[2] * m[11] +
    m[13] * m[3] * m[10];

  inv[5] = m[0] * m[10] * m[15] -
    m[0] * m[11] * m[14] -
    m[8] * m[2] * m[15] +
    m[8] * m[3] * m[14] +
    m[12] * m[2] * m[11] -
    m[12] * m[3] * m[10];

  inv[9] = -m[0] * m[9] * m[15] +
    m[0] * m[11] * m[13] +
    m[8] * m[1] * m[15] -
    m[8] * m[3] * m[13] -
    m[12] * m[1] * m[11] +
    m[12] * m[3] * m[9];

  inv[13] = m[0] * m[9] * m[14] -
    m[0] * m[10] * m[13] -
    m[8] * m[1] * m[14] +
    m[8] * m[2] * m[13] +
    m[12] * m[1] * m[10] -
    m[12] * m[2] * m[9];

  inv[2] = m[1] * m[6] * m[15] -
    m[1] * m[7] * m[14] -
    m[5] * m[2] * m[15] +
    m[5] * m[3] * m[14] +
    m[13] * m[2] * m[7] -
    m[13] * m[3] * m[6];

  inv[6] = -m[0] * m[6] * m[15] +
    m[0] * m[7] * m[14] +
    m[4] * m[2] * m[15] -
    m[4] * m[3] * m[14] -
    m[12] * m[2] * m[7] +
    m[12] * m[3] * m[6];

  inv[10] = m[0] * m[5] * m[15] -
    m[0] * m[7] * m[13] -
    m[4] * m[1] * m[15] +
    m[4] * m[3] * m[13] +
    m[12] * m[1] * m[7] -
    m[12] * m[3] * m[5];

  inv[14] = -m[0] * m[5] * m[14] +
    m[0] * m[6] * m[13] +
    m[4] * m[1] * m[14] -
    m[4] * m[2] * m[13] -
    m[12] * m[1] * m[6] +
    m[12] * m[2] * m[5];

  inv[3] = -m[1] * m[6] * m[11] +
    m[1] * m[7] * m[10] +
    m[5] * m[2] * m[11] -
    m[5] * m[3] * m[10] -
    m[9] * m[2] * m[7] +
    m[9] * m[3] * m[6];

  inv[7] = m[0] * m[6] * m[11] -
    m[0] * m[7] * m[10] -
    m[4] * m[2] * m[11] +
    m[4] * m[3] * m[10] +
    m[8] * m[2] * m[7] -
    m[8] * m[3] * m[6];

  inv[11] = -m[0] * m[5] * m[11] +
    m[0] * m[7] * m[9] +
    m[4] * m[1] * m[11] -
    m[4] * m[3] * m[9] -
    m[8] * m[1] * m[7] +
    m[8] * m[3] * m[5];

  inv[15] = m[0] * m[5] * m[10] -
    m[0] * m[6] * m[9] -
    m[4] * m[1] * m[10] +
    m[4] * m[2] * m[9] +
    m[8] * m[1] * m[6] -
    m[8] * m[2] * m[5];

  double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

  if (det == 0)
    return allo_m4x4_identity();

  det = 1.0 / det;
  for (int i = 0; i < 16; i++)
    out.v[i] = inv[i] * det;

  return out;
}

allo_vector allo_m4x4_transform(allo_m4x4 l, allo_vector r, bool positional)
{
	return (allo_vector) {{
		l.c1r1 * r.x + l.c2r1 * r.y + l.c3r1 * r.z + l.c4r1 * positional,
		l.c1r2 * r.x + l.c2r2 * r.y + l.c3r2 * r.z + l.c4r2 * positional,
		l.c1r3 * r.x + l.c2r3 * r.y + l.c3r3 * r.z + l.c4r3 * positional
	}};
}

extern allo_vector allo_m4x4_get_position(allo_m4x4 l)
{
  return allo_m4x4_transform(l, (allo_vector) {{ 0, 0, 0 }}, true);
}

extern allo_rotation allo_m4x4_get_rotation(allo_m4x4 l)
{
  allo_rotation rot;
  double* m = l.v;

  rot.axis = allo_vector_normalize((allo_vector) {{ m[6] - m[9], m[8] - m[2], m[1] - m[4] }});

  allo_vector lengths = {{ vec3_length(m + 0), vec3_length(m + 4), vec3_length(m + 8) }};
  allo_vector diagonal = {{ m[0], m[5], m[10] }};
  allo_vector scaled_diagonal = allo_vector_div(diagonal, lengths);
  rot.angle = acosf((scaled_diagonal.x + scaled_diagonal.y + scaled_diagonal.z - 1.) / 2.);

  return rot;
}

bool allo_m4x4_equal(allo_m4x4 a, allo_m4x4 b, double sigma)
{
  for (int i = 0; i < 16; i++)
    if (fabs(a.v[i] - b.v[i]) > sigma)
      return false;
  return true;
}

extern char *allo_m4x4_string(allo_m4x4 m)
{
    char *s = malloc(255);
    snprintf(s, 255, "\t%+.2f %+.2f %+.2f %+.2f\n\t%+.2f %+.2f %+.2f %+.2f\n\t%+.2f %+.2f %+.2f %+.2f\n\t%+.2f %+.2f %+.2f %+.2f",
        m.c1r1, m.c2r1, m.c3r1, m.c4r1,
        m.c1r2, m.c2r2, m.c3r2, m.c4r2,
        m.c1r3, m.c2r3, m.c3r3, m.c4r3,
        m.c1r4, m.c2r4, m.c3r4, m.c4r4
    );
    return s;
}

extern char *allo_vec_string(allo_vector l)
{
    char *s = malloc(40);
    snprintf(s, 40, "%.2f %.2f %.2f",
        l.x, l.y, l.z
    );
    return s;
}
