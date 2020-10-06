#include "delta.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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

// figure out which one is most efficient in practice...
static int64_t delta_count = 0;
static int64_t patch_count = 0;

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
        "patch_from", cJSON_CreateNumber(old_revision),
        NULL
    );
    char *deltas = cJSON_Print(delta);
    size_t deltasl = strlen(deltas);
    cJSON_Delete(delta);

    cJSON *mergePatch = cJSONUtils_GenerateMergePatch(old, latest);
    cJSON_AddItemToObject(mergePatch, "patch_style", cJSON_CreateString("merge"));
    cJSON_AddItemToObject(mergePatch, "patch_from", cJSON_CreateNumber(old_revision));
    char *patchs = cJSON_Print(mergePatch);
    size_t patchl = strlen(patchs);
    cJSON_Delete(mergePatch);

    if(deltasl < patchl)
    {
        delta_count++;
        free(patchs);
        return deltas;
    }
    else
    {
        patch_count++;
        free(deltas);
        return patchs;
    }
}

typedef enum { Set, Apply, Merge } PatchStyle;

bool allo_delta_apply(cJSON *current, cJSON *delta)
{
    cJSON *patch_stylej = cJSON_DetachItemFromObject(delta, "patch_style");
    const char *patch_styles = cJSON_GetStringValue(patch_stylej);
    assert(patch_styles);
    PatchStyle patch_style = Set;
    if(strcmp(patch_styles, "apply") == 0) patch_style = Apply;
    if(strcmp(patch_styles, "merge") == 0) patch_style = Merge;
    cJSON_Delete(patch_stylej);

    cJSON *patch_fromj = cJSON_DetachItemFromObject(delta, "patch_from");
    int64_t patch_from = cjson_get_int64_value(patch_fromj);
    int64_t current_rev = cjson_get_int64_value(cJSON_GetObjectItem(current, "revision"));

    if(patch_fromj && patch_from != current_rev)
    {
        // patch is not from our rev; can't apply
        return false;
    }

    cJSON *result;
    switch(patch_style) {
        case Set:
            cjson_clear(current);
            result = cJSONUtils_MergePatch(current, delta);
            assert(result == current); // or else it's a malformed pathc and we will leak memory
            return result == current;
        case Apply:
            return cJSONUtils_ApplyPatches(current, cJSON_GetObjectItem(delta, "patches")) == 0;
        case Merge:
            result = cJSONUtils_MergePatch(current, delta) == current;
            assert(result == current); // same
            return result == current;
    }
}