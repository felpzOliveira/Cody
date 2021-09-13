#include <image_util.h>
#include <utilities.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

uint8 *ImageUtils_LoadPixels(const char *path, int *width, int *height, int *cn){
    uint8 *img = stbi_load(path, width, height, cn, 0);
    if(img == nullptr){
        printf("Failed to load [%s] : %s\n", path, stbi_failure_reason());
    }
    return img;
}

uint8 *ImageUtils_LoadPixels(uint8 *data, uint len, int *width, int *height, int *cn){
    uint8 *img = stbi_load_from_memory(data, len, width, height, cn, 0);
    if(img == nullptr){
        printf("Failed to load data : %s\n", stbi_failure_reason());
    }
    return img;
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
