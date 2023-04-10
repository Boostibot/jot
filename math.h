#pragma once

#ifndef JOT_MATH_DECL
#define JOT_MATH_DECL [[nodiscard]] constexpr
#endif JOT_MATH_DECL

JOT_MATH_DECL float lerp(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JOT_MATH_DECL double lerp(double lo, double hi, double t) 
{
    return lo * (1.0 - t) + hi * t;
}