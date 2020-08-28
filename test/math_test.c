#include <unity.h>
#include "../src/util.h"
#include "mathc.h"
#include <stdlib.h>
#include <string.h>

void test_matrices_equal(allo_m4x4 a, allo_m4x4 b)
{
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
      0.001, b.v[i], a.v[i], "Index is wrong"
    );
  }
}

void test_math_should_be_right(void)
{
  allo_m4x4 t = allo_m4x4_translate((allo_vector){ 1, 2, 3 });
  allo_m4x4 t_theirs;
  mat4_identity(t_theirs.v);
  mat4_translate(t_theirs.v, t_theirs.v, &(mfloat_t[4]){1, 2, 3, 0});

  allo_m4x4 r = allo_m4x4_rotate(2, (allo_vector) { 0, 1, 0 });

  TEST_ASSERT_EQUAL(sizeof(t.v[0]), sizeof(mfloat_t));

  allo_m4x4 my_inv = allo_m4x4_inverse(r);
  allo_m4x4 their_inv;
  mat4_inverse(their_inv.v, r.v);
  test_matrices_equal(my_inv, their_inv);

  allo_m4x4 my_concat = allo_m4x4_concat(t, r);
  allo_m4x4 their_concat;
  mat4_multiply(their_concat.v, t.v, r.v);
  test_matrices_equal(my_concat, their_concat);
}

void setUp()
{

}

void tearDown()
{

}


int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_math_should_be_right);
  
  return UNITY_END();
}
