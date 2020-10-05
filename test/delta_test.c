#include <unity.h>
#include <stdlib.h>
#include "../src/delta.h"
#include "../src/util.h"


static cJSON* spec_located_at(float x, float y, float z, float sz)
{
  return cjson_create_object(
    "transform", cjson_create_object(
      "matrix", m2cjson(allo_m4x4_translate((allo_vector) {{ x, y, z }})),
      NULL
    ),
    NULL
  );
}

void test_matrices_equal(allo_m4x4 a, allo_m4x4 b)
{
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
      0.001, a.v[i], b.v[i], "Index is wrong"
    );
  }
}

statehistory_t *history;
allo_state *state;
allo_entity *foo;
void setUp()
{
  history = calloc(1, sizeof(statehistory_t));
  state = calloc(1, sizeof(allo_state));
  allo_state_init(state);

  cJSON* root = spec_located_at(0, 0, 0, 0.3);
  foo = allo_state_add_entity_from_spec(state, NULL, root, NULL);
}
void tearDown()
{
  allo_delta_destroy(history);
  free(history);
  allo_state_destroy(state);
  free(state);
}

void test_basic(void)
{
  cJSON *first = allo_state_to_json(state);
  allo_delta_insert(history, first);
  printf("First state: %s\n", cJSON_Print(first));

  allo_m4x4 moved = allo_m4x4_translate((allo_vector){2, 3, 4});
  entity_set_transform(foo, moved);
  state->revision++;
  cJSON *second = allo_state_to_json(state);
  printf("Second state: %s\n", cJSON_Print(second));
  allo_delta_insert(history, second);

  char *delta = allo_delta_compute(history, 0);
  printf("delta %s\n", delta);
  cJSON *base = cJSON_Duplicate(first, 1);
  cJSON *patch = cJSON_Parse(delta);
  bool success = allo_delta_apply(base, patch);
  TEST_ASSERT_TRUE_MESSAGE(success, "expected applying patch to succeed");
  free(delta);
  TEST_ASSERT_TRUE_MESSAGE(cJSON_Compare(second, base, true), "expected patch to bring state up to speed");
  cJSON *entsj = cJSON_GetObjectItem(base, "entities");
  cJSON *fooj = cJSON_GetObjectItem(entsj, foo->id);
  cJSON *componentsj = cJSON_GetObjectItem(fooj, "components");
  cJSON *transformj = cJSON_GetObjectItem(componentsj, "transform");
  cJSON *matrix = cJSON_GetObjectItem(transformj, "matrix");
  allo_m4x4 moved2 = cjson2m(matrix);
  test_matrices_equal(moved, moved2);
}

int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_basic);

  return UNITY_END();
}
