#version 450

// GPU Material Resource phase: real, minimal vertex shader for the
// material-resource pipeline -- position only. Color comes entirely
// from the bound uniform buffer descriptor (see
// dominus_material_resource.frag), so there is no per-vertex color
// attribute to pass through here, unlike dominus_triangle.vert.
layout(location = 0) in vec2 inPosition;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
