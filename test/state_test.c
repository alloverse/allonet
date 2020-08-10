#include <unity.h>
#include <allonet/state.h>
#include "../src/util.h"
#include <stdlib.h>
#include <string.h>

static allo_state* state;
allo_entity* a, *aa, *b, *bb;

void test_allostate_should_ascendCoordinateSpaces(void)
{
  allo_m4x4 aat = entity_get_transform(aa);
  allo_vector aav = allo_m4x4_get_position(aat);
  TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.01, 7.0, aav.x, "local x coordinate wrong");
  // convert aa's position from a's coordinate space to world coordinate space
  allo_m4x4 aawt = state_convert_coordinate_space(state, aat, a, NULL);
  allo_vector aawv = allo_m4x4_get_position(aawt);
  // this should be a.x + aa.x since a is not rotated nor scaled
  TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.01, 8.0, aawv.x, "global x coordinate wrong");
}

void test_allostate_should_descendCoordinateSpaces(void)
{
  // jw is a point in world coordinate space
  allo_vector jw = { 1, 2, 3 };
  allo_m4x4 jwt = allo_m4x4_translate(jw);

  // converting jw into the coordinate space of b, calling it jb.
  allo_m4x4 jbt = state_convert_coordinate_space(state, jwt, NULL, b);
  allo_vector jbv = allo_m4x4_get_position(jbt);

  // this should be jw.x - b.x = 1.0 - 2.0 = -1.0, indicating that j is to the left of b's origin.
  TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.01, -1.0, jbv.x, "w in the coordinate space of b is wrong");
}

static cJSON* spec_located_at(float x, float y, float z)
{
  return cjson_create_object(
    "transform", cjson_create_object(
      "matrix", m2cjson(allo_m4x4_translate((allo_vector) { x, y, z })),
      NULL
    ),
    NULL
  );
}
static cJSON* spec_add_child(cJSON* spec, cJSON* childspec)
{
  cJSON *children = cJSON_GetObjectItem(spec, "children");
  if (children == NULL) {
    children = cJSON_CreateArray();
    cJSON_AddItemToObject(spec, "children", children);
  }
  cJSON_AddItemToArray(children, childspec);
  return spec;
}

static allo_entity* get_first_child(allo_state* state, allo_entity* ofParent)
{
  allo_entity* entity = NULL;
  LIST_FOREACH(entity, &state->entities, pointers)
  {
    cJSON* relationships = cJSON_GetObjectItem(entity->components, "relationships");
    cJSON* parent = cJSON_GetObjectItem(relationships, "parent");
    if (parent && parent->valuestring && strcmp(parent->valuestring, ofParent->id) == 0)
    {
      return entity;
    }
  }
  return NULL;
}

void setUp()
{
  state = calloc(1, sizeof(*state));
  LIST_INIT(&state->entities);

  a = allo_state_add_entity_from_spec(state, NULL, spec_add_child(
    spec_located_at(1, 3, 5),
    spec_located_at(7, 11, 13)
  ), NULL);
  aa = get_first_child(state, a);
  b = allo_state_add_entity_from_spec(state, NULL, spec_add_child(
    spec_located_at(2, 2, 2),
    spec_located_at(1, 3, 5)
  ), NULL);
  bb = get_first_child(state, b);
}

void tearDown()
{
  allo_entity *entity = state->entities.lh_first;
  while (entity)
  {
    allo_entity* to_delete = entity;
    entity = entity->pointers.le_next;
    entity_destroy(to_delete);
  }
  free(state);
}


int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_allostate_should_ascendCoordinateSpaces);
  RUN_TEST(test_allostate_should_descendCoordinateSpaces);

  return UNITY_END();
}
