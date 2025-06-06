#include "KramLib.h"

#if KRAM_MAC
#include <sys/sysctl.h>
#endif

#if KRAM_WIN
#include <intrin.h>
#endif

using namespace STL_NAMESPACE;

// These aren't avx2 specific, but just don't want unused func warning 
#if SIMD_AVX2
#if KRAM_MAC

inline const char* getMacOSVersion() {
    static char str[256] = {};
    if (str[0] == 0) {
        size_t size = sizeof(str);
        if (sysctlbyname("kern.osproductversion", str, &size, NULL, 0) == 0) {
            return str;
        }
    }
    return str;
}

inline bool isRunningUnderRosetta() {
    int ret = 0;
    size_t size = sizeof(ret);
    if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
        if (errno == ENOENT) {
            // sysctl doesn't exist - not running under Rosetta
            return false;
        }
        // Other error occurred
        return false;
    }
    return ret > 0;
}


inline uint32_t getMacOSMajorVersion() {
    // 15.4
    static uint32_t majorVersion = 0;
    if (majorVersion == 0) {
        sscanf(getMacOSVersion(), "%u", &majorVersion);
    }
    return majorVersion;
}

#endif
#endif

// TODO: move this into vectormath
void checkSimdSupport()
{
    // Check for AVX2, FMA, F16C support on Intel.
    // Still need to set compile flags, and arm64 emulators are also spotty.
    // arm64 native has everything needed.  No holes to check, or legacy simd.
    
#if SIMD_AVX2
#if KRAM_MAC
    // Apple added AVX2 and F16C? support to Rosetta in macOS 15 with no way
    // to detect it.   Really awesome, so skip the test.  There are
    // no supporting Intel hw devices on macOS 15 that don't have AVX2.
    // const char* macOSVersion = getMacOSVersion();
    // KLOGI("kram", "%s", macOSVersion);
    uint32_t majorOSVersions = getMacOSMajorVersion();
    if (majorOSVersions >= 15) {
        return;
    }
    
    bool hasSimdSupport = true;
    
    vector<char> cpuName;
    size_t cpuNameSize = 0;
    
    const char* cpuNameProp = "machdep.cpu.brand_string";
    
    if (sysctlbyname(cpuNameProp, nullptr, &cpuNameSize, nullptr, 0) >= 0) {
        cpuName.resize(cpuNameSize);
        
        // Assuming this is ascii
        sysctlbyname(cpuNameProp, cpuName.data(), &cpuNameSize, nullptr, 0);
    }
    
    
    // can also check AVX1.0
    // F16C (avx/avx2 imply F16C and assume Rosetta too)
    
    // https://csharpmulticore.blogspot.com/2014/12/how-to-check-intel-avx2-support-on-mac-os-x-haswell.html
    // machdep.cpu.features: FPU VME DE PSE TSC MSR PAE MCE CX8 APIC SEP MTRR PGE MCA CMOV PAT PSE36 CLFSH DS ACPI MMX FXSR SSE SSE2 SS HTT TM PBE SSE3 PCLMULQDQ DTES64 MON DSCPL VMX EST TM2 SSSE3 FMA CX16 TPR PDCM SSE4.1 SSE4.2 x2APIC MOVBE POPCNT AES PCID XSAVE OSXSAVE SEGLIM64 TSCTMR AVX1.0 RDRAND F16C
    // machdep.cpu.leaf7_features: SMEP ERMS RDWRFSGS TSC_THREAD_OFFSET BMI1 AVX2 BMI2 INVPCID
    const char* missingFeatures[4] = { "", "", "", "" };
    uint32_t missingFeaturesCount = 0;
    
    const char* leaf7Features = "machdep.cpu.leaf7_features";
    
    size_t leaf7FeatureSize = 0;
    sysctlbyname(leaf7Features, nullptr, &leaf7FeatureSize, nullptr, 0);
    
    vector<char> bufferLeaf7;
    
    if (leaf7FeatureSize == 0) {
        hasSimdSupport = false;
    }
    else {
        bufferLeaf7.resize(leaf7FeatureSize);
        
        // TODO: check failure
        sysctlbyname(leaf7Features, bufferLeaf7.data(), &leaf7FeatureSize, nullptr, 0);
    }
    
    const char* cpuFeatures = "machdep.cpu.features";
    
    size_t cpuFeatureSize = 0;
    sysctlbyname(cpuFeatures, nullptr, &cpuFeatureSize, nullptr, 0);
    
    vector<char> bufferFeatures;

    if (!hasSimdSupport || cpuFeatureSize == 0) {
        hasSimdSupport = false;
    }
    else {
        bufferFeatures.resize(cpuFeatureSize);
        
        // TODO: check failure
        sysctlbyname(cpuFeatures, bufferFeatures.data(), &cpuFeatureSize, nullptr, 0);
    }

    const char* features = !bufferFeatures.empty() ? bufferFeatures.data() : "";
    const char* features7 = !bufferLeaf7.empty() ? bufferLeaf7.data() : "";
   
    // If don't find avx2, then support is not present.
    // could be running under Rosetta2 but it's supposed to add AVX2 soon.
    bool hasAVX2 = strstr(features7, "AVX2") != nullptr;

    if (!hasAVX2) {
        missingFeatures[missingFeaturesCount++] = "AVX2 ";
        hasSimdSupport = false;
    }

    // Make sure compile has enabled these on AVX2.
    // Rosetta2 and Prism often don't emulate these.
    // (f.e. BMI and F16C)
    
    bool hasAVX = strstr(features, "AVX") != nullptr;
    bool hasF16C = strstr(features, "F16C") != nullptr;
    bool hasFMA = strstr(features, "FMA") != nullptr;
    
    if (!hasAVX) {
        missingFeatures[missingFeaturesCount++] = "AVX ";
        hasSimdSupport = false;
    }
    if (!hasF16C) {
        missingFeatures[missingFeaturesCount++] = "F16C ";
        hasSimdSupport = false;
    }
    if (!hasFMA) {
        missingFeatures[missingFeaturesCount++] = "FMA ";
        hasSimdSupport = false;
    }
    
    if (!hasSimdSupport) {
        bool isEmulated = isRunningUnderRosetta() && (majorVersion < 15);
        const char* emulatedHint = isEmulated ? " install macOS 15.0+" : "";
        
        KLOGE("Main", "Missing simd support for %s%s%s%son %s%s",
              missingFeatures[0], missingFeatures[1], missingFeatures[2], missingFeatures[3],
              cpuName.data(), emulatedHint);
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
    // f1.ecx bit 28 is avx
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
    
    const char* missingFeatures[4] = { "", "", "", "" };
    uint32_t missingFeaturesCount = 0;
    
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
        bool hasAVX = cpuInfoByIndex[1].ecx & (1 << 28);
        bool hasF16C = cpuInfoByIndex[1].ecx & (1 << 29);
        
        if (!hasAVX2) {
            missingFeatures[missingFeaturesCount++] = "AVX2 ";
            hasSimdSupport = false;
        }
        if (!hasAVX) {
            missingFeatures[missingFeaturesCount++] = "AVX ";
            hasSimdSupport = false;
        }
        if (!hasFMA) {
            missingFeatures[missingFeaturesCount++] = "FMA ";
            hasSimdSupport = false;
        }
        if (!hasF16C) {
            missingFeatures[missingFeaturesCount++] = "F16C ";
            hasSimdSupport = false;
        }
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
        KLOGE("Main", "Missing simd support for %s%s%s%son %s",
              missingFeatures[0], missingFeatures[1], missingFeatures[2], missingFeatures[3],
              brandId);
        exit(1);
    }
        
#elif KRAM_LINUX // || KRAM_MAC
        
    // This should apply to all clang and gcc builds.  So may want
    // to use on all platforms.
        
    //        Common CPU features that can be checked with __builtin_cpu_supports include:
    //        sse, sse2, sse3, ssse3, sse4.1, sse4.2
    //        avx, avx2, avx512f
    //        fma
    //        bmi, bmi2
    //        popcnt
    //        lzcnt
    //        mmx
        
        
    bool hasSimdSupport = true;
        
    bool hasAVX2 = __builtin_cpu_supports("avx2");
    
    bool hasFMA = __builtin_cpu_supports("fma");
    bool hasAVX = __builtin_cpu_supports("avx");
    
    // macOS doesn't support f16c as string?
    #if  KRAM_MAC
    bool hasF16C = true; // a lie
    #else
    bool hasF16C = __builtin_cpu_supports("f16c");
    #endif
    
    const char* missingFeatures[4] = { "", "", "", "" };
    uint32_t missingFeaturesCount = 0;
    
    if (!hasAVX2) {
        missingFeatures[missingFeaturesCount++] = "AVX2 ";
        hasSimdSupport = false;
    }
    if (!hasAVX) {
        missingFeatures[missingFeaturesCount++] = "AVX ";
        hasSimdSupport = false;
    }
    if (!hasFMA) {
        missingFeatures[missingFeaturesCount++] = "FMA ";
        hasSimdSupport = false;
    }
    if (!hasF16C) {
        missingFeatures[missingFeaturesCount++] = "F16C ";
        hasSimdSupport = false;
    }
       
    if (!hasSimdSupport) {
        KLOGE("Main", "Missing simd support for %s%s%s%s",
              missingFeatures[0], missingFeatures[1], missingFeatures[2], missingFeatures[3]);
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
