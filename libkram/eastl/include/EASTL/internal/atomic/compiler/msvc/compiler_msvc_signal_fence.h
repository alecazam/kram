/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////

#pragma once


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_*()
//
#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_RELAXED()	\
	EASTL_ATOMIC_COMPILER_BARRIER()

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_ACQUIRE()	\
	EASTL_ATOMIC_COMPILER_BARRIER()

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_RELEASE()	\
	EASTL_ATOMIC_COMPILER_BARRIER()

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_ACQ_REL()	\
	EASTL_ATOMIC_COMPILER_BARRIER()

#define EASTL_COMPILER_ATOMIC_SIGNAL_FENCE_SEQ_CST()	\
	EASTL_ATOMIC_COMPILER_BARRIER()

