/////////////////////////////////////////////////////////////////////////////////
// copyright (c) electronic arts inc. all rights reserved.
/////////////////////////////////////////////////////////////////////////////////


#pragma once


/////////////////////////////////////////////////////////////////////////////////
//
// void EASTL_COMPILER_ATOMIC_CPU_PAUSE()
//
// NOTE:
// Rather obscure macro in Windows.h that expands to pause or rep; nop on
// compatible x86 cpus or the arm yield on compatible arm processors.
// This is nicer than switching on platform specific intrinsics.
//
#define EASTL_COMPILER_ATOMIC_CPU_PAUSE()		\
	YieldProcessor()

