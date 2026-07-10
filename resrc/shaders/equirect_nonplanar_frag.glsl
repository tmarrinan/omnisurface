#version 450
#extension GL_EXT_multiview : enable

// Input from the vertex shader
layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) flat in int view_id;

// Output color to the framebuffer
layout(location = 0) out vec4 FragColor;

// Combined image sampler for the texture
layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

struct DisplaySurface {
    vec3 bottom_left;
    vec3 bottom_right;
    vec3 top_left;
    vec3 top_right;
};

#define PI 3.14159265359
#define TWO_PI 6.28318530718

void main() {
    // TEST - hardcode 3 planes on screen of 2048x1280
    int grid_columns = 3;
    vec2 region_size = vec2(683.0, 1280.0);
    //vec3 origin = vec3(0.0, 0.0, 0.0);
    vec3 camera = vec3(0.0, 1.0, 0.0);
    float delta_eye = 0.0325;
    DisplaySurface surfaces[3] = DisplaySurface[](
        DisplaySurface(vec3(-0.5, 0.0, 0.0), vec3(-0.5, 0.0, -1.0), vec3(-0.5, 1.875, 0.0), vec3(-0.5, 1.875, -1.0)),
        DisplaySurface(vec3(-0.5, 0.0, -1.0), vec3(0.5, 0.0, -1.0), vec3(-0.5, 1.875, -1.0), vec3(0.5, 1.875, -1.0)),
        DisplaySurface(vec3(0.5, 0.0, -1.0), vec3(0.5, 0.0, 0.0), vec3(0.5, 1.875, -1.0), vec3(0.5, 1.875, 0.0))
    );

    int col = int(gl_FragCoord.x / region_size.x);
    int row = int(gl_FragCoord.y / region_size.y);
    int index = row * grid_columns + col;

    float tx = mod(gl_FragCoord.x, region_size.x) / (region_size.x - 1.0); 
    float ty = mod(gl_FragCoord.y, region_size.y) / (region_size.y - 1.0);
    vec3 right = surfaces[index].bottom_right - surfaces[index].bottom_left;
    vec3 down = surfaces[index].bottom_left - surfaces[index].top_left;
    vec3 frag_pos_world = surfaces[index].top_left + tx * right + ty * down;
    vec3 frag_dir = normalize(frag_pos_world - camera);

    float theta = mod(atan(-frag_dir.x, frag_dir.z) + TWO_PI, TWO_PI);
    float phi = acos(frag_dir.y);

    vec2 uv = vec2(theta / TWO_PI, 0.5 * (phi / PI) + 0.5 * float(view_id));
    

    // Offset texture coordinate based on view layer (image is top/bottom stereo)
    //vec2 uv = vec2(frag_texcoord.x, 0.5 * frag_texcoord.y + 0.5 * float(view_id));

    // Sample the texture directly for the final pixel color
    FragColor = texture(tex_sampler, uv);
}
