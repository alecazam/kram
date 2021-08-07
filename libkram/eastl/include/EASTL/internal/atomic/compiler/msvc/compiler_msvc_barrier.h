/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once

/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_COMPILER_BARRIER()
//
#define EASTL_COMPILER_ATOMIC_COMPILER_BARRIER()	\
	_ReadWriteBarrier()


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY(const T&, type)
//
#define EASTL_COMPILER_ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY(val, type) \
	EASTL_COMPILER_ATOMIC_COMPILER_BARRIER_DATA_DEPENDENCY_FUNC(const_cast<type*>(eastl::addressof((val)))); \
	EASTL_ATOMIC_COMPILER_BARRIER()

