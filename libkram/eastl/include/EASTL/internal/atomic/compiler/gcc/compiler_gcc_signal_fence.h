/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once


#define EASTL_GCC_ATOMIC_SIGNAL_FENCE(gccMemoryOrder)	\
	__atomic_signal_fence(gccMemoryOrder)


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_*()
//
#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_RELAXED()	\
	EASTL_GCC_ATOMIC_SIGNAL_FENCE(__ATOMIC_RELAXED)

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_ACQUIRE()	\
	EASTL_GCC_ATOMIC_SIGNAL_FENCE(__ATOMIC_ACQUIRE)

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_RELEASE()	\
	EASTL_GCC_ATOMIC_SIGNAL_FENCE(__ATOMIC_RELEASE)

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_ACQ_REL()	\
	EASTL_GCC_ATOMIC_SIGNAL_FENCE(__ATOMIC_ACQ_REL)

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_SEQ_CST()	\
	EASTL_GCC_ATOMIC_SIGNAL_FENCE(__ATOMIC_SEQ_CST)
