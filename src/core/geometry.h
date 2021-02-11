/* date = December 21st 2020 7:06 pm */
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <math.h>
#include <cfloat>
#include <stdio.h>
#include <stdint.h>
#include <types.h>

#define __vec3_strfmtA(v) "%s = [%g %g %g]"
#define __vec3_strfmt(v) "[%g %g %g]"
#define __vec3_args(v) v.x, v.y, v.z
#define __vec3_argsA(v) #v, v.x, v.y, v.z
#define __vec2_strfmtA(v) "%s = [%g %g]"
#define __vec2_argsA(v) #v, v.x, v.y

#define v3fA(v) __vec3_strfmtA(v)
#define v3aA(v) __vec3_argsA(v)
#define v2fA(v) __vec2_strfmtA(v)
#define v2aA(v) __vec2_argsA(v)

#define Epsilon 0.0001f
#define Pi 3.14159265358979323846
#define InvPi 0.31830988618379067154
#define Inv2Pi 0.15915494309189533577
#define Inv4Pi 0.07957747154594766788
#define PiOver2 1.57079632679489661923
#define PiOver4 0.78539816339744830961
#define Sqrt2 1.41421356237309504880
#define MachineEpsilon 5.96046e-08

#define MIN_FLT -FLT_MAX
#define MAX_FLT  FLT_MAX
#define Infinity FLT_MAX
#define SqrtInfinity 3.1622776601683794E+18
#define IntInfinity 2147483646

inline Float Max(Float a, Float b){ return a < b ? b : a; }
inline Float Min(Float a, Float b){ return a < b ? a : b; }
inline Float Absf(Float v){ return v > 0 ? v : -v; }
inline bool IsNaN(Float v){ return v != v; }
inline Float Radians(Float deg) { return (Pi / 180) * deg; }
inline Float Degrees(Float rad) { return (rad * 180 / Pi); }
inline bool IsZero(Float a){ return Absf(a) < 1e-8; }
inline bool IsHighpZero(Float a) { return Absf(a) < 1e-22; }
inline bool IsUnsafeHit(Float a){ return Absf(a) < 1e-6; }
inline bool IsUnsafeZero(Float a){ return Absf(a) < 1e-6; }
inline Float Sign(Float a){
    if(IsZero(a)) return 0;
    if(a > 0) return 1;
    return -1;
}

inline Float LinearRemap(Float x, Float a, Float b, Float A, Float B){
    return A + (B - A) * ((x - a) / (b - a));
}

inline Float Log2(Float x){
    const Float invLog2 = 1.442695040888963387004650940071;
    return std::log(x) * invLog2;
}

inline void Swap(Float *a, Float *b){
    Float aux = *a; *a = *b; *b = aux;
}

inline void Swap(Float &a, Float &b){
    Float aux = a; a = b; b = aux;
}

inline bool Quadratic(Float a, Float b, Float c, Float *t0, Float *t1){
    double discr = (double)b * (double)b - 4 * (double)a * (double)c;
    if(discr < 0) return false;
    double root = std::sqrt(discr);
    double q;
    if(b < 0)
        q = -0.5 * (b - root);
    else
        q = -0.5 * (b + root);
    *t0 = q / a;
    *t1 = c / q;
    if(*t0 > *t1) Swap(t0, t1);
    return true;
}

inline int Log2Int(uint64_t n){
#define S(k) if (n >= (UINT64_C(1) << k)) { i += k; n >>= k; }
    int i = -(n == 0); S(32); S(16); S(8); S(4); S(2); S(1); return i;
#undef S
}

template <typename T, typename U, typename V> 
inline T Clamp(T val, U low, V high){
    if(val < low) return low;
    if(val > high) return high;
    return val;
}

template<typename T> class vec2{
    public:
    T x, y;
    
    vec2(){ x = y = (T)0; }
    vec2(T a){ x = y = a; }
    vec2(T a, T b): x(a), y(b){
        Assert(!HasNaN());
    }
    
    bool IsZeroVector() const{
        return IsZero(x) && IsZero(y);
    }
    
    bool HasNaN() const{
        return IsNaN(x) || IsNaN(y);
    }
    
    int Dimensions() const{ return 2; }
    
    T operator[](int i) const{
        Assert(i >= 0 && i < 2);
        if(i == 0) return x;
        return y;
    }
    
    T &operator[](int i){
        Assert(i >= 0 && i < 2);
        if(i == 0) return x;
        return y;
    }
    
    vec2<T> operator/(T f) const{
        Assert(!IsZero(f));
        Float inv = (Float)1 / f;
        return vec2<T>(x * inv, y * inv);
    }
    
    vec2<T> &operator/(T f){
        Assert(!IsZero(f));
        Float inv = (Float)1 / f;
        x *= inv; y *= inv;
        return *this;
    }
    
    vec2<T> operator-(){
        return vec2<T>(-x, -y);
    }
    
    vec2<T> operator-() const{
        return vec2<T>(-x, -y);
    }
    
    vec2<T> operator-(const vec2<T> &v) const{
        return vec2(x - v.x, y - v.y);
    }
    
    vec2<T> operator-(const vec2<T> &v){
        return vec2(x - v.x, y - v.y);
    }
    
    vec2<T> operator+(const vec2<T> &v) const{
        return vec2<T>(x + v.x, y + v.y);
    }
    
    vec2<T> &operator-=(const vec2<T> &v){
        x -= v.x; y -= v.y;
        return *this;
    }
    
    vec2<T> operator+=(const vec2<T> &v){
        x += v.x; y += v.y;
        return *this;
    }
    
    vec2<T> operator*(T s) const{
        return vec2<T>(x * s, y * s);
    }
    
    vec2<T> &operator*=(T s){
        x *= s; y *= s;
        return *this;
    }
    
    vec2<T> operator*(const vec2<T> &v) const{
        return vec2<T>(x * v.x, y * v.y);
    }
    
    vec2<T> &operator*=(const vec2<T> &v){
        x *= v.x; y *= v.y;
        return *this;
    }
    
    vec2<T> Rotate(Float radians) const{
        Float si = std::sin(radians);
        Float co = std::cos(radians);
        return vec2<T>(x * co - y * si, x * si + y * co);
    }
    
    Float LengthSquared() const{ return x * x + y * y; }
    Float Length() const{ return sqrt(LengthSquared()); }
    void PrintSelf() const{
        printf("P = {x : %g, y : %g}\n", x, y);
    }
};

template<typename T> class vec3{
    public:
    T x, y, z;
    vec3(){ x = y = z = (T)0; }
    vec3(T a){ x = y = z = a; }
    vec3(T a, T b, T c): x(a), y(b), z(c){
        Assert(!HasNaN());
    }
    
    bool IsZeroVector() const{
        return IsZero(x) && IsZero(y) && IsZero(z);
    }
    
    bool HasNaN(){
        return IsNaN(x) || IsNaN(y) || IsNaN(z);
    }
    
    bool HasNaN() const{
        return IsNaN(x) || IsNaN(y) || IsNaN(z);
    }
    
    int Dimensions() const{ return 3; }
    
    T operator[](int i) const{
        Assert(i >= 0 && i < 3);
        if(i == 0) return x;
        if(i == 1) return y;
        return z;
    }
    
    T &operator[](int i){
        Assert(i >= 0 && i < 3);
        if(i == 0) return x;
        if(i == 1) return y;
        return z;
    }
    
    vec3<T> operator/(T f) const{
        if(IsZero(f)){
            printf("Warning: Propagating error ( division by 0 with value: %g )\n", f);
        }
        Assert(!IsZero(f));
        Float inv = (Float)1 / f;
        return vec3<T>(x * inv, y * inv, z * inv);
    }
    
    vec3<T> &operator/(T f){
        Assert(!IsZero(f));
        if(IsZero(f)){
            printf("Warning: Propagating error ( division by 0 with value: %g )\n", f);
        }
        
        Float inv = (Float)1 / f;
        x *= inv; y *= inv; z *= inv;
        return *this;
    }
    
    vec3<T> operator/(const vec3<T> &v) const{
        Assert(!v.HasNaN());
        Float invx = (Float)1 / v.x;
        Float invy = (Float)1 / v.y;
        Float invz = (Float)1 / v.z;
        return vec3<T>(x * invx, y * invy, z * invz);
    }
    
    vec3<T> &operator/(const vec3<T> &v){
        Assert(!v.HasNaN());
        Float invx = (Float)1 / v.x;
        Float invy = (Float)1 / v.y;
        Float invz = (Float)1 / v.z;
        x = x * invx; y = y * invy; z = z * invz;
        return *this;
    }
    
    vec3<T> operator-(){
        return vec3<T>(-x, -y, -z);
    }
    
    vec3<T> operator-() const{
        return vec3<T>(-x, -y, -z);
    }
    
    vec3<T> operator-(const vec3<T> &v) const{
        return vec3(x - v.x, y - v.y, z - v.z);
    }
    
    vec3<T> &operator-=(const vec3<T> &v){
        x -= v.x; y -= v.y; z -= v.z;
        return *this;
    }
    
    vec3<T> operator+(const vec3<T> &v) const{
        return vec3<T>(x + v.x, y + v.y, z + v.z);
    }
    
    vec3<T> operator+=(const vec3<T> &v){
        x += v.x; y += v.y; z += v.z;
        return *this;
    }
    
    vec3<T> operator*(const vec3<T> &v) const{
        return vec3<T>(x * v.x, y * v.y, z * v.z);
    }
    
    vec3<T> &operator*=(const vec3<T> &v){
        x *= v.x; y *= v.y; z *= v.z;
        return *this;
    }
    
    vec3<T> operator*(T s) const{
        return vec3<T>(x * s, y * s, z * s);
    }
    
    vec3<T> &operator*=(T s){
        x *= s; y *= s; z *= s;
        return *this;
    }
    
    unsigned int ToUnsigned() const{
        unsigned int r = x;
        unsigned int g = y;
        unsigned int b = z;
        unsigned int a = 255;
        return (r) | (g << 8) | (b << 16) | (a << 24);
    }
    
    Float LengthSquared() const{ return x * x + y * y + z * z; }
    Float Length() const{ return sqrt(LengthSquared()); }
    void PrintSelf() const{
        printf("P = {x : %g, y :  %g, z : %g}\n", x, y, z);
    }
};

template<typename T> class vec4{
    public:
    T x, y, z, w;
    vec4(){ x = y = z = (T)0; }
    vec4(T a){ x = y = z = w = a; }
    vec4(T a, T b, T c, T d): x(a), y(b), z(c), w(d){
        Assert(!HasNaN());
    }
    
    bool HasNaN(){
        return IsNaN(x) || IsNaN(y) || IsNaN(z) || IsNaN(w);
    }
    
    bool HasNaN() const{
        return IsNaN(x) || IsNaN(y) || IsNaN(z) || IsNaN(w);
    }
    
    bool IsZeroVector() const{
        return IsZero(x) && IsZero(y) && IsZero(z) && IsZero(w);
    }
    
    T operator[](int i) const{
        Assert(i >= 0 && i < 4);
        if(i == 0) return x;
        if(i == 1) return y;
        if(i == 2) return z;
        return w;
    }
    
    T &operator[](int i){
        Assert(i >= 0 && i < 4);
        if(i == 0) return x;
        if(i == 1) return y;
        if(i == 2) return z;
        return w;
    }
    
    vec4<T> operator/(T f) const{
        Assert(!IsZero(f));
        if(IsZero(f)){
            printf("Warning: Propagating error ( division by 0 with value: %g )\n", f);
        }
        Float inv = (Float)1 / f;
        return vec4<T>(x * inv, y * inv, z * inv, w * inv);
    }
    
    vec4<T> &operator/(T f){
        Assert(!IsZero(f));
        if(IsZero(f)){
            printf("Warning: Propagating error ( division by 0 with value: %g )\n", f);
        }
        
        Float inv = (Float)1 / f;
        x *= inv; y *= inv; z *= inv; w *= inv;
        return *this;
    }
    
    vec4<T> operator/(const vec4<T> &v) const{
        Assert(!v.HasNaN());
        Float invx = (Float)1 / v.x;
        Float invy = (Float)1 / v.y;
        Float invz = (Float)1 / v.z;
        Float invw = (Float)1 / v.w;
        return vec4<T>(x * invx, y * invy, z * invz, w * invw);
    }
    
    vec4<T> &operator/(const vec4<T> &v){
        Assert(!v.HasNaN());
        Float invx = (Float)1 / v.x;
        Float invy = (Float)1 / v.y;
        Float invz = (Float)1 / v.z;
        Float invw = (Float)1 / v.w;
        x = x * invx; y = y * invy; z = z * invz; w *= invw;
        return *this;
    }
    
    vec4<T> operator-(){
        return vec4<T>(-x, -y, -z, -w);
    }
    
    vec4<T> operator-() const{
        return vec4<T>(-x, -y, -z, -w);
    }
    
    vec4<T> operator-(const vec4<T> &v) const{
        return vec4(x - v.x, y - v.y, z - v.z, w - v.w);
    }
    
    vec4<T> &operator-=(const vec4<T> &v){
        x -= v.x; y -= v.y; z -= v.z; w -= v.w;
        return *this;
    }
    
    vec4<T> operator+(const vec4<T> &v) const{
        return vec4<T>(x + v.x, y + v.y, z + v.z, w + v.w);
    }
    
    vec4<T> operator+=(const vec4<T> &v){
        x += v.x; y += v.y; z += v.z; w += v.w;
        return *this;
    }
    
    vec4<T> operator*(const vec4<T> &v) const{
        return vec4<T>(x * v.x, y * v.y, z * v.z, w * v.w);
    }
    
    vec4<T> &operator*=(const vec4<T> &v){
        x *= v.x; y *= v.y; z *= v.z; w *= v.w;
        return *this;
    }
    
    vec4<T> operator*(T s) const{
        return vec4<T>(x * s, y * s, z * s, w * s);
    }
    
    vec4<T> &operator*=(T s){
        x *= s; y *= s; z *= s; w *= s;
        return *this;
    }
    
    unsigned int ToUnsigned() const{
        unsigned int r = x;
        unsigned int g = y;
        unsigned int b = z;
        unsigned int a = w;
        return (r) | (g << 8) | (b << 16) | (a << 24);
    }
    
    Float LengthSquared() const{ return x * x + y * y + z * z + w * w; }
    Float Length() const{ return sqrt(LengthSquared()); }
    void PrintSelf() const{
        printf("P = {x : %g, y :  %g, z : %g, w : %g}\n", x, y, z, w);
    }
};

template<typename T>
inline bool HasZero(const vec2<T> &v){ 
    return (IsZero(v.x) || (IsZero(v.y)));
}

template<typename T>
inline bool HasZero(const vec3<T> &v){ 
    return (IsZero(v.x) || (IsZero(v.y)) || (IsZero(v.z)));
}

template<typename T>
inline vec2<T> Clamp(const vec2<T> &val, const vec2<T> &low, const vec2<T> &high){
    return vec2<T>(Clamp(val.x, low.x, high.x),
                   Clamp(val.y, low.y, high.y));
}

template<typename T>
inline vec3<T> Clamp(const vec3<T> &val, const vec3<T> &low, const vec3<T> &high){
    return vec3<T>(Clamp(val.x, low.x, high.x),
                   Clamp(val.y, low.y, high.y),
                   Clamp(val.z, low.z, high.z));
}

template<typename T>
inline vec4<T> Clamp(const vec4<T> &val, const vec4<T> &low, const vec4<T> &high){
    return vec4<T>(Clamp(val.x, low.x, high.x),
                   Clamp(val.y, low.y, high.y),
                   Clamp(val.z, low.z, high.z),
                   Clamp(val.w, low.w, high.w));
}

template<typename T>
inline vec3<T> Clamp(const vec3<T> &val){
    return Clamp(val, vec3<T>(-1), vec3<T>(1));
}

template<typename T>
inline vec2<T> Round(const vec2<T> &val){
    return vec2<T>(round(val.x), round(val.y));
}

template<typename T> inline vec2<T> operator*(T s, vec2<T> &v){ return v * s; }
template<typename T> inline vec3<T> operator*(T s, vec3<T> &v){ return v * s; }
template<typename T> inline vec4<T> operator*(T s, vec4<T> &v){ return v * s; }
template<typename T> inline vec2<T> Abs(const vec2<T> &v){
    return vec2<T>(Absf(v.x), Absf(v.y));
}

template <typename T, typename U> inline 
vec2<T> operator*(U s, const vec2<T> &v){
    return v * s;
}

template <typename T, typename U> inline 
vec3<T> operator*(U s, const vec3<T> &v){
    return v * s;
}

template <typename T, typename U> inline 
vec4<T> operator*(U s, const vec4<T> &v){
    return v * s;
}

template<typename T> inline vec3<T> Abs(const vec3<T> &v){
    return vec3<T>(Absf(v.x), Absf(v.y), Absf(v.z));
}

template<typename T> inline vec4<T> Abs(const vec4<T> &v){
    return vec4<T>(Absf(v.x), Absf(v.y), Absf(v.z), Absf(v.w));
}

template<typename T> inline T Dot(const vec2<T> &v1, const vec2<T> &v2){
    return v1.x * v2.x + v1.y * v2.y;
}

template<typename T> inline T Dot(const vec3<T> &v1, const vec3<T> &v2){
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

template<typename T> inline T Dot(const vec4<T> &v1, const vec4<T> &v2){
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

template<typename T> inline T Dot2(const vec2<T> &v1){
    return Dot(v1, v1);
}

template<typename T> inline T Dot2(const vec3<T> &v1){
    return Dot(v1, v1);
}

template<typename T> inline T Dot2(const vec4<T> &v1){
    return Dot(v1, v1);
}

template<typename T> inline T AbsDot(const vec3<T> &v1, const vec3<T> &v2){
    return Absf(Dot(v1, v2));
}

template<typename T> inline T AbsDot(const vec4<T> &v1, const vec4<T> &v2){
    return Absf(Dot(v1, v2));
}

template<typename T> inline T Cross(const vec2<T> &v1,
                                    const vec2<T> &v2)
{
    return v1.x * v2.y - v1.y * v2.x;
}

template<typename T> inline vec3<T> Cross(const vec3<T> &v1,
                                          const vec3<T> &v2)
{
    double v1x = v1.x, v1y = v1.y, v1z = v1.z;
    double v2x = v2.x, v2y = v2.y, v2z = v2.z;
    return vec3<T>((v1y * v2z) - (v1z * v2y),
                   (v1z * v2x) - (v1x * v2z),
                   (v1x * v2y) - (v1y * v2x));
}

template<typename T> inline vec2<T> Normalize(const vec2<T> &v){
    return v / v.Length();
}

template<typename T> inline vec3<T> Normalize(const vec3<T> &v){
    return v / v.Length();
}

template<typename T> inline vec4<T> Normalize(const vec4<T> &v){
    return v / v.Length();
}

inline Float Sin(const Float &v){
    return std::sin(v);
}

template<typename T> inline vec2<T> Sin(const vec2<T> &v){
    return vec2<T>(std::sin(v.x), std::sin(v.y));
}

template<typename T> inline vec3<T> Sin(const vec3<T> &v){
    return vec3<T>(std::sin(v.x), std::sin(v.y), std::sin(v.z));
}

template<typename T> inline vec4<T> Sin(const vec4<T> &v){
    return vec4<T>(std::sin(v.x), std::sin(v.y), std::sin(v.z), std::sin(v.w));
}

template<typename T> inline T MinComponent(const vec2<T> &v){
    return Min(v.x, v.y);
}

template<typename T> inline T MinComponent(const vec3<T> &v){
    return Min(v.x, Min(v.y, v.z));
}

template<typename T> inline T MaxComponent(const vec2<T> &v){
    return Max(v.x, v.y);
}

template<typename T> inline T MaxComponent(const vec3<T> &v){
    return Max(v.x, Max(v.y, v.z));
}

template<typename T> inline int MaxDimension(const vec3<T> &v){
    return (v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2);
}

template<typename T> inline int MinDimension(const vec3<T> &v){
    return (v.x < v.y) ? ((v.x < v.z) ? 0 : 2) : ((v.y < v.z) ? 1 : 2);
}

template<typename T> inline vec2<T> Min(const vec2<T> &v1, const vec2<T> &v2){
    return vec2<T>(Min(v1.x, v2.x), Min(v1.y, v2.y));
}

template<typename T> inline vec3<T> Min(const vec3<T> &v1, const vec3<T> &v2){
    return vec3<T>(Min(v1.x, v2.x), Min(v1.y, v2.y), Min(v1.z, v2.z));
}

template<typename T> inline vec4<T> Min(const vec4<T> &v1, const vec4<T> &v2){
    return vec4<T>(Min(v1.x, v2.x), Min(v1.y, v2.y), Min(v1.z, v2.z), Min(v1.w, v2.w));
}

template<typename T> inline vec2<T> Max(const vec2<T> &v1, const vec2<T> &v2){
    return vec2<T>(Max(v1.x, v2.x), Max(v1.y, v2.y));
}

template<typename T> inline vec3<T> Max(const vec3<T> &v1, const vec3<T> &v2){
    return vec3<T>(Max(v1.x, v2.x), Max(v1.y, v2.y), Max(v1.z, v2.z));
}

template<typename T> inline vec4<T> Max(const vec4<T> &v1, const vec4<T> &v2){
    return vec4<T>(Max(v1.x, v2.x), Max(v1.y, v2.y), Max(v1.z, v2.z), Max(v1.w, v2.w));
}

template<typename T> inline vec3<T> Permute(const vec3<T> &v, int x, int y, int z){
    return vec3<T>(v[x], v[y], v[z]);
}

template<typename T> inline 
vec3<T> Flip(const vec3<T> &p){ return vec3<T>(p.z, p.y, p.x); }

template<typename T> inline 
vec2<T> Flip(const vec2<T> &p){ return vec2<T>(p.y, p.x); }

template<typename T> inline void 
CoordinateSystem(const vec3<T> &v1, vec3<T> *v2, vec3<T> *v3){
    if(Absf(v1.x) > Absf(v1.y)){
        Float f = sqrt(v1.x * v1.x + v1.z * v1.z);
        AssertA(!IsZero(f), "Zero x component coordinate system generation");
        *v2 = vec3<T>(-v1.z, 0, v1.x) / f;
    }else{
        Float f = sqrt(v1.z * v1.z + v1.y * v1.y);
        AssertA(!IsZero(f), "Zero y component coordinate system generation");
        *v2 = vec3<T>(0, v1.z, -v1.y) / f;
    }
    
    *v3 = Cross(v1, *v2);
}

inline Float Sqrt(Float v){
    return std::sqrt(v);
}

template<typename T> inline
vec3<T> Sqrt(const vec3<T> &v){
    return vec3<T>(std::sqrt(v.x), std::sqrt(v.y), std::sqrt(v.z));
}

template<typename T> inline 
vec3<T> Pow(const vec3<T> &v, Float val){
    return vec3<T>(std::pow(v.x, val), std::pow(v.y, val), std::pow(v.z, val));
}

template<typename T> inline
vec3<T> Exp(const vec3<T> &v){
    return vec3<T>(std::exp(v.x), std::exp(v.y), std::exp(v.z));
}

typedef vec2<Float> vec2f;
typedef vec2<Float> Point2f;
typedef vec2<int> vec2i;
typedef vec2<int> Point2i;
typedef vec2<unsigned int> vec2ui;

typedef vec3<Float> vec3f;
typedef vec3<Float> Point3f;
typedef vec3<unsigned int> vec3ui;
typedef vec3<int> vec3i;
typedef vec3<int> Point3i;

typedef vec4<Float> vec4f;
typedef vec4<unsigned int> vec4ui;
typedef vec4<int> vec4i;

inline vec3f Max(vec3f a, vec3f b){
    return vec3f(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z));
}

inline vec3f Min(vec3f a, vec3f b){
    return vec3f(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z));
}

template<typename T> inline T Mix(const T &p0, const T &p1, T t){
    return (1 - t) * p0 + t * p1;
}

template<typename T, typename Q> inline T Lerp(const T &p0, const T &p1, Q t){
    return (1 - t) * p0 + t * p1;
}

inline Float Ceil(const Float &v){
    return std::ceil(v);
}

inline Float Floor(const Float &v){
    return std::floor(v);
}

template<typename T> inline vec2<T> Floor(const vec2<T> &v){
    return vec2<T>(std::floor(v.x), std::floor(v.y));
}

template<typename T> inline vec3<T> Floor(const vec3<T> &v){
    return vec3<T>(std::floor(v.x), std::floor(v.y), std::floor(v.z));
}

template<typename T> inline T Fract(T val){
    return val - Floor(val);
}

template <typename T> inline T Mod(T a, T b) {
    T result = a - (a / b) * b;
    return (T)((result < 0) ? result + b : result);
}

template <typename T>
class Bounds2 {
    public:
    vec2<T> pMin, pMax;
    
    Bounds2(){
        T minNum = FLT_MIN;
        T maxNum = FLT_MAX;
        pMin = vec2<T>(maxNum, maxNum);
        pMax = vec2<T>(minNum, minNum);
    }
    
    explicit Bounds2(const vec2<T> &p) : pMin(p), pMax(p) {}
    Bounds2(const vec2<T> &p1, const vec2<T> &p2)
        : pMin(Min(p1.x, p2.x), Min(p1.y, p2.y)), pMax(Max(p1.x, p2.x), Max(p1.y, p2.y)) {}
    
    const vec2<T> &operator[](int i) const;
    vec2<T> &operator[](int i);
    bool operator==(const Bounds2<T> &b) const{
        return b.pMin == pMin && b.pMax == pMax;
    }
    
    bool operator!=(const Bounds2<T> &b) const{
        return b.pMin != pMin || b.pMax != pMax;
    }
    
    vec2<T> Corner(int corner) const{
        Assert(corner >= 0 && corner < 4);
        return vec2<T>((*this)[(corner & 1)].x,
                       (*this)[(corner & 2) ? 1 : 0].y);
    }
    
    void Expand(Float d){
        pMin -= vec2<T>(Absf(d));
        pMax += vec2<T>(Absf(d));
    }
    
    void Reduce(Float d){
        pMin += vec2<T>(Absf(d));
        pMax -= vec2<T>(Absf(d));
    }
    
    T LengthAt(int i, int axis) const{
        Assert(axis == 0 || axis == 1);
        return (i == 0) ? pMin[axis] : pMax[axis];
    }
    
    vec2<T> Diagonal() const { return pMax - pMin; }
    T SurfaceArea() const{
        vec2<T> d = Diagonal();
        return (d.x * d.y);
    }
    
    T Volume() const{
        printf("Warning: Called for volume on 2D surface\n");
        return 0;
    }
    
    vec2<T> Center() const{
        return (pMin + pMax) * 0.5;
    }
    
    T ExtentOn(int i) const{
        Assert(i >= 0 && i < 2);
        if(i == 0) return Absf(pMax.x - pMin.x);
        return Absf(pMax.y - pMin.y);
    }
    
    int MaximumExtent() const{
        vec2<T> d = Diagonal();
        if (d.x > d.y)
            return 0;
        else
            return 1;
    }
    
    int MinimumExtent() const{
        vec2<T> d = Diagonal();
        if (d.x > d.y)
            return 1;
        else
            return 0;
    }
    
    vec2<T> Offset(const vec2<T> &p) const{
        vec2<T> o = p - pMin;
        if (pMax.x > pMin.x) o.x /= pMax.x - pMin.x;
        if (pMax.y > pMin.y) o.y /= pMax.y - pMin.y;
        return o;
    }
    
    void BoundingSphere(vec2<T> *center, Float *radius) const{
        *center = (pMin + pMax) / 2;
        *radius = Inside(*center, *this) ? Distance(*center, pMax) : 0;
    }
    
    vec2<T> MinDistance(const vec2<T> &p) const{
        Float x0 = Absf(pMin.x - p.x), x1 = Absf(pMax.x - p.x);
        Float y0 = Absf(pMin.y - p.y), y1 = Absf(pMax.y - p.y);
        return vec2<T>(Min(x0, x1), Min(y0, y1));
    }
    
    template <typename U> explicit operator Bounds2<U>() const{
        return Bounds2<U>((vec2<U>)pMin, (vec2<U>)pMax);
    }
    
    vec2<T> Clamped(const vec2<T> &point) const{
        return Clamp(point, pMin, pMax);
    }
    
    void PrintSelf() const{
        printf("pMin = {x : %g, y : %g} pMax = {x : %g, y : %g}\n",
               pMin.x, pMin.y, pMax.x, pMax.y);
    }
};

typedef Bounds2<Float> Bounds2f;
typedef Bounds2<int> Bounds2i;

template <typename T> inline 
vec2<T> &Bounds2<T>::operator[](int i){
    Assert(i == 0 || i == 1);
    return (i == 0) ? pMin : pMax;
}

template <typename T> inline
Bounds2<T> Union(const Bounds2<T> &b, const vec2<T> &p){
    Bounds2<T> ret;
    ret.pMin = Min(b.pMin, p);
    ret.pMax = Max(b.pMax, p);
    return ret;
}

template <typename T> inline
Bounds2<T> Union(const Bounds2<T> &b1, const Bounds2<T> &b2){
    Bounds2<T> ret;
    ret.pMin = Min(b1.pMin, b2.pMin);
    ret.pMax = Max(b1.pMax, b2.pMax);
    return ret;
}

template <typename T> inline 
Bounds2<T> Intersect(const Bounds2<T> &b1, const Bounds2<T> &b2){
    Bounds2<T> ret;
    ret.pMin = Max(b1.pMin, b2.pMin);
    ret.pMax = Min(b1.pMax, b2.pMax);
    return ret;
}

template <typename T> inline 
bool Overlaps(const Bounds2<T> &b1, const Bounds2<T> &b2){
    bool x = (b1.pMax.x >= b2.pMin.x) && (b1.pMin.x <= b2.pMax.x);
    bool y = (b1.pMax.y >= b2.pMin.y) && (b1.pMin.y <= b2.pMax.y);
    return (x && y);
}

template <typename T> inline
bool Inside(const vec2<T> &p, const Bounds2<T> &b){
    bool rv = (p.x >= b.pMin.x && p.x <= b.pMax.x && 
               p.y >= b.pMin.y && p.y <= b.pMax.y);
    if(!rv){
        vec2<T> oE = b.MinDistance(p);
        rv = IsUnsafeZero(oE.x) || IsUnsafeZero(oE.y);
    }
    
    return rv;
}

template <typename T> inline
bool InsideExclusive(const vec2<T> &p, const Bounds2<T> &b){
    return (p.x >= b.pMin.x && p.x < b.pMax.x && 
            p.y >= b.pMin.y && p.y < b.pMax.y);
}

template <typename T, typename U> inline 
Bounds2<T> Expand(const Bounds2<T> &b, U delta){
    return Bounds2<T>(b.pMin - vec2<T>(delta, delta),
                      b.pMax + vec2<T>(delta, delta));
}

typedef struct{
    vec2ui lower;
    vec2ui upper;
    vec2f extensionX;
    vec2f extensionY;
}Geometry;

typedef struct{
    vec2ui textPosition;
    vec2ui ghostPosition;
    int is_dirty;
    vec2ui nestStart;
    vec2ui nestEnd;
    int nestValid;
}DoubleCursor;

#endif //GEOMETRY_H
