/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////

#pragma once

#define EASTL_GCC_ATOMIC_FETCH_SUB_N(integralType, type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_INTRIN_N(integralType, __atomic_fetch_sub, type, ret, ptr, val, gccMemoryOrder)


#define EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_SUB_N(uint8_t, type, ret, ptr, val, gccMemoryOrder)

#define EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_SUB_N(uint16_t, type, ret, ptr, val, gccMemoryOrder)

#define EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_SUB_N(uint32_t, type, ret, ptr, val, gccMemoryOrder)

#define EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_SUB_N(uint64_t, type, ret, ptr, val, gccMemoryOrder)

#define EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, gccMemoryOrder) \
	EASTL_GCC_ATOMIC_FETCH_SUB_N(__uint128_t, type, ret, ptr, val, gccMemoryOrder)


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_FETCH_SUB_*_N(type, type ret, type * ptr, type val)
//
#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELAXED_8(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, __ATOMIC_RELAXED)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELAXED_16(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, __ATOMIC_RELAXED)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELAXED_32(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, __ATOMIC_RELAXED)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELAXED_64(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, __ATOMIC_RELAXED)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELAXED_128(type, ret, ptr, val) \
	EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, __ATOMIC_RELAXED)


#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQUIRE_8(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, __ATOMIC_ACQUIRE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQUIRE_16(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, __ATOMIC_ACQUIRE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQUIRE_32(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, __ATOMIC_ACQUIRE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQUIRE_64(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, __ATOMIC_ACQUIRE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQUIRE_128(type, ret, ptr, val) \
	EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, __ATOMIC_ACQUIRE)


#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELEASE_8(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, __ATOMIC_RELEASE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELEASE_16(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, __ATOMIC_RELEASE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELEASE_32(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, __ATOMIC_RELEASE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELEASE_64(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, __ATOMIC_RELEASE)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_RELEASE_128(type, ret, ptr, val) \
	EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, __ATOMIC_RELEASE)


#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQ_REL_8(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, __ATOMIC_ACQ_REL)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQ_REL_16(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, __ATOMIC_ACQ_REL)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQ_REL_32(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, __ATOMIC_ACQ_REL)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQ_REL_64(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, __ATOMIC_ACQ_REL)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_ACQ_REL_128(type, ret, ptr, val) \
	EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, __ATOMIC_ACQ_REL)


#define EASTL_COMPILER_ATOMIC_FETCH_SUB_SEQ_CST_8(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_8(type, ret, ptr, val, __ATOMIC_SEQ_CST)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_SEQ_CST_16(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_16(type, ret, ptr, val, __ATOMIC_SEQ_CST)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_SEQ_CST_32(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_32(type, ret, ptr, val, __ATOMIC_SEQ_CST)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_SEQ_CST_64(type, ret, ptr, val)	\
	EASTL_GCC_ATOMIC_FETCH_SUB_64(type, ret, ptr, val, __ATOMIC_SEQ_CST)

#define EASTL_COMPILER_ATOMIC_FETCH_SUB_SEQ_CST_128(type, ret, ptr, val) \
	EASTL_GCC_ATOMIC_FETCH_SUB_128(type, ret, ptr, val, __ATOMIC_SEQ_CST)
