typedef struct allo_vector
{
	double x, y, z;
} allo_vector;

// column major transformation matrix
typedef union allo_m4x4
{
	struct {
		double m11, m12, m13, m14, // 1st column
			m21, m22, m23, m24, // 2nd column, etc
			m31, m32, m33, m34,
			m41, m42, m43, m44;
	};
	double v[16];
} allo_m4x4;

extern allo_m4x4 allo_m4x4_identity();
extern allo_m4x4 allo_m4x4_translate(allo_vector translation);
extern allo_m4x4 allo_m4x4_rotate(double angle, allo_vector axis);
extern allo_m4x4 allo_m4x4_concat(allo_m4x4 l, allo_m4x4 r);
extern allo_vector allo_m4x4_transform(allo_m4x4 l, allo_vector r);