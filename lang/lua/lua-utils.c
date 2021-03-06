#include "lua-utils.h"
#include <string.h>

////// convenience functions

// useful debugging tool: https://stackoverflow.com/a/59097940
static void dumpstack (lua_State *L) {
  int top=lua_gettop(L);
  for (int i=1; i <= top; i++) {
    printf("%d\t%s\t", i, luaL_typename(L,i));
    switch (lua_type(L, i)) {
      case LUA_TNUMBER:
        printf("%g\n",lua_tonumber(L,i));
        break;
      case LUA_TSTRING:
        printf("%s\n",lua_tostring(L,i));
        break;
      case LUA_TBOOLEAN:
        printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
        break;
      case LUA_TNIL:
        printf("%s\n", "nil");
        break;
      default:
        printf("%p\n",lua_topointer(L,i));
        break;
    }
  }
}

/* assume that table is on the stack top */
char *get_table_string(lua_State *L, const char *key)
{
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    if (!lua_isstring(L, -1))
    {
        luaL_error(L, "Unexpected non-string value for key %s", key);
        return NULL;
    }
    char *result = strdup(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    return result;
}
void set_table_string(lua_State *L, const char *key, const char *value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}
void set_table_number(lua_State *L, const char *key, double value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}
double get_table_number(lua_State *L, const char *key)
{
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    if (!lua_isnumber(L, -1))
    {
        luaL_error(L, "Unexpected non-number value for key %s", key);
        return 0.0;
    }
    double result = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return result;
}
double get_table_inumber(lua_State *L, int key)
{
    lua_pushinteger(L, key);
    lua_gettable(L, -2);
    if (!lua_isnumber(L, -1))
    {
        luaL_error(L, "Unexpected non-number value for key %d", key);
        return 0.0;
    }
    double result = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return result; 
}

bool get_table_bool(lua_State *L, const char *key)
{
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    if (!lua_isboolean(L, -1))
    {
        dumpstack(L);
        luaL_error(L, "Unexpected non-bool value for key %s", key);
        return 0.0;
    }
    bool result = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return result; 
}

allo_client_intent* get_intent(lua_State *L)
{
    allo_client_intent *intent = allo_client_intent_create();
    intent->entity_id = get_table_string(L, "entity_id");
    intent->wants_stick_movement = get_table_bool(L, "wants_stick_movement");
    intent->zmovement = get_table_number(L, "zmovement");
    intent->xmovement = get_table_number(L, "xmovement");
    intent->yaw = get_table_number(L, "yaw");
    intent->pitch = get_table_number(L, "pitch");
    intent->poses = get_table_poses(L, "poses");
    return intent; 
}

allo_client_poses get_table_poses(lua_State *L, const char *key)
{
    allo_client_poses result;
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    result.head = get_table_head_pose(L, "head");
    result.torso = get_table_head_pose(L, "torso");
    result.root = get_table_head_pose(L, "root");
    result.left_hand = get_table_hand_pose(L, "hand/left");
    result.right_hand = get_table_hand_pose(L, "hand/right");
    lua_pop(L, 1);
    return result;
}
allo_client_hand_pose get_table_hand_pose(lua_State *L, const char *key)
{
  allo_client_hand_pose result;
  result.matrix = allo_m4x4_identity();
  for(int i = 0; i < ALLO_HAND_SKELETON_JOINT_COUNT; i++)
    result.skeleton[i] = allo_m4x4_identity();
  
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  if (!lua_isnil(L, -1))
  {
    result.matrix = get_table_matrix(L, "matrix");
    get_table_skeleton(L, "skeleton", result.skeleton);
    result.grab = get_table_grab(L, "grab");
  }
  lua_pop(L, 1);
  return result;
}

allo_client_plain_pose get_table_head_pose(lua_State *L, const char *key)
{
  allo_client_plain_pose result = {allo_m4x4_identity()};
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  if (!lua_isnil(L, -1))
  {
    result.matrix = get_table_matrix(L, "matrix");
  }
  lua_pop(L, 1);
  return result;
}

void get_table_skeleton(lua_State *L, const char *key, allo_m4x4 skeleton[ALLO_HAND_SKELETON_JOINT_COUNT])
{
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  if (!lua_isnil(L, -1))
  {
    for(int i = 0; i < ALLO_HAND_SKELETON_JOINT_COUNT; i++)
    {
      skeleton[i] = get_table_imatrix(L, i+1);
    }
  }
  lua_pop(L, 1);
}

allo_client_pose_grab get_table_grab(lua_State* L, const char* key)
{
  allo_client_pose_grab result;
  memset(&result, 0, sizeof(result));
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  if (!lua_isnil(L, -1))
  {
    result.entity = strdup(get_table_string(L, "entity"));
    result.grabber_from_entity_transform = get_table_matrix(L, "grabber_from_entity_transform");
  }
  lua_pop(L, 1);
  return result;
}
allo_vector get_table_vector(lua_State *L, const char *key)
{
    allo_vector result;
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    result.x = get_table_number(L, "x");
    result.y = get_table_number(L, "y");
    result.z = get_table_number(L, "z");
    lua_pop(L, 1);
    return result;
}
allo_m4x4 get_table_imatrix(lua_State* L, int idx)
{
  allo_m4x4 result;
	lua_pushinteger(L, idx);
	lua_gettable(L, -2);
	for (int i = 0; i < 16; i++) {
		result.v[i] = get_table_inumber(L, i+1);
	}
	lua_pop(L, 1);
	return result;
}
allo_m4x4 get_table_matrix(lua_State* L, const char* key)
{
	allo_m4x4 result;
	lua_pushstring(L, key);
	lua_gettable(L, -2);
	for (int i = 0; i < 16; i++) {
		result.v[i] = get_table_inumber(L, i+1);
	}
	lua_pop(L, 1);
	return result;
}

bool store_function(lua_State *L, int *storage)
{
    if(*storage) {
        luaL_unref(L, LUA_REGISTRYINDEX, *storage);
    }

    if(lua_type(L, -1) == LUA_TFUNCTION)
    {
        *storage = luaL_ref(L, LUA_REGISTRYINDEX);
        return true;
    }
    else if(lua_type(L, -1) == LUA_TNIL)
    {
        *storage = 0;
        return false;
    }
    else
    {
        luaL_error(L, "Invalid function");
        return false;
    }
}
bool get_function(lua_State *L, int storage)
{
    if(storage == 0)
    {
        return false;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, storage);
    if(lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void allua_push_cjson_value(lua_State *L, cJSON *cjvalue)
{
    switch(cjvalue->type) {
    case cJSON_False:
        lua_pushboolean(L, false);
        break;
    case cJSON_True:
        lua_pushboolean(L, true);
        break;
    case cJSON_NULL:
        lua_pushnil(L);
        break;
    case cJSON_Number:
        lua_pushnumber(L, cjvalue->valuedouble);
        break;
    case cJSON_String:
        lua_pushstring(L, cjvalue->valuestring);
        break;
    case cJSON_Array:
    {
        lua_newtable(L);
        int i = 1;
        for(struct cJSON *c = cjvalue->child; c != NULL; i++, c = c->next)
        {
            lua_pushinteger(L, i);
            allua_push_cjson_value(L, c);
            lua_settable(L, -3);
        }
        break;
    }
    case cJSON_Object:
        lua_newtable(L);
        for(struct cJSON *c = cjvalue->child; c != NULL; c = c->next)
        {
            lua_pushstring(L, c->string);
            allua_push_cjson_value(L, c);
            lua_settable(L, -3);
        }
    }
}

void push_interaction_table(lua_State *L, allo_interaction *inter)
{
    lua_newtable(L);
    set_table_string(L, "type", inter->type);
    set_table_string(L, "sender_entity_id", inter->sender_entity_id);
    set_table_string(L, "receiver_entity_id", inter->receiver_entity_id);
    set_table_string(L, "request_id", inter->request_id);
    set_table_string(L, "body", inter->body);
}

void push_state_table(lua_State *L, allo_state *state)
{
    lua_newtable(L); // state table
    set_table_number(L, "revision", state->revision);

    lua_pushstring(L, "entities");
    lua_newtable(L);
    allo_entity *entity = NULL;
    LIST_FOREACH(entity, &state->entities, pointers)
    {
        lua_pushstring(L, entity->id);
        lua_newtable(L); // entity description
            set_table_string(L, "id", entity->id);

            lua_pushstring(L, "components");
            allua_push_cjson_value(L, entity->components);
            lua_settable(L, -3);

        lua_settable(L, -3);
    }
    lua_settable(L, -3);
}

void push_matrix_table(lua_State *L, allo_m4x4 m)
{
    lua_newtable(L);
    for(int i = 0; i < 16; i++)
    {
        lua_pushnumber(L, m.v[i]);
        lua_rawseti(L, -2, i+1);
    }
}
