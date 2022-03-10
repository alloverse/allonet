#include "simulation.h"
#include "mathc.h"
#include <string>
#include "alloverse_generated.h"

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
MathVariant mathvariant_from_flat(const void *flatfield, Alloverse::AnimationValue type);

typedef double (*AlloEasingFunction)(double);

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

struct AlloPropertyAnimation
{
    // the parsed value out of act_on
    struct MathVariant current;
    // the parsed starting value
    struct MathVariant from;
    // the parsed ending value
    struct MathVariant to;
    // how to put from->to into current (see PropertyAnimationUsage)
    PropertyAnimationUsage usage;
    // the path from the entity root to the property being animated
    std::string path;
    std::string component_name;
    // if >0, UsageMat* will mean to only change x(1), y(2) or z(3) of rot, trans or scale
    int component_index;
    // the easing to apply to the progress for interpolation from 'from' to 'to'
    AlloEasingFunction easingFunc;

    AlloPropertyAnimation(const Alloverse::PropertyAnimation *spec);

    // given a prop with derived properties, figure out the MathVariant corresponding to the given fraction.
    // For example, for a from=value.double=5 and to=value.double=10 and fraction=0.5, return a value.double=7.5.
    MathVariant interpolateProperty(Alloverse::Components *comps, double fraction, bool swap);


};
