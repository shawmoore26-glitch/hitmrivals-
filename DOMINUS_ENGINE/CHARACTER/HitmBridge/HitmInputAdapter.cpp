// CHARACTER/HitmBridge/HitmInputAdapter.cpp
#include "CHARACTER/HitmBridge/HitmInputAdapter.h"

namespace dominus::character::hitm {

HitmInputCommand TranslateRawInput(const HitmRawInputState& raw) {
    // See HitmInputAdapter.h's own header comment for the full,
    // evidence-grounded derivation of this exact order.
    if (raw.block) return HitmInputCommand::kBlock;
    if (raw.special || raw.attack) return HitmInputCommand::kSpecial;
    if (raw.up) return HitmInputCommand::kJump;
    if (raw.left && !raw.right) return HitmInputCommand::kLeft;
    if (raw.right && !raw.left) return HitmInputCommand::kRight;
    return HitmInputCommand::kNeutral;
}

HitmRawInputState ReadRawInput(const HitmButtonBinding& binding, const std::function<bool(int)>& isPressed) {
    HitmRawInputState raw;
    raw.left = isPressed(binding.left);
    raw.right = isPressed(binding.right);
    raw.up = isPressed(binding.up);
    raw.attack = isPressed(binding.attack);
    raw.block = isPressed(binding.block);
    raw.special = isPressed(binding.special);
    return raw;
}

}  // namespace dominus::character::hitm
