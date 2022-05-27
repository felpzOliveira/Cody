/* date = April 18th 2021 11:59 */
#pragma once

#include <geometry.h>
#include <types.h>

/*
* Parses an image from already loaded data.
*/
uint8 *ImageUtils_LoadPixels(uint8 *data, uint len, int *width, int *height, int *channels);

/*
* Frees an image pixels. This is calling stbi frees. Since this does not
* map to the Allocator* macros it is better to give it to stbi.
*/
void ImageUtils_FreePixels(uint8 *pixels);
