/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include <EABase/eabase.h>

namespace eastl
{
	///////////////////////////////////////////////////////////////////////////////
	/// piecewise_construct_t
	///
	/// http://en.cppreference.com/w/cpp/utility/piecewise_construct_t
	///
	struct piecewise_construct_t
	{
		explicit piecewise_construct_t() = default;
	};


	///////////////////////////////////////////////////////////////////////////////
	/// piecewise_construct
	/// 
	/// A tag type used to disambiguate between function overloads that take two tuple arguments.
	///
	/// http://en.cppreference.com/w/cpp/utility/piecewise_construct
	///
	EA_CONSTEXPR piecewise_construct_t piecewise_construct = eastl::piecewise_construct_t();

} // namespace eastl






