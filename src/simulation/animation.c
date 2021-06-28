#include "simulation.h"
#include "mathc.h"

static bool allosim_animate_process(allo_entity *entity, cJSON *anim, double server_time);
static double _ease(double value, const char *easing);

enum MathVariantType
{
    TypeInvalid,
    TypeDouble,
    TypeVec3,
    TypeRotation,
    TypeMat4
};

struct MathVariant
{
    enum MathVariantType type;
    union {
        double d;
        allo_vector v;
        allo_rotation r;
        allo_m4x4 m;
    } value;
};

static struct MathVariant json2variant(cJSON *json)
{
    struct MathVariant ret;
    if(cJSON_IsNumber(json))
    {
        ret.type = TypeDouble;
        ret.value.d = cJSON_GetNumberValue(json);
    }
    else if(cJSON_GetArraySize(json) == 3)
    {
        ret.type = TypeVec3;
        ret.value.v = cjson2vec(json);
    }
    else if(cJSON_GetArraySize(json) == 4)
    {
        ret.type = TypeRotation;
        ret.value.r = (allo_rotation){
            .angle = cJSON_GetNumberValue(cJSON_GetArrayItem(json, 0)),
            .axis = (allo_vector){
                .x = cJSON_GetNumberValue(cJSON_GetArrayItem(json, 1)),
                .y = cJSON_GetNumberValue(cJSON_GetArrayItem(json, 2)),
                .z = cJSON_GetNumberValue(cJSON_GetArrayItem(json, 3)),
            }
        };
    }
    else if(cJSON_GetArraySize(json) == 16)
    {
        ret.type = TypeMat4;
        ret.value.m = cjson2m(json);
    }
    else
    {
        ret.type = TypeInvalid;
    }
    return ret;
}

static void variant_replace_json(struct MathVariant variant, cJSON *toreplace)
{
    cJSON *child = toreplace->child;
    switch(variant.type)
    {
        case TypeDouble:
            toreplace->valuedouble = variant.value.d;
            break;
        case TypeRotation:
            child->valuedouble = variant.value.r.angle;
            child = child->next;
            /* FALLTHRU */
        case TypeVec3:
            for(int i = 0; i < 3 && child; i++, child = child->next)
            {
                child->valuedouble = variant.value.v.v[i];
            }
            break;
        case TypeMat4:
            for(int i = 0; i < 16 && child; i++, child = child->next)
            {
                child->valuedouble = variant.value.v.v[i];
            }
            break;
        default: break;
    }
}

// how should the values in the AnimationProperty be used to interpolate the value?
enum AnimationPropertyUsage
{
    // the entire animation property is invalid for some reason and cannot be used
    UsageInvalid,
    // please interpolate `from` and `to` and use the resulting value directly
    UsageStandard,
    // change only rotation, translation or scale in the target matrix value
    UsageMatRot,
    UsageMatTrans,
    UsageMatScale,
};

struct AnimationProperty
{
    // the cJSON value to change with this animation
    cJSON *act_on;
    // the parsed value out of act_on
    struct MathVariant current;
    // the parsed starting value
    struct MathVariant from;
    // the parsed ending value
    struct MathVariant to;
    // how to put from->to into current (see AnimationPropertyUsage)
    enum AnimationPropertyUsage usage;
    // if >0, UsageMat* will mean to only change x(1), y(2) or z(3) of rot, trans or scale
    int component_index;
};

static bool variants_are_compatible(struct AnimationProperty *prop)
{
    struct MathVariant *src = &prop->from;
    struct MathVariant *dest = &prop->current;
    enum AnimationPropertyUsage usage = prop->usage;
    int idx = prop->component_index;

    if(usage == UsageStandard && src->type == dest->type) return true;
    if(usage >= UsageMatRot && idx > 0 && src->type == TypeDouble) return true;
    if(usage == UsageMatRot   && src->type == TypeRotation && dest->type == TypeMat4) return true;
    if(usage == UsageMatTrans && src->type == TypeVec3 && dest->type == TypeMat4) return true;
    if(usage == UsageMatScale && src->type == TypeVec3 && dest->type == TypeMat4) return true;
    return false;
}

static void derive_property(struct AnimationProperty *prop, const char *path)
{
    char this_prop[255] = {0};
    int i = 0;
    while(*path && *path != '.' && i<255) {
        this_prop[i++] = *path++;
    }
    if(*path != 0 && *path != '.')
    {
        // didn't land on null termination or dot separator?
        // probably property length overflow or just weird path :S ignore this prop.
        prop->usage = UsageInvalid;
        return;
    }

    // find out how to apply this current key in the path
    int index;
    bool is_index = sscanf(this_prop, "%d", &index) == 1;
    if(is_index)
    {
        prop->act_on = cJSON_GetArrayItem(prop->act_on, index);
    }
    else if(prop->current.type == TypeMat4 && strcmp(this_prop, "rotation") == 0)
    {
        prop->usage = UsageMatRot;
    }
    else if(prop->current.type == TypeMat4 && strcmp(this_prop, "translation") == 0)
    {
        prop->usage = UsageMatTrans;
    }
    else if(prop->current.type == TypeMat4 && strcmp(this_prop, "scale") == 0)
    {
        prop->usage = UsageMatScale;
    }
    else if(prop->usage >= UsageMatRot && strcmp(this_prop, "x") == 0)
    {
        prop->component_index = 1;
    }
    else if(prop->usage >= UsageMatRot && strcmp(this_prop, "y") == 0)
    {
        prop->component_index = 2;
    }
    else if(prop->usage >= UsageMatRot && strcmp(this_prop, "z") == 0)
    {
        prop->component_index = 3;
    }
    else
    {
        prop->act_on = cJSON_GetObjectItemCaseSensitive(prop->act_on, this_prop);
    }

    // didn't find anything usable?
    if(!prop->act_on)
    {
        // missing property in ECS, don't do anything.
        prop->usage = UsageInvalid;
        return;
    }

    // parse what we've got in case we need it when we recurse
    prop->current = json2variant(prop->act_on);

    // if we found a value, and the current usage is undetermined, set it to standard.
    if(prop->current.type != TypeInvalid && prop->usage == UsageInvalid)
    {
        prop->usage = UsageStandard;
    }

    // if path contains more keys, recurse
    if(*path == '.')
    {
        derive_property(prop, path+1);
    }
    else
    {
        // finish up
        if(!variants_are_compatible(prop))
            prop->usage = UsageInvalid;
        if(prop->from.type == TypeInvalid || prop->to.type == TypeInvalid || prop->current.type == TypeInvalid)
            prop->usage = UsageInvalid;
    }
}

static struct MathVariant interpolate_property(struct AnimationProperty *prop, double fraction)
{
    // Figure out how to interpolate from `from` to `to`. Usage or current value doesn't matter yet,
    // so just go ahead and interpolate.
    struct MathVariant ret;
    if(prop->from.type == TypeDouble)
    {
        double range = prop->to.value.d - prop->from.value.d;
        ret.type = TypeDouble;
        ret.value.d = prop->from.value.d + fraction * range;
    }
    else if(prop->from.type == TypeVec3)
    {
        ret.type = TypeVec3;
        vec3_lerp(ret.value.v.v, prop->from.value.v.v, prop->to.value.v.v, fraction);
    }
    else if(prop->from.type == TypeRotation)
    {
        // this is extremely inefficient :S going from axis-angle to
        // quat to matrix to axis-angle :S
        // FIXME: also, rotations >180deg are folded back to <180deg? :(((
        mfloat_t fromq[4];
        mfloat_t toq[4];
        quat_from_axis_angle(fromq, prop->from.value.r.axis.v, prop->from.value.r.angle);
        quat_from_axis_angle(toq, prop->to.value.r.axis.v, prop->to.value.r.angle);
        mfloat_t retq[4];
        quat_slerp(retq, fromq, toq, fraction);
        allo_m4x4 retm;
        mat4_rotation_quat(retm.v, retq);

        ret.type = TypeRotation;
        ret.value.r =allo_m4x4_get_rotation(retm);
    }
    else if(prop->from.type == TypeMat4)
    {
        ret.type = TypeMat4;
        ret.value.m = allo_m4x4_interpolate(prop->from.value.m, prop->to.value.m, fraction);
    }

    // Now, if usage is standard, we can use this value as-is.
    if(prop->usage == UsageStandard)
    {
        return ret;
    }

    // But if it's not standard usage, the usage means we're modifying the 
    // current value with the interpolated value.
    struct MathVariant ret2 = prop->current;
    int ci = prop->component_index;
    if(prop->usage == UsageMatRot)
    {
        if(ci > 0)
        {
            allo_vector axis = {ci==1, ci==2, ci==3};
            mat4_rotation_axis(ret2.value.m.v, axis.v, ret.value.d);
        }
        else
        {
            // replace with an axis-angle rotation...
            mat4_rotation_axis(ret2.value.m.v, ret.value.r.axis.v, ret.value.r.angle);
        }
        // but restore previous translation
        ret2.value.m.v[12] = prop->current.value.m.v[12];
        ret2.value.m.v[13] = prop->current.value.m.v[13];
        ret2.value.m.v[14] = prop->current.value.m.v[14];
    }
    else if(prop->usage == UsageMatTrans)
    {
        if(ci > 0)
        {
            ret2.value.m.v[11+ci] = ret.value.d;
        }
        else
        {
            mat4_translation(ret2.value.m.v, ret2.value.m.v, ret.value.v.v);
        }
    }
    else if(prop->usage == UsageMatScale)
    {
        if(ci > 0)
        {
            ret2.value.m.v[(ci-1)*5] = ret.value.d;
        }
        else
        {
            mat4_scaling(ret2.value.m.v, ret2.value.m.v, ret.value.v.v);
        }
    }
    return ret2;
}

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
    // all the inputs
    const char *path = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "path"));
    cJSON *from = cJSON_GetObjectItemCaseSensitive(anim, "from");
    cJSON *to = cJSON_GetObjectItemCaseSensitive(anim, "to");
    double start_at = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "start_at"));
    double duration = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(anim, "duration"));
    const char *easing = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(anim, "easing"));
    if(!easing) easing = "linear"; 
    bool repeats = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(anim, "repeats"));
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
    // and ease the progress
    double eased_progress = _ease(progress, easing);

    // great. now figure out what values we're interpolating based on from, to and path.
    struct AnimationProperty prop = {0};
    prop.act_on = entity->components;
    prop.from = json2variant(from);
    prop.to = json2variant(to);
    if(prop.from.type != prop.to.type)
    {
        return false;
    }
    derive_property(&prop, path);
    if(prop.usage == UsageInvalid)
    {
        // something is off and we can't apply this animation
        return false;
    }

    // okay, go interpolate
    struct MathVariant new_value = interpolate_property(&prop, eased_progress);
    
    // apply the new value into state
    variant_replace_json(new_value, prop.act_on);

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
