#pragma once

#ifndef JOT_DECL
#define JOT_DECL constexpr
#endif JOT_DECL

JOT_DECL float max(float a, float b) { return a > b ? a : b; }
JOT_DECL float min(float a, float b) { return a < b ? a : b; }
JOT_DECL float clamp(float val, float lo, float hi)           
{ 
    return max(lo, min(val, hi)); 
}

JOT_DECL float lerp(float lo, float hi, float t) 
{
    return lo * (1.0f - t) + hi * t;
}

JOT_DECL double max(double a, double b) { return a > b ? a : b; }
JOT_DECL double min(double a, double b) { return a < b ? a : b; }
JOT_DECL double clamp(double val, double lo, double hi)            
{ 
    return max(lo, min(val, hi)); 
}

JOT_DECL double lerp(double lo, double hi, double t) 
{
    return lo * (1.0 - t) + hi * t;
}