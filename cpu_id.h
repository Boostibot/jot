#pragma once
//C/C++ header for checking and working with cpu id
//see https://en.wikipedia.org/wiki/CPUID for more info

//CPU_ID_DISSABLE Dissables all non-standard parts of this file.
// All functions will in that case return an empty/false but 
// the resulting program will still be comiplable and will require very little
// changes if any to work properly.
//Also makes it very probable that all the functions will get inlined (cince they are empty)
#if 0
#define CPU_ID_DISSABLE
#endif 

#include <stdint.h>
#include <string.h>

#if defined(_WIN32) && !defined(CPU_ID_DISSABLE)
#include <limits.h>
#include <intrin.h>
#endif

typedef struct CPU_Id
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} CPU_Id;

static  
CPU_Id cpu_id(uint32_t in_eax, uint32_t in_ecx)
{
    CPU_Id id = {0};
    (void) in_eax;
    (void) in_ecx;

    #ifndef CPU_ID_DISSABLE 
        id.eax = in_eax;
        id.ecx = in_ecx;
        #ifdef _WIN32
            __cpuidex((int *) (void*) &id, (int)in_eax, (int)in_ecx);
        #else
            asm volatile("cpuid"
                : "=a" (id.eax),
                  "=b" (id.ebx),
                  "=c" (id.ecx),
                  "=d" (id.edx)
                : "0" (in_eax), "2" (in_ecx));
        #endif
    #endif
    return id;
}

typedef struct CPU_Processor_Brand
{
    char name[48];
} CPU_Processor_Brand;

typedef enum
{
    CPU_VENDOR_INTEL,
    CPU_VENDOR_AMD,
    CPU_VENDOR_OTHER = 0
} CPU_Vendor;

typedef struct CPU_Vendor_Info
{
    char name[16];
    uint32_t max_cpuid_function;
    CPU_Vendor vendor;
} CPU_Vendor_Info;

static
CPU_Vendor_Info cpu_vendor(void)
{
    CPU_Vendor_Info info = {0};

    #ifndef CPU_ID_DISSABLE
    CPU_Id max_id = cpu_id(0, 0);
    info.max_cpuid_function = max_id.eax;

    //copy vendor name
    memcpy(info.name + 0, &max_id.ebx, 4);
    memcpy(info.name + 4, &max_id.edx, 4);
    memcpy(info.name + 8, &max_id.ecx, 4);

    //determine vendor 
    const char* intel = "GenuineIntel";
    const char* amd   = "AuthenticAMD";
    if(memcmp(info.name, intel, strlen(intel)) == 0)
        info.vendor = CPU_VENDOR_INTEL;
    else if(memcmp(info.name, amd, strlen(amd)) == 0)
        info.vendor = CPU_VENDOR_AMD;
    else
        info.vendor = CPU_VENDOR_OTHER;
    #endif

    return info;
}

static
CPU_Processor_Brand cpu_brand(void)
{
    CPU_Processor_Brand brand = {0};

    #ifndef CPU_ID_DISSABLE 
    CPU_Id tester = cpu_id(0x80000000, 0);
    if (tester.eax < 0x80000004)
        return brand;

    CPU_Id ids[3] = {0};
    ids[0] = cpu_id(0x80000002, 0);
    ids[1] = cpu_id(0x80000003, 0);
    ids[2] = cpu_id(0x80000004, 0);

    memcpy(brand.name, ids, sizeof(ids));
    #endif

    return brand;
}

static
uint32_t max_cpuid_function(void)
{
    CPU_Id id = cpu_id(0, 0);
    return id.eax;   
}

typedef struct CPU_Info
{
    //Null terminated vendor string
    char vendor_name[16];        //"GenuineIntel"
    char processor_brand[48];    //"Intel(R) Xeon(R) Platinum 8259CL CPU @ 2.50GHz"
    uint32_t max_cpuid_function; //13

    CPU_Vendor vendor; //..._INTEL

    //0b00 - Original equipment manufacturer (OEM) Processor 
    //0b01 - Intel Overdrive Processor 
    //0b10 - Dual processor (not applicable to Intel486 processors)
    //0b11 - Reserved
    uint32_t processor_type;
    uint32_t family;
    uint32_t model;
    uint32_t stepping;

    uint32_t extended_family;
    uint32_t extended_model;

    //there are many feature flags and we want to keep them as transparent as
    // possible (they should be as little jumbled up from the standard)
    //as such we split the feature flags into 3 (so far) categories:
    // We label them by the command to obtain them
    
    //cpu_id(0, 0)
    uint32_t feature_flags_ecx1;
    uint32_t feature_flags_edx1;

    //cpu_id(7, 0)
    uint32_t feature_flags_ebx2;
    uint32_t feature_flags_ecx2;
    uint32_t feature_flags_edx2;

    //cpu_id(7, 1)
    uint32_t feature_flags_eax3;
    uint32_t feature_flags_ebx3;
    uint32_t feature_flags_edx3;
} CPU_Info;

typedef enum CPU_Feature_Flag
{
    CPU_FEATURE_MMX_EDX1 = 1 << 23,
    CPU_FEATURE_SSE_EDX1 = 1 << 25,
    CPU_FEATURE_SSE2_EDX1 = 1 << 26,

    CPU_FEATURE_SSE3_ECX1 = 1 << 0,
    CPU_FEATURE_FMA_ECX1 = 1 << 12,
    CPU_FEATURE_SSE4_1_ECX1 = 1 << 19,
    CPU_FEATURE_SSE4_2_ECX1 = 1 << 20,
    CPU_FEATURE_POPCNT_ECX1 = 1 << 23,
    CPU_FEATURE_AVX_ECX1 = 1 << 28,
    CPU_FEATURE_F16C_ECX1 = 1 << 29,

    CPU_FEATURE_AVX2_EBX2 = 1 << 5,
    CPU_FEATURE_AVX512_F_EBX2 = 1 << 16,
    CPU_FEATURE_AVX512_DQ_EBX2 = 1 << 17,
    CPU_FEATURE_AVX512_IFMA_EBX2 = 1 << 21,

    CPU_FEATURE_AVX512_VBMI_ECX2 = 1 << 1,
    CPU_FEATURE_AVX512_VBMI2_ECX2 = 1 << 6,

    CPU_FEATURE_AVX512_BF16_EBX3 = 1 << 5,
} CPU_Feature_Flag;

static
CPU_Info cpu_info(void)
{
    CPU_Info info = {0};

    #ifndef CPU_ID_DISSABLE
    CPU_Vendor_Info vendor = cpu_vendor();
    CPU_Processor_Brand brand = cpu_brand();

    info.max_cpuid_function = vendor.max_cpuid_function;
    info.vendor = vendor.vendor;

    //copy brand and vendor strings
    memcpy(info.vendor_name, vendor.name, sizeof(vendor.name));
    memcpy(info.processor_brand, brand.name, sizeof(brand.name));

    if(info.max_cpuid_function >= 1)
    {
        CPU_Id id = cpu_id(1, 0);
        info.stepping = id.eax & 0xF;
        info.model = (id.eax >> 4) & 0xF;
        info.family = (id.eax >> 8) & 0xF;
        info.processor_type = (id.eax >> 12) & 0x3;
        info.extended_model = (id.eax >> 16) & 0xF;
        info.extended_family = (id.eax >> 20) & 0xFF;

        info.feature_flags_ecx1 = id.ecx;
        info.feature_flags_edx1 = id.edx;
    }

    if(info.max_cpuid_function >= 7)
    {
        CPU_Id extended1 = cpu_id(7, 0);
        CPU_Id extended2 = cpu_id(7, 1);

        info.feature_flags_ebx2 = extended1.ebx;
        info.feature_flags_ecx2 = extended1.ecx;
        info.feature_flags_edx2 = extended1.edx;

        info.feature_flags_eax3 = extended2.eax;
        info.feature_flags_ebx3 = extended2.ebx;
        info.feature_flags_edx3 = extended2.edx;
    }
    #endif // !CPU_ID_DISSABLE

    return info;
}

#ifdef __cplusplus
static const CPU_Info CPU_INFO = cpu_info();
#endif

#ifdef CPU_ID_EXAMPLE
#include <stdio.h>

int main()
{
    CPU_Info info = cpu_info();
    printf("vendor: %s\n", info.vendor_name);
    printf("brand: %s\n", info.processor_brand);
    printf("max: %d\n", info.max_cpuid_function);
    printf("is intel: %d\n", info.vendor == CPU_VENDOR_INTEL);

    printf("processor_type: %d\n", info.processor_type);
    printf("family: %d\n", info.family);
    printf("model: %d\n", info.model);
    printf("stepping: %d\n", info.stepping);
    printf("extended_family: %d\n", info.extended_family);
    printf("extended_model: %d\n", info.extended_model);
    
    printf("\nFLAGS:\n");
    printf("MMX: %d\n", (info.feature_flags_edx1 & CPU_FEATURE_MMX_EDX1) > 0);
    printf("SSE: %d\n", (info.feature_flags_edx1 & CPU_FEATURE_SSE_EDX1) > 0);
    printf("SSE2: %d\n", (info.feature_flags_edx1 & CPU_FEATURE_SSE2_EDX1) > 0);
    printf("SSE3: %d\n", (info.feature_flags_ecx1 & CPU_FEATURE_SSE3_ECX1) > 0);
    printf("AVX: %d\n", (info.feature_flags_ecx1 & CPU_FEATURE_AVX_ECX1) > 0);
    printf("AVX2: %d\n", (info.feature_flags_ebx2 & CPU_FEATURE_AVX2_EBX2) > 0);
    printf("AVX512: %d\n", (info.feature_flags_ebx2 & CPU_FEATURE_AVX512_F_EBX2) > 0);

    return (int) max_cpuid_function();
}

#endif