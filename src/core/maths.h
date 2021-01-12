/* date = December 22nd 2020 7:05 pm */

#ifndef MATHS_H
#define MATHS_H
#include <types.h>

#define Float float
//#define Float double

template<typename T>
inline T Min(const T &a, const T &b){ return (a < b) ? a : b; }

template<typename T>
inline T Max(const T &a, const T &b){ return (a > b) ? a : b; }

inline Float Absf(Float v){ return v > 0 ? v : -v; }
inline bool IsZero(Float a){ return Absf(a) < 1e-8; }

#endif //MATHS_H
