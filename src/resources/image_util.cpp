#include <image_util.h>
#include <utilities.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

uint8 *ImageUtils_LoadPixels(const char *path, int *width, int *height, int *cn){
    return stbi_load(path, width, height, cn, 0);
}

uint8 *ImageUtils_LoadPixels(uint8 *data, uint len, int *width, int *height, int *cn){
    return stbi_load_from_memory(data, len, width, height, cn, 0);
}

// TODO: Implement if we ever need this
vec3f *ImageUtils_LoadPixelsEXR(const char *path, int *width, int *height){
    return nullptr;
}

void ImageUtils_FreePixels(uint8 *pixels){
    if(pixels){
        stbi_image_free(pixels);
    }
}
