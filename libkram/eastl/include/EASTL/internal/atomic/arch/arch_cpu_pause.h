/////////////////////////////////////////////////////////////////////////////////
// copyright (c) electronic arts inc. all rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once

/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ARCH_ATOMIC_CPU_PAUSE()
//
#if defined(EASTL_ARCH_ATOMIC_CPU_PAUSE)
	#define EASTL_ARCH_ATOMIC_CPU_PAUSE_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_CPU_PAUSE_AVAILABLE 0
#endif