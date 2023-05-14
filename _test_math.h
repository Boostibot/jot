#pragma once
#include "math.h"
#include <random>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include "_test.h"

namespace jot
{
namespace tests
{
    #define TEST_EQ(a, b) TEST(equals(a, b))
    #define TEST_NOT_EQ(a, b) TEST(equals(a, b) == false)

    using GVector2 = glm::vec2;
    using GVector3 = glm::vec3;
    using GVector4 = glm::vec4;

    Vector2 to_jot(GVector2 ext) { return {ext.x, ext.y}; }
    Vector3 to_jot(GVector3 ext) { return {ext.x, ext.y, ext.z}; }
    Vector4 to_jot(GVector4 ext) { return {ext.x, ext.y, ext.z, ext.w}; }
    
    GVector2 to_glm(Vector2 ext) { return {ext.x, ext.y}; }
    GVector3 to_glm(Vector3 ext) { return {ext.x, ext.y, ext.z}; }
    GVector4 to_glm(Vector4 ext) { return {ext.x, ext.y, ext.z, ext.w}; }


    void test_vector_ops(Vector4 in, Vector4 arg2, Vector4 arg3, float factor_a, float factor_b)
    {
        glm::vec4 gin = to_glm(in);
        glm::vec4 ga = to_glm(arg2);
        glm::vec4 gb = to_glm(arg3);

        GVector4 gsum = gin + factor_a*ga;
        Vector4 sum = add(in, scale(factor_a, arg2));
        TEST_EQ(sum.x, in.x + factor_a*arg2.x);
        TEST_EQ(sum.y, in.y + factor_a*arg2.y);
        TEST_EQ(sum.z, in.z + factor_a*arg2.z);
        TEST_EQ(sum.w, in.w + factor_a*arg2.w);

        TEST_EQ(length(Vector3{2, 0, 0}), 2);
        TEST_EQ(length(Vector3{3, 4, 0}), 5);
        TEST_EQ(length(Vector3{-2, 0, 0}), 2);
        TEST_EQ(length(Vector3{0, 0, 0}), 0);

        TEST_EQ(length(sum), length(gsum));

        {
            Vector4 normed = {0};
            if(!equals(in, Vector4{0}))
            {
                normed = normalize(in);
                TEST_EQ(length(normed), 1);
                TEST_EQ(scale(length(in), normed), in);
            }
            else
            {
                TEST_EQ(length(in), 0);
            }
            
        }

        GVector3 gred_sum = {gsum.x, gsum.y, gsum.z};
        GVector3 gred_a = {arg2.x, arg2.y, arg2.z};

        Vector3 red_sum = {sum.x, sum.y, sum.z};
        Vector3 red_a = {arg2.x, arg2.y, arg2.z};

        TEST_EQ(dot(Vector4{1, 0, 0}, Vector4{0, 0, 1, 0}), 0);
        TEST_EQ(dot(red_sum, red_sum), length(red_sum)*length(red_sum));

        TEST_EQ(cross(red_sum, red_sum), (Vector3{0, 0, 0}));
        {
            Vector3 crossed1 = cross(red_sum, red_a);
            Vector3 crossed2 = to_jot(glm::cross(gred_sum, gred_a));
            TEST_EQ(crossed1, crossed2);

            //Must be ortogonal
            TEST_EQ(dot(red_sum, crossed1), 0);
            TEST_EQ(dot(red_a, crossed1), 0);
        }

        {
            Vector3 crossed1 = scale(factor_b, cross(red_sum, red_a));
            Vector3 crossed2 = cross(scale(factor_b, red_sum), red_a);
            Vector3 crossed3 = cross(red_sum, scale(factor_b, red_a));
            
            //linear in both arguments
            TEST_EQ(crossed1, crossed2);
            TEST_EQ(crossed1, crossed3);
            
            //anticomutative
            Vector3 anticomut1 = cross(red_sum, red_a);
            Vector3 anticomut2 = scale(-1, cross(red_a, red_sum));
            TEST_EQ(anticomut1, anticomut2);
        }
        
        {
            float angle1 = angle_between(Vector3{1, 0, 0}, Vector3{-1, 0, 0});
            float angle2 = angle_between(Vector3{1, 0, 0}, Vector3{0, 1, 0});
            float angle3 = angle_between(Vector3{1, 0, 0}, Vector3{1, 1, 0});
            TEST_EQ(angle1, PI);
            TEST_EQ(angle2, PI/2);
            TEST_EQ(angle3, PI/4);
        }

        {
            float angle1 = angle_between(red_sum, red_a);
            float angle1_slow = angle_between_slow(red_sum, red_a);
            float angle1_slow2 = angle_between_slow(normalize(red_sum), normalize(red_a));
            float angle2 = angle_between(red_a, red_sum);
            float angle3 = angle_between(scale(2.1f, red_a), scale(10, red_sum));
            float bad_angle1 = angle_between(scale(-2, red_a), scale(10, red_sum));
            float bad_angle2 = angle_between(sub(red_sum, red_a), scale(10, red_sum));

            TEST_EQ(angle1, angle2);
            TEST_EQ(angle1, angle3);
            TEST_EQ(angle1, angle1_slow);
            TEST_EQ(angle1, angle1_slow2);

            if(!equals(length(red_a), 0) && !equals(length(red_sum), 0))
            {
                float angle4 = glm::angle(normalize(gred_a), normalize(gred_sum));

                //glm has a very innacurate implementation for whatever reason
                //or maybe its cause by the compiler not seeing into it as much because of all
                //the templates...
                TEST(equals(angle1, angle4, 0.001f)); 
                TEST(equals(angle1_slow2, angle4, 0.001f));

                TEST_NOT_EQ(angle1, bad_angle1);
                if(!equals(angle1, 0) && !equals(bad_angle2, 0))
                    TEST_NOT_EQ(angle1, bad_angle2);
            }
        }
    }

    void test_math()
    {
        test_vector_ops(Vector4{0, 0, 0, 0}, Vector4{0, 0, 0, 0}, Vector4{0, 0, 0, 0}, 2, -1);
        test_vector_ops(Vector4{1, 0, 0, 0}, Vector4{0, 1, 0, 0}, Vector4{0, 0, 1, 0}, 2, -1);
        test_vector_ops(Vector4{0, 0, 32, 0}, Vector4{0, 2, 0, 46.4f}, Vector4{3, 0, 1, 8}, 2845, -513);
        test_vector_ops(Vector4{0, 0, 0, 0}, Vector4{0, 1, 1, 1}, Vector4{0, 0, 1.2f, 0}, 351351.4563f, 424.0f);
    }
}
}