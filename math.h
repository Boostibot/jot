#pragma once

#ifndef JOT_MATH_DECL
#define JOT_MATH_DECL constexpr
#endif JOT_MATH_DECL

JOT_MATH_DECL float max(float a, float b) { return a > b ? a : b; }
JOT_MATH_DECL float min(float a, float b) { return a < b ? a : b; }
JOT_MATH_DECL float clamp(float val, float lo, float hi)           
{ 
    return max(lo, min(val, hi)); 
}

JOT_MATH_DECL float lerp(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JOT_MATH_DECL double max(double a, double b) { return a > b ? a : b; }
JOT_MATH_DECL double min(double a, double b) { return a < b ? a : b; }
JOT_MATH_DECL double clamp(double val, double lo, double hi)            
{ 
    return max(lo, min(val, hi)); 
}

JOT_MATH_DECL double lerp(double lo, double hi, double t) 
{
    return lo * (1.0 - t) + hi * t;
}