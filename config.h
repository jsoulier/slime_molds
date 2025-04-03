#ifndef CONFIG_H
#define CONFIG_H

#define WIDTH 960
#define HEIGHT 720
#define THREADS_X 32
#define THREADS_Y 32

#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_BLUE 2
#define COLOR_WHITE 3
#define COLOR_BLACK 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_YELLOW 7
#define COLOR_COUNT 8

/*
 * We're taking a 4 component u32 texture and treating it as
 * an 8 component u16 texture to support more species.
 * TODO: Investigate if it's possible to extend to 16 species
 * where there's only 8 bits per species
 */
#define SET_COLOR(v, i, o, x) \
    (v[i] = (v[i] & ~(0xFFFF << o)) | (((x) & 0xFFFF) << o)) 
#define GET_COLOR(v, i, o) \
    ((v[i] >> o) & 0xFFFF)

#define SET_RED(v, x) SET_COLOR(v, 0, 0, x)
#define SET_GREEN(v, x) SET_COLOR(v, 0, 16, x)
#define SET_BLUE(v, x) SET_COLOR(v, 1, 0, x)
#define SET_WHITE(v, x) SET_COLOR(v, 1, 16, x)
#define SET_BLACK(v, x) SET_COLOR(v, 2, 0, x)
#define SET_MAGENTA(v, x) SET_COLOR(v, 2, 16, x)
#define SET_CYAN(v, x) SET_COLOR(v, 3, 0, x)
#define SET_YELLOW(v, x) SET_COLOR(v, 3, 16, x)

#define GET_RED(v) GET_COLOR(v, 0, 0)
#define GET_GREEN(v) GET_COLOR(v, 0, 16)
#define GET_BLUE(v) GET_COLOR(v, 1, 0)
#define GET_WHITE(v) GET_COLOR(v, 1, 16)
#define GET_BLACK(v) GET_COLOR(v, 2, 0)
#define GET_MAGENTA(v) GET_COLOR(v, 2, 16)
#define GET_CYAN(v) GET_COLOR(v, 3, 0)
#define GET_YELLOW(v) GET_COLOR(v, 3, 16)

#endif