#include <unity.h>
#include <allonet/state.h>

void test_allostate_should_work(void)
{
  TEST_ASSERT_TRUE(false);
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

  RUN_TEST(test_allostate_should_work);

  return UNITY_END();
}