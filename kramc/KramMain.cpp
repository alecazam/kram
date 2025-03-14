#include "KramLib.h"

#if KRAM_MAC
#include <sys/sysctl.h>
#endif

int main(int argc, char* argv[])
{
    bool hasSimdSupport = true;
    
#if KRAM_MAC
    // TODO: test for simd support here
    
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
        using namespace STL_NAMESPACE;
        
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
    // TODO: check Win support too
    // Also need Win for ARM tests (f.e. Prism is SSE4 -> AVX2 soon).
    
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
