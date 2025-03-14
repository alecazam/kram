#include "KramLib.h"

#if KRAM_MAC
#include <sys/sysctl.h>
#endif

#if KRAM_WIN
#include <intrin.h>
#endif

using namespace STL_NAMESPACE;

int main(int argc, char* argv[])
{
    bool hasSimdSupport = true;
    
#if KRAM_MAC
    #if SIMD_AVX2
    
    // can also check AVX1.0
    // F16C (avx/avx2 imply F16C and assume Rosetta too)
    
    // https://csharpmulticore.blogspot.com/2014/12/how-to-check-intel-avx2-support-on-mac-os-x-haswell.html
    // machdep.cpu.features: FPU VME DE PSE TSC MSR PAE MCE CX8 APIC SEP MTRR PGE MCA CMOV PAT PSE36 CLFSH DS ACPI MMX FXSR SSE SSE2 SS HTT TM PBE SSE3 PCLMULQDQ DTES64 MON DSCPL VMX EST TM2 SSSE3 FMA CX16 TPR PDCM SSE4.1 SSE4.2 x2APIC MOVBE POPCNT AES PCID XSAVE OSXSAVE SEGLIM64 TSCTMR AVX1.0 RDRAND F16C
    // machdep.cpu.leaf7_features: SMEP ERMS RDWRFSGS TSC_THREAD_OFFSET BMI1 AVX2 BMI2 INVPCID
    
    
    //size_t cpuFeatureSize = 0;
    //sysctlbyname("machdep.cpu.features", nullptr, &cpuFeatureSize, nullptr, 0);
    
    const char* avx2Features = "machdep.cpu.leaf7_features";
    
    size_t leaf7FeatureSize = 0;
    sysctlbyname(avx2Features, nullptr, &leaf7FeatureSize, nullptr, 0);
    
    if (leaf7FeatureSize == 0) {
        hasSimdSupport = false;
    }
    else {
        vector<char> buffer;
        buffer.resize(leaf7FeatureSize);
        sysctlbyname(avx2Features, buffer.data(), &leaf7FeatureSize, nullptr, 0);
        
        // If don't find avx2, then support is not present.
        // could be running under Rosetta2 but it's supposed to add AVX2 soon.
        if (strstr(buffer.data(), "AVX2") == nullptr) {
            hasSimdSupport = false;
        }
    }
    #endif
    
#elif KRAM_WIN
    // Also handles Win for ARM (f.e. Prism is SSE4 -> AVX2 soon).
    // See here for more bits (f.e. AVX512)
    // https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
    
    // f1.ecx bit  0 is sse3
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
    };'
    
    // numIds in 0
    // vendorId (12 char string) returned in 1,3,2
    // can tell intel from amd off vendorId
    CpuInfo cpuInfo = {};
    __cpuid(cpuInfo, 0);
    
    int numIds = cpuInfo[0];
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
        
        // TODO: probably another for AVX10
        
// This is to obtain all the bits, but only care about AVX2
//        for (int i = 0; i <= numIds; ++i) {
//            __cpuidex((int*)&cpuInfo, i, 0); // get the values
//            cpuInfoByIndex[i] = cpuInfo;
//        }
        
        // there are extended bits above 0x8000000
        
        #if SIMD_AVX2
        bool hasAVX2 = cpuInfoByIndex[7].ebx & (1 << 5);
        hasSimdSupport = hasAVX2;
        #endif
    }
    
#endif
    
    if (!hasSimdSupport) {
        KLOGE("Main", "Missing simd support");
        exit(1);
    }
    
    // verify that machine has simd support to run
    int errorCode = kram::kramAppMain(argc, argv);

    // returning -1 from main results in exit code of 255, so fix this to return 1 on failure.
    if (errorCode != 0) {
        exit(1);
    }

    return 0;
}
