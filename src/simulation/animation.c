#include "simulation.h"
#include "mathc.h"

bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time);

void allosim_animate(allo_state *state, double server_time)
{
    allo_entity* entity = NULL;
    LIST_FOREACH(entity, &state->entities, pointers)
    {
        cJSON *comp = cJSON_GetObjectItemCaseSensitive(entity->components, "property_animations");
        if(comp)
        {
            cJSON *anims = cJSON_GetObjectItemCaseSensitive(comp, "animations");
            cJSON *anim = NULL;
            cJSON_ArrayForEach(anim, anims) {
                if(allosim_animate_process(entity, anim, server_time))
                {
                    // todo: remove the animation
                }
            }
        }
    }
}

bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time)
{
    const char *path = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "path"));
    cJSON *from = cJSON_GetObjectItemCaseSensitive(anim, "from");
    cJSON *to = cJSON_GetObjectItemCaseSensitive(anim, "to");
    double start_at = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "start_at"));
    double duration = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "duration"));
    const char *easing = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "easing")) ?: "linear";
    bool repeats = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(anim, "repeats"));
    bool done = false;

    double seconds_into = server_time - start_at;
    if(repeats)
    {
        seconds_into = fmod(seconds_into, duration);
    }
    double progress = seconds_into / duration;
    if(progress > 1.0)
    {
        done = true;
        progress = 1.0;
    }

    double easedProgress = progress;
         if(strcmp(easing, "quadInOut") == 0) easedProgress = quadratic_ease_in_out(progress);
    else if(strcmp(easing, "quadIn") == 0) easedProgress = quadratic_ease_in(progress);
    else if(strcmp(easing, "quadOut") == 0) easedProgress = quadratic_ease_out(progress);
    else if(strcmp(easing, "bounceInOut") == 0) easedProgress = bounce_ease_in_out(progress);
    else if(strcmp(easing, "bounceIn") == 0) easedProgress = bounce_ease_in(progress);
    else if(strcmp(easing, "bounceOut") == 0) easedProgress = bounce_ease_out(progress);
    // ... etc etc

    // todo: we can interpolate vectors too. but for now, assume double
    double fromd = cJSON_GetNumberValue(from);
    double tod = cJSON_GetNumberValue(to);
    double range = tod - fromd;
    double value = fromd + easedProgress * range;

    //printf("Animating %s: progress %f eased(%s) %f value %f\n", anim->string, progress, easing, easedProgress, value);


    if(strcmp(path, "transform.matrix.rotation.y") == 0)
    {
        allo_m4x4 mat = entity_get_transform(entity);
        mat4_rotation_y(mat.v, value);
        entity_set_transform(entity, mat);
    }
    // ':D yes, we'll generalize this later...


    return done;
}
