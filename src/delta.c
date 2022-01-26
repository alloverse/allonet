#include "delta.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void _compute_full_diff(cJSON *latest, cJSON *delta, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo);
static void _compute_merge_diff(cJSON *latest, cJSON *current, cJSON *newstate, cJSON *delta, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo);

extern cJSON *statehistory_get(statehistory_t *history, int64_t revision)
{
    return history->history[revision%allo_statehistory_length];
}
extern cJSON *statehistory_get_latest(statehistory_t *history)
{
    return statehistory_get(history, history->latest_revision);
}

void allo_delta_insert(statehistory_t *history, cJSON *next_state)
{
    cJSON *revj = cJSON_GetObjectItemCaseSensitive(next_state, "revision");
    assert(revj);
    int64_t rev = cjson_get_int64_value(revj);
    cJSON *existing = history->history[rev%allo_statehistory_length];
    cJSON_Delete(existing);
    history->history[rev%allo_statehistory_length] = next_state;
    history->latest_revision = rev;
}
void allo_delta_clear(statehistory_t *history)
{
    for(int i = 0; i < allo_statehistory_length; i++)
    {
        cJSON_Delete(history->history[i]);
    }
    history->latest_revision = 0;
}

cJSON *allo_delta_compute_cjson(cJSON *latest, cJSON *old, int64_t old_revision)
{
    cJSON *mergePatch = cJSONUtils_GenerateMergePatch(old, latest);
    assert(mergePatch); // should never generate a COMPLETELY empty merge. Should at least have a new revision.
    cJSON_AddItemToObject(mergePatch, "patch_style", cJSON_CreateString("merge"));
    cJSON_AddItemToObject(mergePatch, "patch_from", cJSON_CreateNumber(old_revision));
    return mergePatch;
}

char *allo_delta_compute(statehistory_t *history, int64_t old_revision)
{
    cJSON *latest = statehistory_get_latest(history);
    assert(latest);
    cJSON *old = statehistory_get(history, old_revision);
    int64_t old_history_rev = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(old, "revision"));

    if(!old || old_history_rev != old_revision)
    {
        cJSON_AddItemToObject(latest, "patch_style", cJSON_CreateString("set"));
        char *deltas = cJSON_PrintUnformatted(latest);
        cJSON_DeleteItemFromObject(latest, "patch_style");
        return deltas;
    }
    
    cJSON *mergePatch = allo_delta_compute_cjson(latest, old, old_revision);
    char *patchs = cJSON_PrintUnformatted(mergePatch);
    cJSON_Delete(mergePatch);
    return patchs;
}

typedef enum { Set, Merge } PatchStyle;

cJSON *allo_delta_apply(statehistory_t *history, cJSON *delta, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo)
{
    cJSON *patch_stylej = cJSON_DetachItemFromObject(delta, "patch_style");
    const char *patch_styles = cJSON_GetStringValue(patch_stylej);
    assert(patch_styles);
    PatchStyle patch_style = Set;
    if(strcmp(patch_styles, "merge") == 0) patch_style = Merge;
    cJSON_Delete(patch_stylej);

    cJSON *patch_fromj = cJSON_DetachItemFromObject(delta, "patch_from");
    bool has_patch_from = patch_fromj != NULL;
    int64_t patch_from = cjson_get_int64_value(patch_fromj);
    assert(has_patch_from || patch_style == Set);
    cJSON_Delete(patch_fromj);
    if(!(has_patch_from || patch_style == Set))
    {
        // Invalid patch. If it's not Set, patch_from must be available.
        cJSON_Delete(delta);
        return NULL;
    }

    cJSON *current = statehistory_get(history, patch_from);
    cJSON *latest = statehistory_get_latest(history);
    int64_t current_rev = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(current, "revision"));

    if(has_patch_from && (patch_from != current_rev || current == NULL))
    {
        // patch is not from our rev; can't apply
        cJSON_Delete(delta);
        return NULL;
    }

    cJSON *result;
    switch(patch_style) {
        case Set:
            allo_statistics.ndelta_set++;
            if(diff)
                _compute_full_diff(latest, delta, diff, handler, userinfo);
            result = delta;
            break;
        case Merge:
            allo_statistics.ndelta_merge++;
            result = cJSON_Duplicate(current, 1);
            result = cJSONUtils_MergePatchCaseSensitive(result, delta);
            if(diff)
                _compute_merge_diff(latest, current, result, delta, diff, handler, userinfo);
            cJSON_Delete(delta);
            break;
    }
    allo_delta_insert(history, result);
    return result;
}

// if we don't have a server-side computed delta, we'll have to figure it out ourselves
static void _compute_full_diff(cJSON *latest, cJSON *newstate, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo)
{
    int64_t old_history_rev = cjson_get_int64_value(cJSON_GetObjectItemCaseSensitive(latest, "revision"));
    cJSON *delta = allo_delta_compute_cjson(newstate, latest, old_history_rev);
    _compute_merge_diff(latest, latest, newstate, delta, diff, handler, userinfo);
    cJSON_Delete(delta);
}

// if the server has already computed what has changed, we might as well use it.
static void _compute_merge_diff(cJSON *latest, cJSON *current, cJSON *newstate, cJSON *delta, allo_state_diff *diff, allo_statediff_handler handler, void *userinfo)
{
    if (latest != current)
    {
        // if the delta is from an older-than-previous revision, the diff will include things
        // that we have already covered with diff callbacks. This would be bad (e g calling new_entity or
        // new_component twice for the same entity/component), so we need to compute the diff
        // from the last 
        _compute_full_diff(latest, newstate, diff, handler, userinfo);
        return;
    }
    const cJSON *delta_entities = cJSON_GetObjectItemCaseSensitive(delta, "entities");
    const cJSON *existing_entities = cJSON_GetObjectItemCaseSensitive(current, "entities");
    const cJSON *newstate_entities = cJSON_GetObjectItemCaseSensitive(newstate, "entities");

    cJSON *edesc = NULL;
    cJSON_ArrayForEach(edesc, delta_entities)
    {
        const char *eid = edesc->string;
        const cJSON *existing_ent = cJSON_GetObjectItemCaseSensitive(existing_entities, eid);
        const cJSON *newstate_ent = cJSON_GetObjectItemCaseSensitive(newstate_entities, eid);
        
        if(cJSON_IsNull(edesc))
        {
            // deleted entity!
            arr_push(&diff->deleted_entities, eid);
            // all its components are gone now too
            const cJSON *existing_comps = cJSON_GetObjectItemCaseSensitive(existing_ent, "components");
            cJSON *cdesc = NULL;
            cJSON_ArrayForEach(cdesc, existing_comps)
            {
                const char *cname = cdesc->string;
                allo_component_ref ref = {eid, cname, cdesc};
                arr_push(&diff->deleted_components, ref);
            }
        }
        else if(!existing_ent)
        {
            // completely new entity!
            arr_push(&diff->new_entities, eid);
            // all of its components are new
            const cJSON *new_comps = cJSON_GetObjectItemCaseSensitive(edesc, "components");
            cJSON *cdesc = NULL;
            cJSON_ArrayForEach(cdesc, new_comps)
            {
                const char *cname = cdesc->string;
                allo_component_ref ref = {eid, cname, cdesc};
                arr_push(&diff->new_components, ref);
            }
        }
        else
        {
            // existing and updated entity!
            
            
            // let's figure out the state of the new comps
            const cJSON *existing_comps = cJSON_GetObjectItemCaseSensitive(existing_ent, "components");
            const cJSON *new_partial_comps = cJSON_GetObjectItemCaseSensitive(edesc, "components");
            const cJSON *new_comps = cJSON_GetObjectItemCaseSensitive(newstate_ent, "components");
            cJSON *partial_cdesc = NULL;
            cJSON_ArrayForEach(partial_cdesc, new_partial_comps)
            {
                const char *cname = partial_cdesc->string;
                const cJSON *existing_comp = cJSON_GetObjectItemCaseSensitive(existing_comps, cname);
                const cJSON *new_comp = cJSON_GetObjectItemCaseSensitive(new_comps, cname);
                allo_component_ref ref = {eid, cname, new_comp};
                if(cJSON_IsNull(partial_cdesc)) {
                    // it's a deleted comp!
                    ref.data = existing_comp;
                    arr_push(&diff->deleted_components, ref);
                }
                else if(!existing_comp)
                {
                    // it's a completely new comp!
                    arr_push(&diff->new_components, ref);
                }
                else
                {
                    // the comp has updated somehow.
                    arr_push(&diff->updated_components, ref);
                }
            }
        }

    }
    handler(userinfo, delta, newstate, diff);
}
