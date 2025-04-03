#version 450

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform usampler2D s_trail;

const vec3 COLORS[] = vec3[8]
(
    vec3(1, 0, 0), /* RED */
    vec3(0, 1, 0), /* GREEN */
    vec3(0, 0, 1), /* BLUE */
    vec3(1, 1, 1), /* WHITE */
    vec3(0, 0, 0), /* BLACK */
    vec3(1, 0, 1), /* MAGENTA */
    vec3(0, 1, 1), /* CYAN */
    vec3(1, 1, 0)  /* YELLOW */
);

void main()
{
    const uvec4 trail = texture(s_trail, i_uv);
    const uint counts[] = uint[8]
    (
        (trail[0] >> 0) & 0xFFFF, /* RED */ 
        (trail[0] >> 16) & 0xFFFF, /* GREEN */ 
        (trail[1] >> 0) & 0xFFFF, /* BLUE */ 
        (trail[1] >> 16) & 0xFFFF, /* WHITE */ 
        (trail[2] >> 0) & 0xFFFF, /* BLACK */ 
        (trail[2] >> 16) & 0xFFFF, /* MAGENTA */ 
        (trail[3] >> 0) & 0xFFFF, /* CYAN */ 
        (trail[3] >> 16) & 0xFFFF /* YELLOW */ 
    );
    uint count = 0;
    uint index = 0;
    for (uint i = 0; i < 8; i++)
    {
        if (counts[i] > count)
        {
            index = 0;
            count = counts[i];
        }
    }
    /* TODO: scale by the count */
    o_color = vec4(COLORS[index], 0.2f);
}