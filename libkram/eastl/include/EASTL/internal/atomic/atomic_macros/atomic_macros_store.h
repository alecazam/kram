/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once

/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_ATOMIC_STORE_*_N(type, type * ptr, type val)
//
#define EASTL_ATOMIC_STORE_RELAXED_8(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELAXED_8)(type, ptr, val)

#define EASTL_ATOMIC_STORE_RELEASE_8(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELEASE_8)(type, ptr, val)

#define EASTL_ATOMIC_STORE_SEQ_CST_8(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_SEQ_CST_8)(type, ptr, val)


#define EASTL_ATOMIC_STORE_RELAXED_16(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELAXED_16)(type, ptr, val)

#define EASTL_ATOMIC_STORE_RELEASE_16(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELEASE_16)(type, ptr, val)

#define EASTL_ATOMIC_STORE_SEQ_CST_16(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_SEQ_CST_16)(type, ptr, val)


#define EASTL_ATOMIC_STORE_RELAXED_32(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELAXED_32)(type, ptr, val)

#define EASTL_ATOMIC_STORE_RELEASE_32(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELEASE_32)(type, ptr, val)

#define EASTL_ATOMIC_STORE_SEQ_CST_32(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_SEQ_CST_32)(type, ptr, val)


#define EASTL_ATOMIC_STORE_RELAXED_64(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELAXED_64)(type, ptr, val)

#define EASTL_ATOMIC_STORE_RELEASE_64(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELEASE_64)(type, ptr, val)

#define EASTL_ATOMIC_STORE_SEQ_CST_64(type, ptr, val)						\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_SEQ_CST_64)(type, ptr, val)


#define EASTL_ATOMIC_STORE_RELAXED_128(type, ptr, val)					\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELAXED_128)(type, ptr, val)

#define EASTL_ATOMIC_STORE_RELEASE_128(type, ptr, val)					\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_RELEASE_128)(type, ptr, val)

#define EASTL_ATOMIC_STORE_SEQ_CST_128(type, ptr, val)					\
	EASTL_ATOMIC_CHOOSE_OP_IMPL(ATOMIC_STORE_SEQ_CST_128)(type, ptr, val)
