#include <stdint.h>
#include <stdbool.h>

#pragma pack(push)
#pragma pack(1)

typedef struct allo_vector
{
	double x, y, z;
} allo_vector;

extern allo_vector allo_vector_subtract(allo_vector l, allo_vector r);
extern double allo_vector_dot(allo_vector l, allo_vector r);
extern double allo_vector_length(allo_vector l);
extern double allo_vector_angle(allo_vector l, allo_vector r);
extern char *allo_vec_string(allo_vector l);



// column major transformation matrix
typedef union allo_m4x4
{
	struct {
		double c1r1, c1r2, c1r3, c1r4, // 1st column
			c2r1, c2r2, c2r3, c2r4, // 2nd column, etc
			c3r1, c3r2, c3r3, c3r4,
			c4r1, c4r2, c4r3, c4r4;
	};
	double v[16];
} allo_m4x4;

extern allo_m4x4 allo_m4x4_identity();
extern allo_m4x4 allo_m4x4_translate(allo_vector translation);
extern allo_m4x4 allo_m4x4_rotate(double angle, allo_vector axis);
extern allo_m4x4 allo_m4x4_concat(allo_m4x4 l, allo_m4x4 r);
extern allo_m4x4 allo_m4x4_inverse(allo_m4x4 m);
extern allo_vector allo_m4x4_transform(allo_m4x4 l, allo_vector r, bool positional);
extern char *allo_m4x4_string(allo_m4x4 m);
#pragma pack(pop)