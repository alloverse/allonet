#define ALLO_INTERNALS 1
#include "animation.h"
#include <unordered_map>
using namespace Alloverse;
using namespace std;

struct DerivedProperty
{
    MathVariant current;
    PropertyAnimationUsage usage;
    void *act_on;
    string componentName;
};
static DerivedProperty animation_derive_property(Components *comps, const char *path);

static double _linear(double v) { return v; }
static unordered_map<string, AlloEasingFunction> g_easings = {
    {"linear", _linear},
    {"quadInOut", quadratic_ease_in_out},
    {"quadIn", quadratic_ease_in},
    {"quadOut", quadratic_ease_out},
    {"bounceInOut", bounce_ease_in_out},
    {"bounceIn", bounce_ease_in},
    {"bounceOut", bounce_ease_out},
    {"backInOut", back_ease_in_out},
    {"backIn", back_ease_in},
    {"backOut", back_ease_out},
    {"sineInOut", sine_ease_in_out},
    {"sineIn", sine_ease_in},
    {"sineOut", sine_ease_out},
    {"cubicInOut", cubic_ease_in_out},
    {"cubicIn", cubic_ease_in},
    {"cubicOut", cubic_ease_out},
    {"quartInOut", quartic_ease_in_out},
    {"quartIn", quartic_ease_in},
    {"quartOut", quartic_ease_out},
    {"quintInOut", quintic_ease_in_out},
    {"quintIn", quintic_ease_in},
    {"quintOut", quintic_ease_out},
    {"elasticInOut", elastic_ease_in_out},
    {"elasticIn", elastic_ease_in},
    {"elasticOut", elastic_ease_out},
    {"circularInOut", circular_ease_in_out},
    {"circularIn", circular_ease_in},
    {"circularOut", circular_ease_out},
    {"expInOut", exponential_ease_in_out},
    {"expIn", exponential_ease_in},
    {"expOut", exponential_ease_out}
};

AlloPropertyAnimation::AlloPropertyAnimation(const PropertyAnimation *spec)
{
    easingFunc = g_easings[spec->easing()->c_str()];
    from = mathvariant_from_flat(spec->from(), spec->from_type());
    to = mathvariant_from_flat(spec->to(), spec->to_type());
    if(from.type != to.type)
    {
        usage = UsageInvalid;
    }
    path = spec->path()->str();
}

MathVariant 
AlloPropertyAnimation::interpolateProperty(Alloverse::Components *comps, double fraction, bool swap)
{
    DerivedProperty derived = animation_derive_property(comps, path.c_str());
    
    // Figure out how to interpolate from `from` to `to`. Usage or current value doesn't matter yet,
    // so just go ahead and interpolate.
    MathVariant ret;
    if(from.type == TypeDouble)
    {
        double range = to.value.d - from.value.d;
        ret.type = TypeDouble;
        ret.value.d = from.value.d + fraction * range;
    }
    else if(from.type == TypeVec3)
    {
        ret.type = TypeVec3;
        vec3_lerp(ret.value.v.v, from.value.v.v, to.value.v.v, fraction);
    }
    else if(from.type == TypeRotation)
    {
        // this is extremely inefficient :S going from axis-angle to
        // quat to matrix to axis-angle :S
        // FIXME: also, rotations >180deg are folded back to <180deg? :(((
        mfloat_t fromq[4];
        mfloat_t toq[4];
        quat_from_axis_angle(fromq, from.value.r.axis.v, from.value.r.angle);
        quat_from_axis_angle(toq, to.value.r.axis.v, to.value.r.angle);
        mfloat_t retq[4];
        quat_slerp(retq, fromq, toq, fraction);
        allo_m4x4 retm;
        mat4_rotation_quat(retm.v, retq);

        ret.type = TypeRotation;
        ret.value.r =allo_m4x4_get_rotation(retm);
    }
    else if(from.type == TypeMat4)
    {
        ret.type = TypeMat4;
        ret.value.m = allo_m4x4_interpolate(from.value.m, to.value.m, fraction);
    }

    // Now, if usage is standard, we can use this value as-is.
    if(usage == UsageStandard)
    {
        return ret;
    }

    // But if it's not standard usage, the usage means we're modifying the 
    // current value with the interpolated value.
    MathVariant ret2 = derived.current;
    int ci = component_index;
    if(usage == UsageMatRot)
    {
        if(ci > 0)
        {
            allo_vector axis{ci==1?1.0:0.0, ci==2?1.0:0.0, ci==3?1.0:0.0};
            mat4_rotation_axis(ret2.value.m.v, axis.v, ret.value.d);
        }
        else
        {
            // replace with an axis-angle rotation...
            mat4_rotation_axis(ret2.value.m.v, ret.value.r.axis.v, ret.value.r.angle);
        }
        // but restore previous translation
        ret2.value.m.v[12] = current.value.m.v[12];
        ret2.value.m.v[13] = current.value.m.v[13];
        ret2.value.m.v[14] = current.value.m.v[14];
    }
    else if(usage == UsageMatTrans)
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
    else if(usage == UsageMatScale)
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



MathVariant mathvariant_from_flat(const void *flatfield, AnimationValue type)
{
    MathVariant ret;
    switch(type)
    {
    case AnimationValue_number:
        ret.type = TypeDouble;
        ret.value.d = static_cast<const NumberAnimationValue*>(flatfield)->number();
        break;
    case AnimationValue_vector: {
        auto vect = static_cast<const VectorAnimationValue*>(flatfield)->vector()->v();
        ret.type = TypeVec3;
        ret.value.v = allo_vector{vect->Get(1), vect->Get(2), vect->Get(3)};
        break; }
    case AnimationValue_matrix: {
        auto matt = static_cast<const TransformAnimationValue*>(flatfield)->matrix()->m();
        ret.type = TypeMat4;
        int i = 0;
        for(auto it = matt->begin(), end = matt->end(); it != end; it++, i++)
        {
            ret.value.m.v[i] = *it;
        }
        break; }
    case AnimationValue_rotation: {
        auto rott = static_cast<const RotationAnimationValue*>(flatfield);
        ret.type = TypeRotation;
        ret.value.r.angle = rott->angle();
        ret.value.r.axis = allo_vector{rott->axis()->v()->Get(0), rott->axis()->v()->Get(1), rott->axis()->v()->Get(2)};
        break; }
    }
    return ret;
}

void mathvariant_replace_json(MathVariant variant, cJSON *toreplace)
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

static bool variants_are_compatible(AlloPropertyAnimation *prop)
{
    MathVariant *src = &prop->from;
    MathVariant *dest = &prop->current;
    PropertyAnimationUsage usage = prop->usage;
    int idx = prop->component_index;

    if(usage == UsageStandard && src->type == dest->type) return true;
    if(usage >= UsageMatRot && idx > 0 && src->type == TypeDouble) return true;
    if(usage == UsageMatRot   && src->type == TypeRotation && dest->type == TypeMat4) return true;
    if(usage == UsageMatTrans && src->type == TypeVec3 && dest->type == TypeMat4) return true;
    if(usage == UsageMatScale && src->type == TypeVec3 && dest->type == TypeMat4) return true;
    return false;
}

// parse 'path' from prop->act_on into prop->current and prop->usage and prop->component_index.
// act_on, from and to must be set before calling.
static DerivedProperty animation_derive_property(Components *comps, const char *path)
{
    DerivedProperty derived;

    /*char this_prop[255] = {0};
    int i = 0;
    while(*path && *path != '.' && i<255) {
        this_prop[i++] = *path++;
    }
    if(*path != 0 && *path != '.')
    {
        // didn't land on null termination or dot separator?
        // probably property length overflow or just weird path :S ignore this prop.
        derived.usage = UsageInvalid;
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
    prop->current = mathvariant_from_json(prop->act_on);

    // if we found a value, and the current usage is undetermined, set it to standard.
    if(prop->current.type != TypeInvalid && prop->usage == UsageInvalid)
    {
        prop->usage = UsageStandard;
    }

    if(!prop->component)
    {
        // first key in keypath must be component name, so first act_on must be the component
        assert(prop->act_on);
        prop->component = prop->act_on;
    }

    // if path contains more keys, recurse
    if(*path == '.')
    {
        animation_derive_property(prop, path+1);
    }
    else
    {
        // finish up
        if(!variants_are_compatible(prop))
            prop->usage = UsageInvalid;
        if(prop->from.type == TypeInvalid || prop->to.type == TypeInvalid || prop->current.type == TypeInvalid)
            prop->usage = UsageInvalid;
    }*/
    return derived;
}
