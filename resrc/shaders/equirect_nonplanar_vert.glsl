#version 450

// Outputs to the fragment shader
layout(location = 0) out vec2 frag_texcoord;

// Constant vertex locations and texcoords (full-screen quad)
const vec2 POSITIONS[6] = vec2[](
    vec2(-1.0, -1.0), vec2(-1.0,  1.0), vec2( 1.0, -1.0),
    vec2( 1.0, -1.0), vec2(-1.0,  1.0), vec2( 1.0,  1.0)
);
const vec2 TEXCOORDS[6] = vec2[](
    vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0),
    vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0)
);

void main() {
    // Set position based on vertex index
    gl_Position = vec4(POSITIONS[gl_VertexIndex], 0.0, 1.0);
    
    // Pass UV coordinates to fragment shader
    frag_texcoord = TEXCOORDS[gl_VertexIndex];
}
