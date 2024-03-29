/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ARCH_ATOMIC_THREAD_FENCE_*()
//
#if defined(EASTL_ARCH_ATOMIC_THREAD_FENCE_RELAXED)
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_RELAXED_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_RELAXED_AVAILABLE 0
#endif

#if defined(EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQUIRE)
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQUIRE_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQUIRE_AVAILABLE 0
#endif

#if defined(EASTL_ARCH_ATOMIC_THREAD_FENCE_RELEASE)
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_RELEASE_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_RELEASE_AVAILABLE 0
#endif

#if defined(EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQ_REL)
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQ_REL_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_ACQ_REL_AVAILABLE 0
#endif

#if defined(EASTL_ARCH_ATOMIC_THREAD_FENCE_SEQ_CST)
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_SEQ_CST_AVAILABLE 1
#else
	#define EASTL_ARCH_ATOMIC_THREAD_FENCE_SEQ_CST_AVAILABLE 0
#endif
