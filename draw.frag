#version 450

#include "config.h"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_trail;

void main()
{
    const vec4 trail = texture(s_trail, i_uv);
    o_color.r = trail[COLOR_RED];
    o_color.g = trail[COLOR_GREEN];
    o_color.b = trail[COLOR_BLUE];
    o_color.a = 1.0f;
}