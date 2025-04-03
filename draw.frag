#version 450

#include "config.h"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform usampler2D s_trail;

const vec3 colors[] = vec3[COLOR_COUNT]
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
    const uint counts[] = uint[COLOR_COUNT]
    (
        GET_RED(trail),
        GET_GREEN(trail),
        GET_BLUE(trail),
        GET_WHITE(trail),
        GET_BLACK(trail),
        GET_MAGENTA(trail),
        GET_CYAN(trail),
        GET_YELLOW(trail)
    );
    uint count = 0;
    uint index = 0;
    for (uint i = 0; i < COLOR_COUNT; i++)
    {
        if (counts[i] > count)
        {
            count = counts[i];
            index = i;
        }
    }
    /* TODO: scale by the count */
    o_color = vec4(colors[index], 1.0f);
}