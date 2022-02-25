#include "animation.h"

static bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time, allo_state_diff *diff);
static double _ease(double value, const char *easing);

// perform all property animations specified in 'state' for where they should be at 'server_time'.
// Also, delete non-repeating animations that have progressed to completion come `server_time`.
void allosim_animate(allo_state *state, double server_time, allo_state_diff *diff)
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
                if(allosim_animate_process(entity, anim, server_time, diff))
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

// animate a single property for a single entity. Return whether that particular animation
// has completed 100%.
static bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time, allo_state_diff *diff)
{
    // all the inputs
    const char *path = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "path"));
    cJSON *from = cJSON_GetObjectItemCaseSensitive(anim, "from");
    cJSON *to = cJSON_GetObjectItemCaseSensitive(anim, "to");
    double start_at = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "start_at"));
    double duration = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "duration"));
    const char *easing = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "easing"));
    if(!easing) easing = "linear"; 
    bool repeats = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(anim, "repeats"));
    bool autoreverses = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(anim, "autoreverses"));
    // state
    bool done = false;

    // figure out how far we've gotten into the animation
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
    // reverse every other iteration if requested
    int64_t iteration = (server_time-start_at)/duration;
    if(repeats && autoreverses && iteration%2 == 1)
    {
        // if we invert progress, easing will also play in reverse. which might make sense...
        // but... I feel like, if you're bouncing an animation, you want the same easing in the
        // other direction? I might be wrong, in which case, swap the swap for the inversion.
        //progress = 1.0 - progress;
        cJSON *swap = to;
        to = from;
        from = swap;
    }
    // and ease the progress
    double eased_progress = _ease(progress, easing);

    // great. now figure out what values we're interpolating based on from, to and path.
    // todo: this could be done only once per animation, which would save a LOT of work.
    AlloPropertyAnimation prop = propertyanimation_create(entity->components, from, to, path);
    if(prop.usage == UsageInvalid)
    {
        // something is off and we can't apply this animation right now.
        return false;
    }

    // okay, go interpolate
    MathVariant new_value = animation_interpolate_property(&prop, eased_progress);
    
    // apply the new value into state
    mathvariant_replace_json(new_value, prop.act_on);
    allo_state_diff_mark_component_updated(diff, entity->id, prop.component->string, prop.component);

    return done;
}

// todo: lookup table?
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
