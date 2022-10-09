/*
 * LegacyClonk
 *
 * Copyright (c) 1998-2000, Matthes Bender (RedWolf Design)
 * Copyright (c) 2017-2021, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

/* All kinds of valuable helpers */

#pragma once

#include "C4Breakpoint.h"
#include "C4Chrono.h"
#include "C4Math.h"
#include "C4Strings.h"
#include "StdColors.h"
#include "StdFile.h"

// These functions have to be provided by the application.
bool Log(const char *szMessage);
bool LogSilent(const char *szMessage);

#include <algorithm>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

template<std::integral To, std::integral From>
To checked_cast(From from)
{
	if constexpr (std::is_signed_v<From>)
	{
		if constexpr (std::is_unsigned_v<To>)
		{
			if (from < 0)
			{
				throw std::runtime_error{"Conversion of negative value to unsigned type requested"};
			}
		}
		else if constexpr (std::numeric_limits<From>::min() < std::numeric_limits<To>::min())
		{
			if (from < std::numeric_limits<To>::min())
			{
				throw std::runtime_error{"Conversion of value requested that is smaller than the target-type minimum"};
			}
		}
	}

	if constexpr (std::numeric_limits<From>::max() > std::numeric_limits<To>::max())
	{
		if (std::cmp_greater(from, std::numeric_limits<To>::max()))
		{
			throw std::runtime_error{"Conversion of value requested that is bigger than the target-type maximum"};
		}
	}

	return static_cast<To>(from);
}
