#include "delta.h"
#include "util.h"
#include <assert.h>
#include <string.h>

void allo_delta_insert(statehistory_t *history, cJSON *next_state)
{
    cJSON *revj = cJSON_GetObjectItem(next_state, "revision");
    assert(revj);
    int64_t rev = cjson_get_int64_value(revj);
    cJSON *existing = history->history[rev%allo_statehistory_length];
    cJSON_Delete(existing);
    history->history[rev%allo_statehistory_length] = next_state;
    history->latest_revision = rev;
}
void allo_delta_destroy(statehistory_t *history)
{
    for(int i = 0; i++; i < allo_statehistory_length)
    {
        cJSON_Delete(history->history[i]);
    }
}

char *allo_delta_compute(statehistory_t *history, int64_t old_revision)
{
    cJSON *latest = history->history[history->latest_revision%allo_statehistory_length];
    assert(latest);
    cJSON *old = history->history[old_revision%allo_statehistory_length];
    if(!old)
    {
        cJSON_AddItemToObject(latest, "patch_style", cJSON_CreateString("set"));
        char *deltas = cJSON_Print(latest);
        cJSON_DeleteItemFromObject(latest, "patch_style");
        return deltas;
    }

    cJSON *patches = cJSONUtils_GeneratePatches(old, latest);
    cJSON *delta = cjson_create_object(
        "patches", patches,
        "patch_style", cJSON_CreateString("apply"),
        NULL
    );
    char *deltas = cJSON_Print(delta);
    cJSON_Delete(delta);
    return deltas;
}

typedef enum { Set, Apply } PatchStyle;

bool allo_delta_apply(cJSON *current, cJSON *delta)
{
    cJSON *patch_stylej = cJSON_DetachItemFromObject(delta, "patch_style");
    const char *patch_styles = cJSON_GetStringValue(patch_stylej);
    assert(patch_styles);
    PatchStyle patch_style = Set;
    if(strcmp(patch_styles, "apply") == 0) patch_style = Apply;
    cJSON_Delete(patch_stylej);

    switch(patch_style) {
        case Set:
            cjson_clear(current);
            cJSONUtils_MergePatch(current, delta);
            return true;
        case Apply:
            return cJSONUtils_ApplyPatches(current, cJSON_GetObjectItem(delta, "patches")) == 0;
    }
}
