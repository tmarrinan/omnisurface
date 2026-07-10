#version 450
#extension GL_EXT_multiview : enable

// Input from the vertex shader
layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) flat in int view_id;

// Output color to the framebuffer
layout(location = 0) out vec4 FragColor;

// Combined image sampler for the texture
layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

void main() {
    // Offset texture coordinate based on view layer (image is top/bottom stereo)
    vec2 uv = vec2(frag_texcoord.x, 0.5 * frag_texcoord.y + 0.5 * float(view_id));

    // Sample the texture directly for the final pixel color
    FragColor = texture(tex_sampler, uv);
}
