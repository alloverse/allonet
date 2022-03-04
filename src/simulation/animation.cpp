#define ALLO_INTERNALS 1

#include "animation.h"
#include "alloverse_generated.h"
#include <unordered_map>
using namespace Alloverse;
using namespace std;

static bool allosim_animate_process(SimulationCache *cache, Entity *entity, const PropertyAnimation *anim, double server_time, allo_state_diff *diff);



// perform all property animations specified in 'state' for where they should be at 'server_time'.
// Also, delete non-repeating animations that have progressed to completion come `server_time`.
void allosim_animate(allo_state *state, SimulationCache *cache, double server_time, allo_state_diff *diff)
{
    auto entities = state->_cur->mutable_entities();
    for(int i = 0, c = entities->size(); i < c; i++)
    {
        auto entity = entities->GetMutableObject(i);
        auto comp = entity->components()->property_animations();
        if(comp)
        {
            auto anims = comp->animations();
            for(int ai = 0, ac = anims->size(); ai < ac; ai++)
            {
                auto anim = anims->Get(ai);
                if(allosim_animate_process(cache, entity, anim, server_time, diff))
                {
                    cache->animations.erase(anim->id()->c_str());
                    // TODO! Remove this animation from world state too!
                }
            }
        }
    }
}

// animate a single property for a single entity. Return whether that particular animation
// has completed 100%.
static bool allosim_animate_process(SimulationCache *cache, allo_entity *entity, const PropertyAnimation *anim, double server_time, allo_state_diff *diff)
{
    auto stateiter = cache->animations.find(anim->id()->c_str());
    shared_ptr<AlloPropertyAnimation> animstate;
    if(stateiter != cache->animations.end())
    {
        // animation state already exists; use it and just process the next iteration.
        animstate = stateiter->second;
    }
    else
    {
        // validate the animation before trying to process it:
        if(anim->from_type() == AnimationValue_NONE || anim->from_type() != anim->to_type()) {
            return;
        }

        // ok it's good, create state for it!
        animstate = shared_ptr<AlloPropertyAnimation>(new AlloPropertyAnimation(anim));
        cache->animations[anim->id()->c_str()] = animstate;
    }

    // all the inputs
    auto path = anim->path()->str();
    double start_at = anim->start_at();
    double duration = anim->duration();
    const char *easingName = anim->easing() ? anim->easing()->c_str() : "linear";
    bool repeats = anim->repeats();
    bool autoreverses = anim->autoreverses();
    
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
    bool swap = false;
    if(repeats && autoreverses && iteration%2 == 1)
    {
        // if we invert progress, easing will also play in reverse. which might make sense...
        // but... I feel like, if you're bouncing an animation, you want the same easing in the
        // other direction? I might be wrong, in which case, swap the swap for the inversion.
        //progress = 1.0 - progress;
        swap = true;
    }
    // and ease the progress
    double eased_progress = _ease(progress, easingName);

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

