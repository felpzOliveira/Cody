/* date = February 7th 2021 4:30 pm */

#ifndef QUERY_BAR_H
#define QUERY_BAR_H

#include <buffers.h>
#include <geometry.h>
#include <keyboard.h>

typedef struct{
    Geometry geometry;
    Buffer buffer;
    vec2ui cursor;
}QueryBar;

#endif //QUERY_BAR_H
