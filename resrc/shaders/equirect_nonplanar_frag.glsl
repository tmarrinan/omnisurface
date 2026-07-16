#version 450
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_multiview : enable


struct DisplaySurface {
    int base_shape;
    vec3 raw_d1;
    vec3 raw_d2;
    vec3 raw_d3;
};

#define bottom_left raw_d1
#define bottom_right raw_d2
#define top_left raw_d3
#define radius raw_d1.x
#define altitude raw_d2
#define sector raw_d3

#define PI 3.14159265359
#define TWO_PI 6.28318530718


// Input from the vertex shader
layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) flat in int view_id;

// Output color to the framebuffer
layout(location = 0) out vec4 FragColor;

// Combined image sampler for the texture
layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

// Uniform display surfaces
layout(scalar, set = 0, binding = 1) uniform DisplayData {
    // virtual desktop screen layout
    ivec2 grid_dims;
    vec2 resolution;
    // physical display surface layout
    DisplaySurface surfaces[32];
} display_data;

// Push constants (updated each frame)
layout(push_constant) uniform PushConstants {
    vec2 rotation;
    // TODO: add camera?
} pcs;

void main() {
    vec3 camera = vec3(0.0, 1.0, 0.0); // TODO: handle eye separation
    
    // Calculate which region current fragment is on
    vec2 region_size = ceil(display_data.resolution / vec2(display_data.grid_dims));    
    int col = int(gl_FragCoord.x / region_size.x);
    int row = int(gl_FragCoord.y / region_size.y);
    int index = row * display_data.grid_dims.x + col;
    
    // Calculate normalized fragment position within display region
    float tx = mod(gl_FragCoord.x, region_size.x) / (region_size.x - 1.0); 
    float ty = mod(gl_FragCoord.y, region_size.y) / (region_size.y - 1.0);
    
    // Calculate 3D world position of fragment
    vec3 frag_pos_world = vec3(0.0, 0.0, 0.0);
    // PLANE
    if (display_data.surfaces[index].base_shape == 0) {
        vec3 right = display_data.surfaces[index].bottom_right - display_data.surfaces[index].bottom_left;
        vec3 down = display_data.surfaces[index].bottom_left - display_data.surfaces[index].top_left;
        frag_pos_world = display_data.surfaces[index].top_left + tx * right + ty * down;
    }
    // CYLINDER
    else if (display_data.surfaces[index].base_shape == 1) {
        float d_angle = mod(display_data.surfaces[index].sector.y - display_data.surfaces[index].sector.x, TWO_PI);
        float d_altitude = display_data.surfaces[index].altitude.y - display_data.surfaces[index].altitude.x;
        float angle = mod(display_data.surfaces[index].sector.x + tx * d_angle, TWO_PI);
        
        frag_pos_world.x = -display_data.surfaces[index].radius * cos(angle);
        frag_pos_world.y = display_data.surfaces[index].altitude.y - ty * d_altitude;
        frag_pos_world.z = -display_data.surfaces[index].radius * sin(angle);
    }
    
    // Calculate spherical direction of fragment
    vec3 frag_dir = normalize(frag_pos_world - camera);
    float cos_rx = cos(-pcs.rotation.x);
    float sin_rx = sin(-pcs.rotation.x);
    float cos_ry = cos(pcs.rotation.y);
    float sin_ry = sin(pcs.rotation.y);
    mat3 pitch = mat3(
        1.0,     0.0,    0.0,
        0.0,  cos_ry, sin_ry,
        0.0, -sin_ry, cos_ry
    );
    mat3 yaw = mat3(
        cos_rx, 0.0, -sin_rx,
           0.0, 1.0,     0.0,
        sin_rx, 0.0,  cos_rx
    );
    vec3 rotated_dir = yaw * (pitch * frag_dir);
    float theta = mod(atan(-rotated_dir.x, rotated_dir.z) , TWO_PI);
    float phi = acos(rotated_dir.y);
    
    // Convert spherical coords to normalized UV coords
    //  * offset Y-coordinate based on view layer (image is top/bottom stereo)
    vec2 uv = vec2(theta / TWO_PI, 0.5 * (phi / PI) + 0.5 * float(view_id));
    
    // Sample the texture directly for the final pixel color
    FragColor = texture(tex_sampler, uv);
}
