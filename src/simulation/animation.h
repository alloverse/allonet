#include "simulation.h"
#include "mathc.h"

namespace Alloverse { 
    class Components;
    class PropertyAnimation;
}

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

typedef struct AlloPropertyAnimation
{
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

    AlloEasingFunction easing;

    AlloPropertyAnimation(const Alloverse::PropertyAnimation *spec);

    // given a prop with derived properties, figure out the MathVariant corresponding to the given fraction.
    // For example, for a from=value.double=5 and to=value.double=10 and fraction=0.5, return a value.double=7.5.
    void interpolate_property(Alloverse::Components *comps, double fraction);
} AlloPropertyAnimation;
