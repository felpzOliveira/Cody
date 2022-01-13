#pragma once
#include <geometry.h>

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
        return vec3<T>(m.m[0][0] * x + m.m[1][0] * y + m.m[2][0] * z,
                       m.m[0][1] * x + m.m[1][1] * y + m.m[2][1] * z,
                       m.m[0][2] * x + m.m[1][2] * y + m.m[2][2] * z);
    }

    template<typename T> vec2<T> Point(const vec2<T> &p) const{
        T x = p.x, y = p.y, z = T(0);
        T xp = m.m[0][0] * x + m.m[1][0] * y + m.m[2][0] * z + m.m[3][0];
        T yp = m.m[0][1] * x + m.m[1][1] * y + m.m[2][1] * z + m.m[3][1];
        T wp = m.m[0][3] * x + m.m[1][3] * y + m.m[2][3] * z + m.m[3][3];
        if (wp == 1)
            return vec2<T>(xp, yp);
        else{
            AssertA(!IsZero(wp), "Zero transform wp [Point2f]");
            return vec2<T>(xp, yp) / wp;
        }
    }

    template<typename T> vec3<T> Point(const vec3<T> &p) const{
        T x = p.x, y = p.y, z = p.z;
        T xp = m.m[0][0] * x + m.m[1][0] * y + m.m[2][0] * z + m.m[3][0];
        T yp = m.m[0][1] * x + m.m[1][1] * y + m.m[2][1] * z + m.m[3][1];
        T zp = m.m[0][2] * x + m.m[1][2] * y + m.m[2][2] * z + m.m[3][2];
        T wp = m.m[0][3] * x + m.m[1][3] * y + m.m[2][3] * z + m.m[3][3];
        if (wp == 1)
            return vec3<T>(xp, yp, zp);
        else{
            if(IsZero(wp)){
                printf("Zero wp\n");
            }
            AssertA(!IsZero(wp), "Zero transform wp [Point3f]");
            return vec3<T>(xp, yp, zp) / wp;
        }
    }

    bool HasTranslation() const{
        return !IsZero(m.m[3][0]) || !IsZero(m.m[3][1]) || !IsZero(m.m[3][2]);
    }

    bool IsIdentity() const{
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
    void PrintSelf(){
        m.PrintSelf();
    }
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
Transform Orthographic(Float left, Float right, Float bottom, Float top,
                       Float zNear, Float zFar);
