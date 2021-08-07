/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ARCH_ATOMIC_CPU_MB()
//
#if defined(EASTL_ARCH_ATOMIC_CPU_MB)
	#define EASTL_ARCH_ATOMIC_CPU_MB_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_CPU_MB_AVAILABLE 0
#endif


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ARCH_ATOMIC_CPU_WMB()
//
#if defined(EASTL_ARCH_ATOMIC_CPU_WMB)
	#define EASTL_ARCH_ATOMIC_CPU_WMB_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_CPU_WMB_AVAILABLE 0
#endif


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ARCH_ATOMIC_CPU_RMB()
//
#if defined(EASTL_ARCH_ATOMIC_CPU_RMB)
	#define EASTL_ARCH_ATOMIC_CPU_RMB_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_CPU_RMB_AVAILABLE 0
#endif


