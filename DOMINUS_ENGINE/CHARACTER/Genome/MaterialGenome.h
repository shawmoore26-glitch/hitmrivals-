// CHARACTER/Genome/MaterialGenome.h
// "Material Reality Genome" -- state, not rendering. Describes what a
// material/object IS (type, age, wear, exposure), not how scratches or
// shaders should look. A future renderer decides that; this just
// stores the truth it would read from.
//
// `wear_state` is real, validated data -- but see MaterialWearDeriver
// (this same directory) for how it SHOULD be computed: derived from
// WORLD::WorldHistory's actual recorded events, not hand-set to
// whatever number looks right. A loaded MaterialGenome's wear_state is
// either author-set (for a freshly-created object with no history yet)
// or the last value a deriver computed -- this struct doesn't care
// which, same as CreatureGenome doesn't care whether its fields were
// hand-authored or eventually machine-assisted.
#pragma once

#include <string>

namespace dominus::character {

struct MaterialIdentity {
    std::string type;  // free-form: "urban_leather", "steel", "cloth", ...
};

struct MaterialProperties {
    int age_years = 0;             // >= 0
    float wear_state = 0.0f;       // 0-1 -- 0 = pristine, 1 = destroyed
    bool damage_history = false;   // has this object ever been damaged
    bool weather_exposure = false; // is this object exposed to weather
};

struct MaterialGenome {
    std::string material_id;  // required -- e.g. "MAT-JACKET-001"
    MaterialIdentity identity;
    MaterialProperties properties;
};

}  // namespace dominus::character
