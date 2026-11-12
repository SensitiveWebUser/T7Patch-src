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
#include <atomic>
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

// Both helpers sit on per-packet and per-frame paths, so the vectors are taken by reference rather
// than copied - every element and string - on each call. strcmp is kept deliberately: it stops at
// an embedded NUL where std::string's == would not, and that is the comparison upstream ships.
inline bool is_in_array(const std::string& cmp1, const std::vector<std::string>& cmp2)
{
	for (const auto& cmp : cmp2)
	{
		if (!strcmp(cmp1.data(), cmp.data()))
			return true;
	}
	return false;
}

inline bool is_in_number_array(int cmp1, const std::vector<int>& cmp2)
{
	for (auto cmp : cmp2)
	{
		if (cmp1 == cmp)
			return true;
	}
	return false;
}

inline std::string to_lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), ::tolower);
	return text;
}

inline bool is_equal(const std::string& lhs, const std::string& rhs, const std::size_t count, const bool case_sensitive)
{
	auto left = lhs;
	auto right = rhs;

	if (count != std::string::npos)
	{
		if (lhs.size() > count)
			left.erase(count);

		if (rhs.size() > count)
			right.erase(count);
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

// Diagnostic trace log, written to t7patch_trace.log next to the game. Off unless t7patch.conf
// contains `debuglog=1` - a key upstream's own parser ignores, so a conf carrying it still works
// with an upstream DLL.
#define ZBR_TRACE_NAME "t7patch_trace.log"

// Written by the config watcher thread, read from every game thread that reaches a ZLOG.
inline std::atomic<bool> g_trace_enabled{ false };

// Rotated to .1 rather than truncated once past this, so a long session cannot fill the disk and
// the run before the current one survives.
#define ZBR_TRACE_MAX_BYTES (16 * 1024 * 1024)

// Opens and closes the file per line instead of holding a handle. That is slow and deliberate: a
// fault ends in SuspendProcess, and the value of this log is the line written immediately before
// it, which a cached handle can lose in the CRT buffer. crashes.log cannot supply that - it prints
// a CONTEXT that is not one.
inline void zbr_trace(const char* fmt, ...)
{
	static SRWLOCK lock = SRWLOCK_INIT;

	// Required, not defensive: SRWLOCK is not recursive, so a fault raised inside fopen/fprintf
	// below that comes back through here on the same thread would block forever - a hang rather
	// than a crash, so nothing after it would ever run.
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
		FILE* traceFile = fopen(ZBR_TRACE_NAME, "a");
		if (traceFile)
		{
			fseek(traceFile, 0, SEEK_END);
			if (ftell(traceFile) >= ZBR_TRACE_MAX_BYTES)
			{
				fclose(traceFile);
				remove(ZBR_TRACE_NAME ".1");
				rename(ZBR_TRACE_NAME, ZBR_TRACE_NAME ".1");
				traceFile = fopen(ZBR_TRACE_NAME, "a");
			}
		}
		if (traceFile)
		{
			fprintf(traceFile, "%02u:%02u:%02u.%03u t%-5lu %s\n",
				t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId(), line);
			fclose(traceFile);
		}
		ReleaseSRWLockExclusive(&lock);
	}

	in_trace = false;
}

// Tested at the call site so a disabled log costs one relaxed load: the arguments are not even
// evaluated.
#define ZLOG(...) do { if (g_trace_enabled.load(std::memory_order_relaxed)) { zbr_trace(__VA_ARGS__); } } while (0)

// The one definition of what a `debuglog` line means, shared by the early reader below and by
// patch_config::loadfrom so the two can never disagree. Returns false when the line is not a
// debuglog assignment at all; otherwise sets `enabled`.
//
// injector/core/src/config.rs mirrors this exactly; change both or neither:
//  - a UTF-8 BOM is stripped from the first line only (a Notepad-saved conf with debuglog first
//    would otherwise read as "\xEF\xBB\xBFdebuglog");
//  - the key is trimmed of space/tab/CR/LF and matched case-sensitively;
//  - the value goes through istream >> int: leading whitespace, optional sign, digits, anything
//    after ignored. Enabled only if that extraction succeeds and is non-zero - so "true", empty,
//    and an out-of-range number (failbit) are all off.
inline bool zbr_parse_debuglog_line(std::string line, bool firstLine, bool& enabled)
{
	if (firstLine && line.size() >= 3 && (unsigned char)line[0] == 0xEF
		&& (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
	{
		line.erase(0, 3);
	}

	const auto sep = line.find('=');
	if (sep == std::string::npos)
	{
		return false;
	}

	auto key = line.substr(0, sep);
	const auto first = key.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
	{
		return false;
	}
	key = key.substr(first, key.find_last_not_of(" \t\r\n") - first + 1);
	if (key != "debuglog")
	{
		return false;
	}

	int value = 0;
	std::istringstream reader(line.substr(sep + 1));
	enabled = (reader >> value) && value != 0;
	return true;
}

// Runs first in RunPatching. The full parser only runs from Protection::install(), the last step,
// so without this every install-time diagnostic - the Arxan build check, MinHook init, each failed
// hook - was written while tracing was still off and thrown away. That matters most after a game
// update on Proton, where a message box is often invisible and this log is the only artifact.
//
// It reads ONLY debuglog and touches nothing else. Routing it through apply_settings would call
// SetPlayerName, which writes two game addresses before the build check has verified the exe.
inline void zbr_enable_trace_from_config()
{
	try
	{
		std::ifstream infile(PATCH_CONFIG_LOCATION, std::ifstream::in | std::ifstream::binary);
		if (!infile.is_open())
		{
			return;
		}

		// A duplicated key lets the last occurrence win, as it does in the full parser.
		bool enabled = false;
		bool firstLine = true;
		std::string line;
		while (std::getline(infile, line))
		{
			bool lineEnabled = false;
			if (zbr_parse_debuglog_line(line, firstLine, lineEnabled))
			{
				enabled = lineEnabled;
			}
			firstLine = false;
		}
		g_trace_enabled.store(enabled, std::memory_order_relaxed);
	}
	catch (...)
	{
		// No-Op: tracing is a diagnostic. Never let reading it stop the patch from installing.
	}
}

// MH_CreateHook's status is otherwise discarded at every call site. A hook that fails to install
// removes whatever it was protecting with no crash and no message.
inline void zbr_create_hook_checked(LPVOID target, LPVOID detour, LPVOID* original, const char* detourName)
{
	const MH_STATUS status = MH_CreateHook(target, detour, original);
	if (status != MH_OK)
	{
		ZLOG("hook: FAILED %s (MH_STATUS=%d)", detourName, (int)status);
	}
}

// A macro only so the log carries the detour's name: #detour needs the preprocessor.
#define CREATE_HOOK(target, detour, original) zbr_create_hook_checked((target), (detour), (original), #detour)
#define ZBR_WINDOW_TEXT "Call of Duty: Black Ops III (community patch by serious)"
#define ZBR_VERSION_FULL "Patch 3.09 - by serious <3"
#define SPOOF_OWNERSHIP false
#define SPOOF_RANKED false
#define SPOOF_UNLOCK false
#define SPOOF_SKIP_CWL false
#define SPOOF_GUM_COUNT 255
