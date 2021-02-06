/* date = February 3rd 2021 3:45 pm */

#ifndef APP_H
#define APP_H
#include <bufferview.h>

void AppEarlyInitialize();
void AppInitialize();
BufferView *AppGetActiveBufferView();

void AppSetViewingGeometry(Geometry geometry, Float lineHeight);
BufferView *AppGetBufferView(int i);
int AppGetBufferViewCount();

#endif //APP_H
