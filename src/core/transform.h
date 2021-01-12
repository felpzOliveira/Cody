#pragma once

#include <geometry.h>

struct Matrix3x3{
    Float m[3][3];
    
    Matrix3x3(){
        m[0][0] = m[1][1] = m[2][2] = 1.f;
        m[0][1] = m[1][0] = m[0][2] = 0.f;
        m[1][0] = m[1][2] = 0.f;
        m[2][0] = m[2][1] = 0.f;
    }
    
    Matrix3x3(Float mat[3][3]){
        m[0][0] = mat[0][0]; m[1][0] = mat[1][0]; m[2][0] = mat[2][0];
        m[0][1] = mat[0][1]; m[1][1] = mat[1][1]; m[2][1] = mat[2][1];
        m[0][2] = mat[0][2]; m[1][2] = mat[1][2]; m[2][2] = mat[2][2];
    }
    
    Matrix3x3(Float t00, Float t01, Float t02, 
              Float t10, Float t11, Float t12,
              Float t20, Float t21, Float t22)
    {
        m[0][0] = t00; m[0][1] = t01; m[0][2] = t02;
        m[1][0] = t10; m[1][1] = t11; m[1][2] = t12;
        m[2][0] = t20; m[2][1] = t21; m[2][2] = t22;
    }
    
    friend Matrix3x3 Transpose(const Matrix3x3 &o){
        return Matrix3x3(o.m[0][0], o.m[1][0], o.m[2][0],
                         o.m[0][1], o.m[1][1], o.m[2][1],
                         o.m[0][2], o.m[1][2], o.m[2][2]);
    }
    
    static Matrix3x3 Mul(const Matrix3x3 &m1, const Matrix3x3 &m2){
        Matrix3x3 r;
        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
            r.m[i][j] = m1.m[i][0]*m2.m[0][j]+m1.m[i][1]*m2.m[1][j]+m1.m[i][2]*m2.m[2][j];
        return r;
    }
    
    friend Matrix3x3 Inverse(const Matrix3x3 &o){
        Float det = (o.m[0][0] * (o.m[1][1] * o.m[2][2] - o.m[1][2] * o.m[2][1]) -
                     o.m[0][1] * (o.m[1][0] * o.m[2][2] - o.m[1][2] * o.m[2][0]) +
                     o.m[0][2] * (o.m[1][0] * o.m[2][1] - o.m[1][1] * o.m[2][0]));
        
        AssertA(!IsZero(det), "Zero determinant on matrix inverse");
        Float invDet = 1.f / det;
        Float a00 = (o.m[1][1] * o.m[2][2] - o.m[2][1] * o.m[1][2]) * invDet;
        Float a01 = (o.m[0][2] * o.m[2][1] - o.m[2][2] * o.m[0][1]) * invDet;
        Float a02 = (o.m[0][1] * o.m[1][2] - o.m[1][1] * o.m[0][2]) * invDet;
        Float a10 = (o.m[1][2] * o.m[2][0] - o.m[2][2] * o.m[1][0]) * invDet;
        Float a11 = (o.m[0][0] * o.m[2][2] - o.m[2][0] * o.m[0][2]) * invDet;
        Float a12 = (o.m[0][2] * o.m[1][0] - o.m[1][2] * o.m[0][0]) * invDet;
        Float a20 = (o.m[1][0] * o.m[2][1] - o.m[2][0] * o.m[1][1]) * invDet;
        Float a21 = (o.m[0][1] * o.m[2][0] - o.m[2][1] * o.m[0][0]) * invDet;
        Float a22 = (o.m[0][0] * o.m[1][1] - o.m[1][0] * o.m[0][1]) * invDet;
        return Matrix3x3(a00, a01, a02, a10, a11, a12, a20, a21, a22);
    }
    
    void PrintSelf(){
        for(int i = 0; i < 3; i++){
            printf("[ ");
            for(int j = 0; j < 3; j++){
                printf("%g ", m[i][j]);
            }
            
            printf("]\n");
        }
    }
};

struct Matrix4x4{
    Float m[4][4];
    
    Matrix4x4(){
        m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.f;
        m[0][1] = m[0][2] = m[0][3] = m[1][0] = m[1][2] = m[1][3] = m[2][0] =
            m[2][1] = m[2][3] = m[3][0] = m[3][1] = m[3][2] = 0.f;
    }
    
    Matrix4x4(Float mat[4][4]);
    Matrix4x4(Float t00, Float t01, Float t02, Float t03, Float t10, Float t11,
              Float t12, Float t13, Float t20, Float t21, Float t22, Float t23,
              Float t30, Float t31, Float t32, Float t33);
    
    friend Matrix4x4 Transpose(const Matrix4x4 &);
    
    static Matrix4x4 Mul(const Matrix4x4 &m1, const Matrix4x4 &m2) {
        Matrix4x4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
            r.m[i][j] = m1.m[i][0] * m2.m[0][j] + m1.m[i][1] * m2.m[1][j] +
            m1.m[i][2] * m2.m[2][j] + m1.m[i][3] * m2.m[3][j];
        return r;
    }
    
    friend Matrix4x4 Inverse(const Matrix4x4 &);
    
    void PrintSelf(){
        for(int i = 0; i < 4; ++i){
            printf("[ ");
            for(int j = 0; j < 4; ++j){
                printf("%g  ", m[i][j]);
            }
            
            printf("]\n");
        }
    }
};

class Transform2{
    public:
    Matrix3x3 m, mInv;
    Transform2(){}
    Transform2(const Matrix3x3 &m) : m(m), mInv(Inverse(m)){}
    Transform2(const Matrix3x3 &mat, const Matrix3x3 &inv): m(mat), mInv(inv){}
    friend Transform2 Inverse(const Transform2 &t){
        return Transform2(t.mInv, t.m);
    }
    
    vec2f Vector(const vec2f &p) const{
        Float x = p.x, y = p.y;
        Float xp = m.m[0][0] * x + m.m[0][1] * y;
        Float yp = m.m[1][0] * x + m.m[1][1] * y;
        return vec2f(xp, yp);
    }
    
    vec2f Point(const vec2f &p) const{
        Float x = p.x, y = p.y;
        Float xp = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2];
        Float yp = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2];
        Float wp = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2];
        
        if(IsZero(wp - 1.)){
            return vec2f(xp, yp);
        }else{
            AssertA(!IsZero(wp), "Invalid homogeneous coordinate normalization");
            return vec2f(xp, yp) / wp;
        }
    }
};

Transform2 Scale2(Float u);
Transform2 Scale2(Float x, Float y);
Transform2 Translate2(Float u);
Transform2 Translate2(Float x, Float y);

class Transform{
    public:
    Matrix4x4 m, mInv;
    // Transform Public Methods
    Transform() {}
    Transform(const Float mat[4][4]){
        m = Matrix4x4(mat[0][0], mat[0][1], mat[0][2], mat[0][3], mat[1][0],
                      mat[1][1], mat[1][2], mat[1][3], mat[2][0], mat[2][1],
                      mat[2][2], mat[2][3], mat[3][0], mat[3][1], mat[3][2],
                      mat[3][3]);
        mInv = Inverse(m);
    }
    
    Transform(const Matrix4x4 &m) : m(m), mInv(Inverse(m)) {}
    Transform(const Matrix4x4 &m, const Matrix4x4 &mInv) : m(m), mInv(mInv) {}
    
    friend Transform Inverse(const Transform &t) {
        return Transform(t.mInv, t.m);
    }
    
    friend Transform Transpose(const Transform &t) {
        return Transform(Transpose(t.m), Transpose(t.mInv));
    }
    
    template<typename T> vec3<T> Vector(const vec3<T> &v) const{
        T x = v.x, y = v.y, z = v.z;
        return vec3<T>(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
                       m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
                       m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
    }
    
    template<typename T> vec3<T> Point(const vec3<T> &p) const{
        T x = p.x, y = p.y, z = p.z;
        T xp = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3];
        T yp = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3];
        T zp = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3];
        T wp = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
        if (wp == 1)
            return vec3<T>(xp, yp, zp);
        else{
            AssertAEx(!IsZero(wp), "Zero transform wp [Point3f]");
            return vec3<T>(xp, yp, zp) / wp;
        }
    }
    
    bool IsIdentity() const {
        return (m.m[0][0] == 1.f && m.m[0][1] == 0.f && m.m[0][2] == 0.f &&
                m.m[0][3] == 0.f && m.m[1][0] == 0.f && m.m[1][1] == 1.f &&
                m.m[1][2] == 0.f && m.m[1][3] == 0.f && m.m[2][0] == 0.f &&
                m.m[2][1] == 0.f && m.m[2][2] == 1.f && m.m[2][3] == 0.f &&
                m.m[3][0] == 0.f && m.m[3][1] == 0.f && m.m[3][2] == 0.f &&
                m.m[3][3] == 1.f);
    }
    
    const Matrix4x4 &GetMatrix() const { return m; }
    const Matrix4x4 &GetInverseMatrix() const { return mInv; }
    
    Transform operator*(const Transform &t2) const;
    bool SwapsHandedness() const;
};

Transform Translate(const vec3f &delta);
Transform Translate(Float x, Float y, Float z);
Transform Translate(Float u);
Transform Scale(Float x, Float y, Float z);
Transform Scale(Float u);
Transform RotateX(Float theta);
Transform RotateY(Float theta);
Transform RotateZ(Float theta);
Transform Rotate(Float theta, const vec3f &axis);
bool SolveLinearSystem2x2(const Float A[2][2], const Float B[2], Float *x0,
                          Float *x1);
