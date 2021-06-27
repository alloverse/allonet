#include "simulation.h"
#include "mathc.h"

static bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time);
static double _ease(double value, const char *easing);

void allosim_animate(allo_state *state, double server_time)
{
    allo_entity* entity = NULL;
    LIST_FOREACH(entity, &state->entities, pointers)
    {
        cJSON *comp = cJSON_GetObjectItemCaseSensitive(entity->components, "property_animations");
        if(comp)
        {
            cJSON *anims = cJSON_GetObjectItemCaseSensitive(comp, "animations");
            cJSON *anim = anims->child;
            while(anim) {
                cJSON *remove = NULL;
                if(allosim_animate_process(entity, anim, server_time))
                {
                    remove = anim;
                }
                anim = anim->next;
                if(remove)
                {
                    cJSON_Delete(cJSON_DetachItemViaPointer(anims, remove));
                }
            }
        }
    }
}

static bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time)
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
        // do one final update pass at exactly 1.0 and then delete the animation
        done = true;
        progress = 1.0;
    }
    else if(progress < 0.0)
    {
        // don't update any props until we're inside the animation's period
        return false;
    }

    double easedProgress = _ease(progress, easing);

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

static double _ease(double value, const char *easing)
{
         if(strcmp(easing, "linear") == 0)          return value;
    else if(strcmp(easing, "quadInOut") == 0)       return quadratic_ease_in_out(value);
    else if(strcmp(easing, "quadIn") == 0)          return quadratic_ease_in(value);
    else if(strcmp(easing, "quadOut") == 0)         return quadratic_ease_out(value);
    else if(strcmp(easing, "bounceInOut") == 0)     return bounce_ease_in_out(value);
    else if(strcmp(easing, "bounceIn") == 0)        return bounce_ease_in(value);
    else if(strcmp(easing, "bounceOut") == 0)       return bounce_ease_out(value);
    else if(strcmp(easing, "backInOut") == 0)       return back_ease_in_out(value);
    else if(strcmp(easing, "backIn") == 0)          return back_ease_in(value);
    else if(strcmp(easing, "backOut") == 0)         return back_ease_out(value);
    else if(strcmp(easing, "sineInOut") == 0)       return sine_ease_in_out(value);
    else if(strcmp(easing, "sineIn") == 0)          return sine_ease_in(value);
    else if(strcmp(easing, "sineOut") == 0)         return sine_ease_out(value);
    else if(strcmp(easing, "cubicInOut") == 0)      return cubic_ease_in_out(value);
    else if(strcmp(easing, "cubicIn") == 0)         return cubic_ease_in(value);
    else if(strcmp(easing, "cubicOut") == 0)        return cubic_ease_out(value);
    else if(strcmp(easing, "quartInOut") == 0)      return quartic_ease_in_out(value);
    else if(strcmp(easing, "quartIn") == 0)         return quartic_ease_in(value);
    else if(strcmp(easing, "quartOut") == 0)        return quartic_ease_out(value);
    else if(strcmp(easing, "quintInOut") == 0)      return quintic_ease_in_out(value);
    else if(strcmp(easing, "quintIn") == 0)         return quintic_ease_in(value);
    else if(strcmp(easing, "quintOut") == 0)        return quintic_ease_out(value);
    else if(strcmp(easing, "elasticInOut") == 0)    return elastic_ease_in_out(value);
    else if(strcmp(easing, "elasticIn") == 0)       return elastic_ease_in(value);
    else if(strcmp(easing, "elasticOut") == 0)      return elastic_ease_out(value);
    else if(strcmp(easing, "circularInOut") == 0)   return circular_ease_in_out(value);
    else if(strcmp(easing, "circularIn") == 0)      return circular_ease_in(value);
    else if(strcmp(easing, "circularOut") == 0)     return circular_ease_out(value);
    else if(strcmp(easing, "expInOut") == 0)        return exponential_ease_in_out(value);
    else if(strcmp(easing, "expIn") == 0)           return exponential_ease_in(value);
    else if(strcmp(easing, "expOut") == 0)          return exponential_ease_out(value);
    return value;
}
