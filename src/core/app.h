/* date = February 3rd 2021 3:45 pm */

#ifndef APP_H
#define APP_H
#include <geometry.h>

struct BufferView;

typedef struct{
    int tabSpacing;
}AppConfig;

extern AppConfig appGlobalConfig;

void AppEarlyInitialize();
void AppInitialize();
BufferView *AppGetActiveBufferView();
vec2ui AppActivateBufferViewAt(int x, int y);

void AppSetViewingGeometry(Geometry geometry, Float lineHeight);
void AppClickPosition(int x, int y);

BufferView *AppGetBufferView(int i);
int AppGetBufferViewCount();

#endif //APP_H
