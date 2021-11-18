#include "simulation.h"
#include "mathc.h"

typedef enum MathVariantType
{
    TypeInvalid,
    TypeDouble,
    TypeVec3,
    TypeRotation,
    TypeMat4
} MathVariantType;

typedef struct MathVariant
{
    MathVariantType type;
    union {
        double d;
        allo_vector v;
        allo_rotation r;
        allo_m4x4 m;
    } value;
} MathVariant;

extern struct MathVariant mathvariant_from_json(cJSON *json);
extern void mathvariant_replace_json(MathVariant variant, cJSON *toreplace);


// how should the values in the AlloPropertyAnimation be used to interpolate the value?
typedef enum PropertyAnimationUsage
{
    // the entire animation property is invalid for some reason and cannot be used
    UsageInvalid,
    // please interpolate `from` and `to` and use the resulting value directly
    UsageStandard,
    // change only rotation, translation or scale in the target matrix value
    UsageMatRot,
    UsageMatTrans,
    UsageMatScale,
} PropertyAnimationUsage;

typedef struct AlloPropertyAnimation
{
    // the cJSON value to change with this animation
    cJSON *act_on;
    // the parsed value out of act_on
    struct MathVariant current;
    // the parsed starting value
    struct MathVariant from;
    // the parsed ending value
    struct MathVariant to;
    // how to put from->to into current (see PropertyAnimationUsage)
    PropertyAnimationUsage usage;
    // if >0, UsageMat* will mean to only change x(1), y(2) or z(3) of rot, trans or scale
    int component_index;
} AlloPropertyAnimation;

// create a property animation with the given components to act on, generic value to animate from and to, and the
// keypath to the property that is to be animated.
extern AlloPropertyAnimation propertyanimation_create(cJSON *comps, cJSON *from, cJSON *to, const char *path);

// given a prop with derived properties, figure out the MathVariant corresponding to the given fraction.
// For example, for a from=value.double=5 and to=value.double=10 and fraction=0.5, return a value.double=7.5.
extern struct MathVariant animation_interpolate_property(AlloPropertyAnimation *prop, double fraction);
