#version 450

// Input from the vertex shader
layout(location = 0) in vec2 frag_texcoord;

// Output color to the framebuffer
layout(location = 0) out vec4 FragColor;

// Combined image sampler for the texture
layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

void main() {
    // Sample the texture directly for the final pixel color
    FragColor = texture(tex_sampler, frag_texcoord);
}
