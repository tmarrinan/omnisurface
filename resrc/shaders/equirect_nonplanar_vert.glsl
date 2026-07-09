#version 450

// Vertex inputs (attributes)
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;

// Outputs to the fragment shader
layout(location = 0) out vec2 frag_texcoord;

// Uniform buffer for transformations
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // MVP transformation
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(v_position, 1.0);
    
    // Pass UV coordinates to fragment shader
    frag_texcoord = v_texcoord;
}
