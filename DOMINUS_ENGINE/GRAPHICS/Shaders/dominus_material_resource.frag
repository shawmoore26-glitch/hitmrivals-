#version 450

// GPU Material Resource phase: real, minimal fragment shader consuming
// a real, bound uniform buffer descriptor -- not vertex color. Kept
// deliberately separate from dominus_triangle.frag (used by the
// existing, checkpoint-verified render path, which is completely
// unmodified by this phase) so this new path carries zero risk to
// existing pixel output.
layout(binding = 0) uniform MaterialResourceUBO {
    vec4 color;  // real, authoritative MaterialContract-resolved color
} materialResource;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = materialResource.color;
}
