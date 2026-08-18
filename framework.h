#pragma once

#define WIN32_LEAN_AND_MEAN
#include <ctype.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <Winternl.h>
#include <unordered_set>
#include <filesystem>
#include "functional"
#include "Arxan.h"
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <TlHelp32.h>
#include "structs.h"
#include "minhook/include/MinHook.h"
#include "gscu_hashing.h"
#include "offsets.h"
#include "Protection.h"
#include "Hooks.h"

constexpr uint32_t fnv_base_32 = 0x4B9ACE2F;

inline uint32_t fnv1a(const char* key) {

	const char* data = key;
	uint32_t hash = 0x4B9ACE2F;
	while (*data)
	{
		hash ^= *data;
		hash *= 0x1000193;
		data++;
	}
	hash *= 0x1000193; // bo3 wtf lol
	return hash;

}

template <unsigned __int32 NUM>
struct canon_const_builtins
{
	static const unsigned __int32 value = NUM;
};

constexpr unsigned __int32 fnv1a_const(const char* input)
{
	const char* data = input;
	uint32_t hash = 0x4B9ACE2F;
	while (*data)
	{
		hash ^= *data;
		hash *= 0x01000193;
		data++;
	}
	hash *= 0x01000193; // bo3 wtf lol
	return hash;
}

#define FNV32(x) canon_const_builtins<fnv1a_const(x)>::value

inline bool is_in_array(const std::string& value, const std::vector<std::string>& allowed)
{
	for (const auto& candidate : allowed)
	{
		if (value == candidate)
			return true;
	}
	return false;
}

inline bool is_in_number_array(int value, const std::vector<int>& allowed)
{
	for (auto candidate : allowed)
	{
		if (value == candidate)
			return true;
	}
	return false;
}

inline std::string to_lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), ::tolower);
	return text;
}

inline bool is_equal(const std::string& lhs, const std::string& rhs, const std::size_t max_chars, const bool case_sensitive)
{
	auto left = lhs;
	auto right = rhs;

	if (max_chars != std::string::npos)
	{
		if (lhs.size() > max_chars)
			left.erase(max_chars);

		if (rhs.size() > max_chars)
			right.erase(max_chars);
	}

	if (case_sensitive)
	{
		return left == right;
	}

	else
	{
		return to_lower(left) == to_lower(right);
	}
}

inline bool is_address_within_range(std::uintptr_t address, std::uintptr_t min, std::uintptr_t max)
{
	return (address >= min && address <= max);
}

inline std::vector<std::string> legit_packets = {
	"connectResponse",
	"statresponse",
	"LM",
	"disconnect",
	"loadoutResponse",
	"infoResponse",
	"statusResponse",
	"keyAuthorize",
	"error",
	"print",
	"fastrestart",
	"ping",
	"pinga",
	"steamAuthReq",
	"cfl"
};

#define CRASH_LOG_NAME "crashes.log"
#define PATCH_CONFIG_LOCATION "t7patch.conf"

// Diagnostic trace log, written to t7patch_trace.log next to the game.
// Off unless t7patch.conf contains `debuglog=1`.
#define ZBR_TRACE_NAME "t7patch_trace.log"

inline bool g_trace_enabled = false;

// Opens and closes the file per line instead of holding a handle. That is slow
// and deliberate: a fault here ends in SuspendProcess, and the value of this log
// is the line written immediately before it. crashes.log cannot supply that - it
// prints a CONTEXT that is not one.
inline void zbr_trace(const char* fmt, ...)
{
	static SRWLOCK lock = SRWLOCK_INIT;

	// SRWLOCK is not recursive, and this is reachable from the exception handler. A fault
	// raised inside fopen/fprintf below would re-enter on the same thread and block forever,
	// which is a hang rather than a crash
	static thread_local bool in_trace = false;
	if (in_trace)
	{
		return;
	}
	in_trace = true;

	char line[512];
	va_list args;
	va_start(args, fmt);
	const int written = vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);

	if (written >= 0)
	{
		SYSTEMTIME t{};
		GetLocalTime(&t);

		AcquireSRWLockExclusive(&lock);
		if (FILE* f = fopen(ZBR_TRACE_NAME, "a"))
		{
			fprintf(f, "%02u:%02u:%02u.%03u t%-5lu %s\n",
				t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId(), line);
			fclose(f);
		}
		ReleaseSRWLockExclusive(&lock);
	}

	in_trace = false;
}

// Tested at the call site so arguments are not evaluated while logging is off.
#define ZLOG(...) do { if (g_trace_enabled) { zbr_trace(__VA_ARGS__); } } while (0)
#define ZBR_WINDOW_TEXT "Call of Duty: Black Ops III (community patch by serious)"
#define ZBR_VERSION_FULL "Patch 3.05 - by serious <3"
#define SPOOF_UNLOCK_ALL false
