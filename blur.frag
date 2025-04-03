#version 450

#include "config.h"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_trail;

void main()
{
    vec4 trail = vec4(0);
    const int kernel = 1;
    for (int x = -kernel; x <= kernel; x++)
    for (int y = -kernel; y <= kernel; y++)
    {
        const ivec2 coord = ivec2(gl_FragCoord.xy) + ivec2(x, y);
        trail += texelFetch(s_trail, coord, 0);
    }
    trail /= pow(kernel * 2 + 1, 2) * 1.0;
    vec4 start = texelFetch(s_trail, ivec2(gl_FragCoord), 0);
    trail = mix(trail, start, DIFFUSE_SPEED);
    trail = max(vec4(0), trail - EVAPORATE_SPEED);
    o_color = trail;
}