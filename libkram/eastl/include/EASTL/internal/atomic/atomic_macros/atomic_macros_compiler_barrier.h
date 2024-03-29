/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ATOMIC_COMPILER_BARRIER()
//
#define EASTL_ATOMIC_COMPILER_BARRIER()						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_COMPILER_BARRIER)()


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY(const T&, type)
//
#define EASTL_ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY(val, type)		\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY)(val, type)

