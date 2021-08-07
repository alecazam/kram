/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once

/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ATOMIC_THREAD_FENCE_*()
//
#define EASTL_ATOMIC_THREAD_FENCE_RELAXED()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_THREAD_FENCE_RELAXED)()

#define EASTL_ATOMIC_THREAD_FENCE_ACQUIRE()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_THREAD_FENCE_ACQUIRE)()

#define EASTL_ATOMIC_THREAD_FENCE_RELEASE()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_THREAD_FENCE_RELEASE)()

#define EASTL_ATOMIC_THREAD_FENCE_ACQ_REL()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_THREAD_FENCE_ACQ_REL)()

#define EASTL_ATOMIC_THREAD_FENCE_SEQ_CST()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_THREAD_FENCE_SEQ_CST)()
