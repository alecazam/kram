/////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once

namespace eastl
{


namespace internal
{


struct memory_order_relaxed_s {};
struct memory_order_read_depends_s {};
struct memory_order_acquire_s {};
struct memory_order_release_s {};
struct memory_order_acq_rel_s {};
struct memory_order_seq_cst_s {};


} // namespace internal


EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_relaxed = internal::memory_order_relaxed_s{};
EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_read_depends = internal::memory_order_read_depends_s{};
EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_acquire = internal::memory_order_acquire_s{};
EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_release = internal::memory_order_release_s{};
EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_acq_rel = internal::memory_order_acq_rel_s{};
EASTL_CPP17_INLINE_VARIABLE constexpr auto memory_order_seq_cst = internal::memory_order_seq_cst_s{};


} // namespace eastl
