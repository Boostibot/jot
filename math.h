#pragma once
#include <math.h>
#include <stddef.h>
#include <string.h>

//@TODO: Projection perspective and ortogonal
//@TODO: Look at
//@TODO: ortogonalize
//@TODO: rotate vec2 by angle + by vector angle (complex-like rotation)
//@TODO: quaternions 
//@TODO: test matrices
//@TODO: make math_simd.h

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

#ifndef EPSILON
    #define EPSILON 1.0e-6f
#endif

#ifndef JMAPI
    #define JMAPI static
#endif JMAPI

struct Vector2
{
    float x;
    float y;
};

struct Vector3
{
    float x;
    float y;
    float z;
};

struct Vector4
{
    float x;
    float y;
    float z;
    float w;
};

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

//Column major matrix
struct Matrix4
{
    //@NOTE: we declare all ops primarily on Matrix4 and only provide Matrix3 and Matrix2 as
    // shrunk down storagge types. We also provide wrappers that promote the given 
    // matrix to Matrix4 and hope the compiler will be smart enough to optimize all the 
    // unneccessary ops result

    float _11;
    float _21;
    float _31;
    float _41;

    float _12;
    float _22;
    float _32;
    float _42;
    
    float _13;
    float _23;
    float _33;
    float _43;
    
    float _14;
    float _24;
    float _34;
    float _44;
};

struct Matrix3
{
    float _11;
    float _21;
    float _31;

    float _12;
    float _22;
    float _32;
    
    float _13;
    float _23;
    float _33;
};

struct Matrix2
{
    float _11;
    float _21;

    float _12;
    float _22;
};


JMAPI float max(float a, float b) { return a > b ? a : b; }
JMAPI float min(float a, float b) { return a < b ? a : b; }
JMAPI float clamp(float val, float lo, float hi)           
{ 
    return max(lo, min(val, hi)); 
}

JMAPI float lerp(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JMAPI float remap(float value, float input_from, float input_to, float output_from, float output_to)
{
    float result = (value - input_from)/(input_to - input_from)*(output_to - output_from) + output_from;
    return result;
}

JMAPI bool equals(float a, float b, float epsilon)
{
    return fabsf(a - b) <= epsilon;
}

JMAPI bool equals(float x, float y)
{
    float factor = fmaxf(1.0f, fmaxf(fabsf(x), fabsf(y)));
    return equals(x, y, factor*EPSILON);
}

JMAPI const float* data(Vector2 const& vec) { return &vec.x; }
JMAPI const float* data(Vector3 const& vec) { return &vec.x; }
JMAPI const float* data(Vector4 const& vec) { return &vec.x; }

JMAPI float* data(Vector2* vec) { return &vec->x; }
JMAPI float* data(Vector3* vec) { return &vec->x; }
JMAPI float* data(Vector4* vec) { return &vec->x; }

JMAPI Vector2 vec2_of(float scalar) { return {scalar, scalar}; }
JMAPI Vector3 vec3_of(float scalar) { return {scalar, scalar, scalar}; }
JMAPI Vector4 vec4_of(float scalar) { return {scalar, scalar, scalar, scalar}; }

JMAPI Vector2 add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
JMAPI Vector3 add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
JMAPI Vector4 add(Vector4 a, Vector4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }

JMAPI Vector2 sub(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
JMAPI Vector3 sub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
JMAPI Vector4 sub(Vector4 a, Vector4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

JMAPI Vector2 pairwise_mul(Vector2 a, Vector2 b) { return {a.x * b.x, a.y * b.y}; }
JMAPI Vector3 pairwise_mul(Vector3 a, Vector3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
JMAPI Vector4 pairwise_mul(Vector4 a, Vector4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

JMAPI Vector2 scale(float scalar, Vector2 a) { return {scalar * a.x, scalar * a.y}; }
JMAPI Vector3 scale(float scalar, Vector3 a) { return {scalar * a.x, scalar * a.y, scalar * a.z}; }
JMAPI Vector4 scale(float scalar, Vector4 a) { return {scalar * a.x, scalar * a.y, scalar * a.z, scalar * a.w}; }

JMAPI float dot(Vector2 a, Vector2 b) { return a.x*b.x + a.y*b.y; }
JMAPI float dot(Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
JMAPI float dot(Vector4 a, Vector4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

JMAPI float square_length(Vector2 a) { return dot(a, a); }
JMAPI float square_length(Vector3 a) { return dot(a, a); }
JMAPI float square_length(Vector4 a) { return dot(a, a); }

JMAPI float length(Vector2 a) { return sqrtf(square_length(a)); }
JMAPI float length(Vector3 a) { return sqrtf(square_length(a)); }
JMAPI float length(Vector4 a) { return sqrtf(square_length(a)); }

JMAPI Vector2 normalize(Vector2 a) { return scale(1.0f/length(a), a); }
JMAPI Vector3 normalize(Vector3 a) { return scale(1.0f/length(a), a); }
JMAPI Vector4 normalize(Vector4 a) { return scale(1.0f/length(a), a); }

JMAPI bool exact_equals(Vector2 a, Vector2 b) { return a.x==b.x && a.y==b.y; }
JMAPI bool exact_equals(Vector3 a, Vector3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
JMAPI bool exact_equals(Vector4 a, Vector4 b) { return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w; }

JMAPI bool equals(Vector2 a, Vector2 b, float epsilon) 
{
    return equals(a.x, b.x, epsilon) && equals(a.y, b.y, epsilon);
}

JMAPI bool equals(Vector3 a, Vector3 b, float epsilon) 
{
    return equals(a.x, b.x, epsilon) && equals(a.y, b.y, epsilon) && equals(a.z, b.z, epsilon);
}

JMAPI bool equals(Vector4 a, Vector4 b, float epsilon) 
{
    return equals(a.x, b.x, epsilon) && equals(a.y, b.y, epsilon) && equals(a.z, b.z, epsilon) && equals(a.w, b.w, epsilon);
}

JMAPI bool equals(Vector2 a, Vector2 b) 
{
    return equals(a.x, b.x) && equals(a.y, b.y);
}

JMAPI bool equals(Vector3 a, Vector3 b) 
{
    return equals(a.x, b.x) && equals(a.y, b.y) && equals(a.z, b.z);
}

JMAPI bool equals(Vector4 a, Vector4 b) 
{
    return equals(a.x, b.x) && equals(a.y, b.y) && equals(a.z, b.z) && equals(a.w, b.w);
}

JMAPI Vector3 cross(Vector3 a, Vector3 b)
{
    Vector3 result = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    return result;
}

JMAPI float angle_between(Vector2 a, Vector2 b)
{
    float len2a = square_length(a);
    float len2b = square_length(b);
    float den = sqrtf(len2a*len2b);
    float num = dot(a, b);
    float result = acosf(num/den);

    return result;
}

JMAPI float angle_between_slow(Vector3 a, Vector3 b)
{
    float len2a = square_length(a);
    float len2b = square_length(b);
    float den = sqrtf(len2a*len2b);
    float num = dot(a, b);
    float result = acosf(num/den);

    return result;
}

JMAPI float angle_between(Vector3 a, Vector3 b)
{
    //source: raymath.h
    Vector3 crossed = cross(a, b);
    float cross_len = length(crossed);
    float dotted = dot(a, b);
    float result = atan2f(cross_len, dotted);

    return result;
}

JMAPI Vector2 apply(Matrix4 mat, Vector2 vec)
{
    Vector2 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y;
    result.y = mat._21*vec.x + mat._22*vec.y;
    return result;
}

JMAPI Vector3 apply(Matrix4 mat, Vector3 vec)
{
    Vector3 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y + mat._13*vec.z;
    result.y = mat._21*vec.x + mat._22*vec.y + mat._23*vec.z;
    result.z = mat._31*vec.x + mat._32*vec.y + mat._33*vec.z;
    return result;
}

JMAPI Vector4 apply(Matrix4 mat, Vector4 vec)
{
    Vector4 result = {0};
    result.x = mat._11*vec.x + mat._12*vec.y + mat._13*vec.z + mat._14*vec.w;
    result.y = mat._21*vec.x + mat._22*vec.y + mat._23*vec.z + mat._24*vec.w;
    result.z = mat._31*vec.x + mat._32*vec.y + mat._33*vec.z + mat._34*vec.w;
    result.w = mat._41*vec.x + mat._42*vec.y + mat._43*vec.z + mat._44*vec.w;
    return result;
}

JMAPI const float* data(Matrix2 const& mat) { return &mat._11; }
JMAPI const float* data(Matrix3 const& mat) { return &mat._11; }
JMAPI const float* data(Matrix4 const& mat) { return &mat._11; }

JMAPI float* data(Matrix2* mat) { return &mat->_11; }
JMAPI float* data(Matrix3* mat) { return &mat->_11; }
JMAPI float* data(Matrix4* mat) { return &mat->_11; }

JMAPI Vector2* column(Matrix2* matrix, ptrdiff_t column_i)
{
    float* ptr = (&matrix->_11) + 2*column_i;
    return (Vector2*) (void*) ptr;
}

JMAPI Vector3* column(Matrix3* matrix, ptrdiff_t column_i)
{
    float* ptr = (&matrix->_11) + 3*column_i;
    return (Vector3*) (void*) ptr;
}

JMAPI Vector4* column(Matrix4* matrix, ptrdiff_t column_i)
{
    float* ptr = (&matrix->_11) + 4*column_i;
    return (Vector4*) (void*) ptr;
}

JMAPI Vector2 column(Matrix2 matrix, ptrdiff_t column_i) { return *column(&matrix, column_i); }
JMAPI Vector3 column(Matrix3 matrix, ptrdiff_t column_i) { return *column(&matrix, column_i); }
JMAPI Vector4 column(Matrix4 matrix, ptrdiff_t column_i) { return *column(&matrix, column_i); }

JMAPI Vector2 row(Matrix2 matrix, ptrdiff_t row_i) 
{ 
    float* ptr = (&matrix._11) + row_i;
    Vector2 result = {ptr[0], ptr[2]};
    return result;
}

JMAPI Vector3 row(Matrix3 matrix, ptrdiff_t row_i) 
{ 
    float* ptr = (&matrix._11) + row_i;
    Vector3 result = {ptr[0], ptr[3], ptr[6]};
    return result;
}

JMAPI Vector4 row(Matrix4 matrix, ptrdiff_t row_i) 
{ 
    float* ptr = (&matrix._11) + row_i;
    Vector4 result = {ptr[0], ptr[4], ptr[8], ptr[12]};
    return result;
}

JMAPI Matrix4 add(Matrix4 a, Matrix4 b)
{
    Matrix4 result = {};
    result._11 = a._11 + b._11;
    result._21 = a._21 + b._21;
    result._31 = a._31 + b._31;
    result._41 = a._41 + b._41;

    result._12 = a._12 + b._12;
    result._22 = a._22 + b._22;
    result._32 = a._32 + b._32;
    result._42 = a._42 + b._42;
    
    result._13 = a._13 + b._13;
    result._23 = a._23 + b._23;
    result._33 = a._33 + b._33;
    result._43 = a._43 + b._43;
    
    result._14 = a._14 + b._14;
    result._24 = a._24 + b._24;
    result._34 = a._34 + b._34;
    result._44 = a._44 + b._44;
}

JMAPI Matrix4 sub(Matrix4 a, Matrix4 b)
{
    Matrix4 result = {};
    result._11 = a._11 - b._11;
    result._21 = a._21 - b._21;
    result._31 = a._31 - b._31;
    result._41 = a._41 - b._41;

    result._12 = a._12 - b._12;
    result._22 = a._22 - b._22;
    result._32 = a._32 - b._32;
    result._42 = a._42 - b._42;
    
    result._13 = a._13 - b._13;
    result._23 = a._23 - b._23;
    result._33 = a._33 - b._33;
    result._43 = a._43 - b._43;
    
    result._14 = a._14 - b._14;
    result._24 = a._24 - b._24;
    result._34 = a._34 - b._34;
    result._44 = a._44 - b._44;
    return result;
}

JMAPI Matrix4 scale(float scalar, Matrix4 mat)
{
    Matrix4 result = {};
    result._11 = scalar * mat._11;
    result._21 = scalar * mat._21;
    result._31 = scalar * mat._31;
    result._41 = scalar * mat._41;

    result._12 = scalar * mat._12;
    result._22 = scalar * mat._22;
    result._32 = scalar * mat._32;
    result._42 = scalar * mat._42;
    
    result._13 = scalar * mat._13;
    result._23 = scalar * mat._23;
    result._33 = scalar * mat._33;
    result._43 = scalar * mat._43;
    
    result._14 = scalar * mat._14;
    result._24 = scalar * mat._24;
    result._34 = scalar * mat._34;
    result._44 = scalar * mat._44;
    return result;
}

JMAPI Matrix4 mul(Matrix4 a, Matrix4 b)
{
    Matrix4 result = {};
    //result._ij = a._i1*b._1j + a._i2*b._2j + a._i3*b._3j + a._i4*b._4j;
    result._11 = a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41;
    result._21 = a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41;
    result._31 = a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41;
    result._41 = a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41;
    
    result._12 = a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42;
    result._22 = a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42;
    result._32 = a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42;
    result._42 = a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42;
    
    result._13 = a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43;
    result._23 = a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43;
    result._33 = a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43;
    result._43 = a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43;
    
    result._14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44;
    result._24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44;
    result._34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44;
    result._44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44;

    return result;
}

JMAPI bool equals(Matrix4 a, Matrix4 b, float epsilon) 
{
    const float* a_ptr = data(a);
    const float* b_ptr = data(b);
    for(ptrdiff_t i = 0; i < 4*4; i++)
    {
        if(equals(a_ptr[i], b_ptr[i], epsilon) == false)
            return false;
    }

    return true;
}

JMAPI bool equals(Matrix4 a, Matrix4 b) 
{
    const float* a_ptr = data(a);
    const float* b_ptr = data(b);
    for(ptrdiff_t i = 0; i < 4*4; i++)
    {
        if(equals(a_ptr[i], b_ptr[i]) == false)
            return false;
    }

    return true;
}

JMAPI bool exact_equals(Matrix4 a, Matrix4 b) 
{
    return memcmp(data(a), data(b), sizeof(Matrix4)) == 0;
}


JMAPI Matrix4 column_matrix(Vector4 col1, Vector4 col2, Vector4 col3, Vector4 col4)
{
    Matrix4 result = {};
    result._11 = col1.x;
    result._21 = col1.y;
    result._31 = col1.z;
    result._41 = col1.w;
    
    result._12 = col2.x;
    result._22 = col2.y;
    result._32 = col2.z;
    result._42 = col2.w;
    
    result._13 = col3.x;
    result._23 = col3.y;
    result._33 = col3.z;
    result._43 = col3.w;
    
    result._14 = col4.x;
    result._24 = col4.y;
    result._34 = col4.z;
    result._44 = col4.w;
    return result;
}

JMAPI Matrix4 row_matrix(Vector4 row1, Vector4 row2, Vector4 row3, Vector4 row4)
{
    Matrix4 result = {};
    result._11 = row1.x;
    result._12 = row1.y;
    result._13 = row1.z;
    result._14 = row1.w;
    
    result._21 = row2.x;
    result._22 = row2.y;
    result._23 = row2.z;
    result._24 = row2.w;
    
    result._31 = row3.x;
    result._32 = row3.y;
    result._33 = row3.z;
    result._34 = row3.w;
    
    result._41 = row4.x;
    result._42 = row4.y;
    result._43 = row4.z;
    result._44 = row4.w;
    return result;
}

JMAPI Matrix4 diagonal_matrix(Vector4 vec)
{
    Matrix4 result = {};
    result._11 = vec.x;
    result._22 = vec.y;
    result._33 = vec.z;
    result._44 = vec.w;
    return result;
}

JMAPI Matrix4 identity_matrix()
{
    return diagonal_matrix({1, 1, 1, 1});
}

JMAPI Matrix4 scaling_matrix(Vector3 vec)
{
    Vector4 diag = {vec.x, vec.y, vec.z, 1.0f};
    return diagonal_matrix(diag);
}

JMAPI Matrix4 translation_matrix(Vector3 vec)
{
    Matrix4 result = identity_matrix();
    result._41 = vec.x;
    result._42 = vec.y;
    result._43 = vec.z;
    return result;
}

JMAPI Matrix4 rotation_matrix(float radians, Vector3 axis)
{
	float c = cosf(radians);
	float s = sinf(radians);

	Vector3 na = normalize(axis);
    Vector3 t = scale(1.0f - c, na);

    Matrix4 rotation = {0};
	rotation._11 = c + t.x * na.x;
	rotation._12 = t.x * na.y + s*na.z;
	rotation._13 = t.x * na.z - s*na.y;

	rotation._21 = t.y * na.x - s*na.z;
	rotation._22 = c + t.y * na.y;
	rotation._23 = t.y * na.z + s*na.x;

	rotation._31 = t.z * na.x + s*na.y;
	rotation._32 = t.z * na.y - s*na.x;
	rotation._33 = c + t.z * na.z;
    rotation._44 = 1.0f;
    return rotation;
}

//@NOTE: the order is reverse from glm!
//this means rotate(translate(mat, ...), ...) in glm first rotates and then translates
//here it first translates and then rotatets!
JMAPI Matrix4 translate(Matrix4 matrix, Vector3 offset)
{
    Matrix4 translation = translation_matrix(offset);
    Matrix4 result = mul(translation, matrix);
    return result;
}

JMAPI Matrix4 rotate(Matrix4 mat, float radians, Vector3 axis)
{
    Matrix4 rotation = rotation_matrix(radians, axis);
    Matrix4 result = mul(rotation, mat);
    return result;
}

JMAPI Matrix4 scale(Matrix4 mat, Vector3 scale_by)
{
    Matrix4 result = {};
    *column(&result, 0) = scale(scale_by.x, *column(&mat, 0)); 
    *column(&result, 1) = scale(scale_by.y, *column(&mat, 1)); 
    *column(&result, 2) = scale(scale_by.z, *column(&mat, 2)); 
    *column(&result, 3) = *column(&mat, 3); 
    return result;
}

JMAPI Matrix4 to_matrix4(Matrix3 mat)
{
    Matrix4 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    result._31 = mat._31;
    
    result._12 = mat._12;
    result._22 = mat._22;
    result._32 = mat._32;
    
    result._13 = mat._13;
    result._23 = mat._23;
    result._33 = mat._33;

    return result;
}
JMAPI Matrix4 to_matrix4(Matrix2 mat)
{
    Matrix4 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    
    result._12 = mat._12;
    result._22 = mat._22;

    return result;
}

JMAPI Matrix3 to_matrix3(Matrix4 mat)
{
    Matrix3 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    result._31 = mat._31;
    
    result._12 = mat._12;
    result._22 = mat._22;
    result._32 = mat._32;
    
    result._13 = mat._13;
    result._23 = mat._23;
    result._33 = mat._33;
    return result;
}
JMAPI Matrix3 to_matrix3(Matrix2 mat)
{
    Matrix3 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    
    result._12 = mat._12;
    result._22 = mat._22;

    return result;
}

JMAPI Matrix2 to_matrix2(Matrix4 mat)
{
    Matrix2 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    
    result._12 = mat._12;
    result._22 = mat._22;
    
    return result;
}
JMAPI Matrix2 to_matrix2(Matrix3 mat)
{
    Matrix2 result = {};
    result._11 = mat._11;
    result._21 = mat._21;
    
    result._12 = mat._12;
    result._22 = mat._22;
    
    return result;
}

#if 0
JMAPI double max(double a, double b) { return a > b ? a : b; }
JMAPI double min(double a, double b) { return a < b ? a : b; }
JMAPI double clamp(double val, double lo, double hi)            
{ 
    return max(lo, min(val, hi)); 
}

JMAPI double lerp(double lo, double hi, double t) 
{
    return lo * (1.0 - t) + hi * t;
}
#endif
