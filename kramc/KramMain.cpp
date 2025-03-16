#include "KramLib.h"

#if KRAM_MAC
#include <sys/sysctl.h>
#endif

#if KRAM_WIN
#include <intrin.h>
#endif

using namespace STL_NAMESPACE;

// TODO: move this into vectormath
void checkSimdSupport()
{
    // Check for AVX2, FMA, F16C support on Intel.
    // AVX2 implies the other 2, but still have to enable on compile.
#if SIMD_AVX2
#if KRAM_MAC
    bool hasSimdSupport = true;
    
    // can also check AVX1.0
    // F16C (avx/avx2 imply F16C and assume Rosetta too)
    
    // https://csharpmulticore.blogspot.com/2014/12/how-to-check-intel-avx2-support-on-mac-os-x-haswell.html
    // machdep.cpu.features: FPU VME DE PSE TSC MSR PAE MCE CX8 APIC SEP MTRR PGE MCA CMOV PAT PSE36 CLFSH DS ACPI MMX FXSR SSE SSE2 SS HTT TM PBE SSE3 PCLMULQDQ DTES64 MON DSCPL VMX EST TM2 SSSE3 FMA CX16 TPR PDCM SSE4.1 SSE4.2 x2APIC MOVBE POPCNT AES PCID XSAVE OSXSAVE SEGLIM64 TSCTMR AVX1.0 RDRAND F16C
    // machdep.cpu.leaf7_features: SMEP ERMS RDWRFSGS TSC_THREAD_OFFSET BMI1 AVX2 BMI2 INVPCID
    
    
    const char* leaf7Features = "machdep.cpu.leaf7_features";
    
    size_t leaf7FeatureSize = 0;
    sysctlbyname(leaf7Features, nullptr, &leaf7FeatureSize, nullptr, 0);
    
    if (leaf7FeatureSize == 0) {
        hasSimdSupport = false;
    }
    else {
        vector<char> buffer;
        buffer.resize(leaf7FeatureSize);
        sysctlbyname(leaf7Features, buffer.data(), &leaf7FeatureSize, nullptr, 0);
        
        // If don't find avx2, then support is not present.
        // could be running under Rosetta2 but it's supposed to add AVX2 soon.
        bool hasAVX2 = strstr(buffer.data(), "AVX2") != nullptr;
        
        if (!hasAVX2) {
            hasSimdSupport = false;
        }
    }
    
    const char* cpuFeatures = "machdep.cpu.features";
    
    size_t cpuFeatureSize = 0;
    sysctlbyname(cpuFeatures, nullptr, &cpuFeatureSize, nullptr, 0);
    
    if (!hasSimdSupport || cpuFeatureSize == 0) {
        hasSimdSupport = false;
    }
    else {
        vector<char> buffer;
        buffer.resize(cpuFeatureSize);
        sysctlbyname(cpuFeatures, buffer.data(), &cpuFeatureSize, nullptr, 0);
        
        // Make sure compile has enabled these on AVX2
        bool hasF16C = strstr(buffer.data(), "F16C") != nullptr;
        bool hasFMA = strstr(buffer.data(), "FMA") != nullptr;
        
        if (!hasF16C) {
            hasSimdSupport = false;
        }
        else if (!hasFMA) {
            hasSimdSupport = false;
        }
    }
    
    // TODO: can add brand to this if find the sysctlbyname query
    if (!hasSimdSupport) {
        KLOGE("Main", "Missing simd support");
        exit(1);
    }
    
#elif KRAM_WIN
    bool hasSimdSupport = true;
    
    // Also handles Win for ARM (f.e. Prism is SSE4 -> AVX2 soon).
    // See here for more bits (f.e. AVX512)
    // https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
    
    // f1.ecx bit  0 is sse3
    // f1.ecx bit 12 is fma
    // f1.ecx bit 19 is sse4.1
    // f1.ecx bit 20 is sse4.2
    // f1.ecx bit 28 is avx.
    // f1.ecx bit 29 is f16c (docs are wrong about this being avx2)
    
    // f7.ebx bit 5 is avx2
    // f7.ebx bit 16 is avx-512f
    // f7.ebx bit 26 is avx-512pf
    // f7.ebx bit 27 is avx-512er
    // f7.ebx bit 28 is avx-512cd
    
    
    // This returns a count of the ids from mthe docs.
    struct CpuInfo {
        int eax, ebx, ecx, edx;
    };
    
    // numIds in 0
    // vendorId (12 char string) returned in 1,3,2
    // can tell intel from amd off vendorId
    CpuInfo cpuInfo = {};
    __cpuid((int*)&cpuInfo, 0);
    
    // This is GenuineIntel or AuthenticAMD
    char vendorId[12+1] = {};
    *reinterpret_cast<int*>(vendorId + 0) = cpuInfo.ebx;
    *reinterpret_cast<int*>(vendorId + 4) = cpuInfo.edx;
    *reinterpret_cast<int*>(vendorId + 8) = cpuInfo.ecx;
    
    int numIds = cpuInfo.eax;
    if (numIds < 7) {
        hasSimdSupport = false;
    }
    else {
        // +1 since 0 is the count and vendorId
        vector<CpuInfo> cpuInfoByIndex;
        cpuInfoByIndex.resize(numIds+1);
        
        // This has sse4, avx, f16c
        __cpuidex((int*)&cpuInfo, 1, 0);
        cpuInfoByIndex[1] = cpuInfo;
        
        // This has AVX2, avx512
        __cpuidex((int*)&cpuInfo, 7, 0);
        cpuInfoByIndex[7] = cpuInfo;
        
        bool hasAVX2 = cpuInfoByIndex[7].ebx & (1 << 5);
        
        bool hasFMA = cpuInfoByIndex[1].ecx & (1 << 12);
        bool hasF16C = cpuInfoByIndex[1].ecx & (1 << 29);
        
        if (!hasAVX2)
            hasSimdSupport = false;
        else if (!hasFMA)
            hasSimdSupport = false;
        else if (!hasF16C)
            hasSimdSupport = false;
    }
    
    // extended cpuid attributes
    int extBase = 0x80000000;
    __cpuid((int*)&cpuInfo, extBase);
    numIds = cpuInfo.eax - extBase;
    
    char brandId[48+1] = {};
    
    if (numIds >= 4)
    {
        vector<CpuInfo> cpuInfoByIndex;
        cpuInfoByIndex.resize(numIds+1);
        
        // f81
        __cpuidex((int*)&cpuInfo, extBase+1, 0);
        cpuInfoByIndex[1] = cpuInfo;
        
        // brand
        __cpuidex((int*)&cpuInfo, extBase+2, 0);
        cpuInfoByIndex[2] = cpuInfo;
        __cpuidex((int*)&cpuInfo, extBase+3, 0);
        cpuInfoByIndex[3] = cpuInfo;
        __cpuidex((int*)&cpuInfo, extBase+4, 0);
        cpuInfoByIndex[4] = cpuInfo;
        
        memcpy(brandId +  0, &cpuInfoByIndex[2], sizeof(CpuInfo));
        memcpy(brandId + 16, &cpuInfoByIndex[3], sizeof(CpuInfo));
        memcpy(brandId + 32, &cpuInfoByIndex[4], sizeof(CpuInfo));
    }
    
    if (!hasSimdSupport) {
        KLOGE("Main", "Missing simd support for %s", brandId);
        exit(1);
    }
    
#endif
#endif
}

int main(int argc, char* argv[])
{
    // This will exit if insufficient simd support on x64.
    // arm64+neon has full support of all operations.
    checkSimdSupport();
    
    // verify that machine has simd support to run
    int errorCode = kram::kramAppMain(argc, argv);

    // returning -1 from main results in exit code of 255, so fix this to return 1 on failure.
    if (errorCode != 0) {
        exit(1);
    }

    return 0;
}
