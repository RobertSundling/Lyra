// Copyright René Ferdinand Rivera Morell
// Copyright 2017 Two Blue Cubes Ltd. All rights reserved.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef LYRA_DETAIL_FROM_STRING_HPP
#define LYRA_DETAIL_FROM_STRING_HPP

#include "lyra/detail/trait_utils.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <type_traits>

#ifndef LYRA_CONFIG_OPTIONAL_TYPE
#	if defined(__has_include) && __has_include(<version>)
#		include <version>
#	elif defined(__has_include) && __has_include(<ciso646>)
#		include <ciso646>
#	endif
#	if defined(__has_include) && __has_include(<optional>) \
		&& defined(__cpp_lib_optional) && (__cpp_lib_optional >= 201606L)
#		include <optional>
#		define LYRA_CONFIG_OPTIONAL_TYPE std::optional
#	endif
#endif

namespace lyra { namespace detail {

template <typename T>
bool to_string(const T & source, std::string & target)
{
	std::stringstream ss;
	ss << source;
	ss >> target;
	return !ss.fail();
}

inline bool to_string(const std::string & source, std::string & target)
{
	target = source;
	return true;
}

inline bool to_string(const char * source, std::string & target)
{
	target = source;
	return true;
}

inline bool to_string(bool source, std::string & target)
{
	target = source ? "true" : "false";
	return true;
}

#ifdef LYRA_CONFIG_OPTIONAL_TYPE
template <typename T>
inline bool to_string(
	LYRA_CONFIG_OPTIONAL_TYPE<T> & source, std::string & target)
{
	if (source)
		return to_string(*source, target);
	else
		target = "<nullopt>";
	return true;
}
#endif // LYRA_CONFIG_OPTIONAL_TYPE

template <typename, typename = void>
struct is_convertible_from_string : std::false_type
{};

template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<std::is_arithmetic<T>::value>::type>
	: std::true_type
{};

// Validates format of given value strings before conversion. This default
// template return true always.
template <typename, typename = void>
struct validate_from_string
{
	static bool validate(const std::string &) { return true; }
};

template <typename S, typename T>
inline bool from_string(S const & source, T & target)
{
	// This requires C++17 for "if constexpr."
    if constexpr (std::is_integral_v<std::remove_cvref_t<T>>) {
		// First, put source into a string
		std::string src_str;
		to_string(source, src_str);

		// We allow:
		//
		// 1. An "h" suffix -- this means everything before it is a hex number
		// 2. A "0x" prefix -- this means everything after it is a hex number
		// 3. A "0b" prefix -- this means everything after it is a binary number
		// 4. A "0o" prefix -- this means everything after it is an octal number
		//
		// We do not allow octal with just an 0 prefix (such as 051) because that
		// prevents using numbers with leading zeros in decimal.
		//
		// Technically this will also allow numbers like "0x123h" because the
		// "h" is stripped first, then the radix is set to 16. When that is the
		// case, stoll then allows an optional "0x" prefix.

		int radix = 10; // Default radix is decimal

		if (src_str.size() > 1 && src_str[src_str.size() - 1] == 'h')
		{
			radix = 16; // Hexadecimal
			src_str.erase(src_str.size() - 1); // Remove the 'h' suffix
		}
		else if (src_str.size() > 2 && src_str[0] == '0')
		{
			if (src_str[1] == 'x')
			{
				radix = 16; // Hexadecimal
				src_str.erase(0, 2); // Remove the "0x" prefix
			}
			else if (src_str[1] == 'b')
			{
				radix = 2; // Binary
				src_str.erase(0, 2); // Remove the "0b" prefix
			}
			else if (src_str[1] == 'o')
			{
				radix = 8; // Octal
				src_str.erase(0, 2); // Remove the "0o" prefix
			}
		}
		
		// Now convert the string to the target type, using the specified radix
		std::size_t pos = 0;
		auto value = std::stoll(src_str, &pos, radix);

		// Check that the value is in range for the target type
		if (pos != src_str.size() || value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
		{
			return false; // Conversion failed or out of range
		}

		target = static_cast<T>(value);
		return true; // Conversion succeeded
    } else {
		std::stringstream ss;
		// Feed what we want to convert into the stream so that we can convert it
		// on extraction to the target type.
		ss << source;
		// Check that the source string data is valid. This check depends on the
		// target type.
		if (!validate_from_string<T>::validate(ss.str())) return false;
		T temp {};
		ss >> temp;
		if (!ss.fail() && ss.eof())
		{
			target = temp;
			return true;
		}
		return false;
    }
}

template <typename S, typename... C>
inline bool from_string(S const & source, std::basic_string<C...> & target)
{
	to_string(source, target);
	return true;
}

template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<std::is_same<T, bool>::value>::type>
	: std::true_type
{};

template <typename S>
inline bool from_string(S const & source, bool & target)
{
	std::string srcLC;
	to_string(source, srcLC);
	for (std::string::value_type & c : srcLC)
		c = static_cast<std::string::value_type>(std::tolower(c));
	if (srcLC == "y" || srcLC == "1" || srcLC == "true" || srcLC == "yes"
		|| srcLC == "on")
		target = true;
	else if (srcLC == "n" || srcLC == "0" || srcLC == "false" || srcLC == "no"
		|| srcLC == "off")
		target = false;
	else
		return false;
	return true;
}

#ifdef LYRA_CONFIG_OPTIONAL_TYPE
template <typename T>
struct is_convertible_from_string<T,
	typename std::enable_if<
		is_specialization_of<T, LYRA_CONFIG_OPTIONAL_TYPE>::value>::type>
	: std::true_type
{};

template <typename S, typename T>
inline bool from_string(S const & source, LYRA_CONFIG_OPTIONAL_TYPE<T> & target)
{
	std::string srcLC;
	to_string(source, srcLC);
	for (std::string::value_type & c : srcLC)
		c = static_cast<std::string::value_type>(::tolower(c));
	if (srcLC == "<nullopt>")
	{
		target.reset();
		return true;
	}
	else
	{
		T temp;
		auto str_result = from_string(source, temp);
		if (str_result) target = std::move(temp);
		return str_result;
	}
}
#endif // LYRA_CONFIG_OPTIONAL_TYPE

}} // namespace lyra::detail

#endif
