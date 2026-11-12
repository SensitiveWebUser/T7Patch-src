// dsound.dll search-order-hijack proxy, built from source to replace a real Windows dsound.dll
// that used to be patched (import-table injection) to force-load a loader DLL as a side effect
// of the game loading DirectSound. That approach shipped Microsoft's own DirectSound
// implementation (proprietary, not something this project can rebuild from source); this proxy
// instead re-exports the same 12 DirectSound entry points as thin trampolines forwarding
// straight through to the real system dsound.dll, loaded explicitly from %WINDIR%\System32 so
// it can never resolve back to this file. Its DllMain loads t7patch.dll from an absolute path
// built from this module's own location (see DllMain for why the bare filename it used to pass
// worked only by coincidence); that alone maps it into the process, since patch/dllmain.cpp's
// own DllMain does nothing but DisableThreadLibraryCalls.
//
// Actually activating it - EnableInjectorlessInstall() then zbr_run_gamemode_lui, reaching
// RunPatching (the Arxan check, every hook install, the config watcher) - is NOT done
// synchronously here. dsound.dll's DllMain fires within milliseconds of process creation, long
// before the game finishes its own startup: an earlier version called both exports right here
// and produced field crashes (MH_ERROR_UNSUPPORTED_FUNCTION on most hooks, then
// STATUS_PRIVILEGED_INSTRUCTION) because Arxan hadn't unpacked the target code yet and systems
// like the lobby object didn't exist. The Windows injector exe reaches the same two exports
// safely because it polls for the game process first and a human typically reaches the main
// menu before triggering it manually - very different timing from firing on process creation.
// ActivationThreadProc reproduces that "give the game a head start" timing: DllMain starts it
// and returns immediately (never blocking the loader lock), and the thread waits for the
// game's own main window before activating. See AGENTS.md - t7patch.dll, this file, and
// t7patch.conf all have to sit loose in the game's install directory; that's inherent to how
// this search order works, not a choice made here.
//
// Every exported signature below is the public, Microsoft-documented DirectSound COM API
// (unchanged since DirectX's earliest versions) - parameters pass through as opaque
// pointer/integer values without this file ever needing their real types, since it never
// dereferences them; only the real dsound.dll (loaded from its real location) ever touches
// actual DirectSound objects. This keeps the file free of any dependency on dsound.h or other
// DirectX SDK headers - <windows.h> and plain (non-secure) <cstring> are the only includes
// needed.
//
// Exported by name only (__declspec(dllexport), no .def file) - every real caller binds by
// name, never by ordinal, so there's no need to match the original dsound.dll's ordinal
// numbers, and mixing a .def EXPORTS block with __declspec(dllexport) on the same symbols
// risks a linker conflict for no benefit.
#include <windows.h>
#include <cstring>

namespace {

HMODULE g_real = nullptr;
CRITICAL_SECTION g_lock;

// Set once at DLL_PROCESS_ATTACH before anything needs to log; read-only after that, so no
// synchronization needed on the read side. Lets RealModule()/RealProc() (no HINSTANCE of
// their own) log through the same AppendLogLine as everything else here.
HINSTANCE g_selfModule = nullptr;

// Best-effort de-dup so a persistently missing system dsound.dll (or a missing export) logs
// once per process, not once per trampoline call - unlike DllMain's one-shot load, these two
// paths are reachable from every DirectSound call for the life of the process. A benign race
// on first use can duplicate a line at most once; not worth a lock for that.
bool g_realModuleFailureLogged = false;
bool g_realProcFailureLogged = false;

// Writes value as decimal digits into buffer (room for a DWORD's 10 digits plus a null
// terminator), returning the digit count. Hand-rolled instead of wsprintfA/snprintf, which
// would need the secure-CRT define patch/'s build uses but this file deliberately doesn't.
int WriteDecimal(char *buffer, DWORD value)
{
    char digits[10];
    int digitCount = 0;
    do
    {
        digits[digitCount++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && digitCount < 10);

    for (int i = 0; i < digitCount; ++i)
    {
        buffer[i] = digits[digitCount - 1 - i];
    }
    return digitCount;
}

const char *const kLogName = "t7patch-dsound.log";

// Builds "<directory this DLL loaded from>\<leafName>" into buffer (room for MAX_PATH bytes).
// Returns false (buffer unusable) if the module path doesn't fit, has no directory separator,
// or the result would overrun - every caller treats that as "skip this, best-effort".
//
// Shared by the log file and the t7patch.dll load below: both want "the file next to me", and
// deriving it from this module's own path makes that a guarantee rather than something
// inferred from where the process happened to start.
bool BuildSiblingPath(HINSTANCE selfModule, const char *leafName, char *buffer)
{
    DWORD length = GetModuleFileNameA(selfModule, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        // length == MAX_PATH is how this reports truncation on older Windows (no error set),
        // so it is rejected along with the genuine failure rather than used truncated.
        return false;
    }

    char *lastSlash = strrchr(buffer, '\\');
    if (!lastSlash)
    {
        return false;
    }

    // The last byte written by the strcpy below is index (lastSlash - buffer) + 1 +
    // strlen(leafName), the null terminator, so >= MAX_PATH is the exact rejection bound.
    if (static_cast<size_t>(lastSlash - buffer) + 1 + strlen(leafName) >= MAX_PATH)
    {
        return false;
    }
    strcpy(lastSlash + 1, leafName);
    return true;
}

// Best-effort diagnostic: a plain text file next to this DLL recording what happened on a
// given launch. OutputDebugStringA alone needs an attached debugger nobody has - this is the
// only way a real user, or anyone helping them, can see what happened. Plain <cstring> only,
// no secure-CRT ("_s") calls.
void AppendLogLine(HINSTANCE selfModule, const char *text)
{
    char path[MAX_PATH];
    if (!BuildSiblingPath(selfModule, kLogName, path))
    {
        return;
    }

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    CloseHandle(file);
}

// "<prefix><decimal error code>.\r\n" - the shape of every Win32-failure line this file logs.
// Both call sites used to spell out the same hand-rolled sequence inline (strcpy the prefix,
// WriteDecimal the code, '.', CR, LF, NUL by index) with a hand-sized local buffer each; only
// the prefix text and code ever differed. The guard: 14 bytes is WriteDecimal's 10-digit max
// plus ".\r\n" and the terminator, so any prefix under 242 characters always formats in full.
// Both current prefixes are ~50; a longer one added later logs nothing rather than overrun,
// matching how the rest of this file treats a line that won't fit (see RealProc).
void AppendWin32ErrorLine(HINSTANCE selfModule, const char *prefix, DWORD errorCode)
{
    char line[256];
    if (strlen(prefix) + 14 > sizeof(line))
    {
        return;
    }

    int at = static_cast<int>(strlen(strcpy(line, prefix)));
    at += WriteDecimal(line + at, errorCode);
    strcpy(line + at, ".\r\n");
    AppendLogLine(selfModule, line);
}

// Every writer above appends and nothing truncates, so the log grows for the life of the
// install - unlike t7patch_trace.log, which rotates at 16 MB. That also makes the
// launch-to-launch boundary invisible, breaking the documented way of reading the log ("a
// 'loaded successfully' line with no activation line after it points at the activation step"
// only holds if the next line can't be the *next launch's* load line). This writes a boundary
// marker and first discards the file if it's grown past a generous cap - truncating before
// this launch's own first line keeps the current launch always present and complete; only old
// launches nobody was going to read are lost.
void BeginLaunchLog(HINSTANCE selfModule)
{
    const DWORD maxLogBytes = 64 * 1024;

    char path[MAX_PATH];
    if (BuildSiblingPath(selfModule, kLogName, path))
    {
        WIN32_FILE_ATTRIBUTE_DATA info;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &info)
            && (info.nFileSizeHigh != 0 || info.nFileSizeLow > maxLogBytes))
        {
            HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                CloseHandle(file);
            }
        }
    }

    // "--- launch: pid <n>, tick <n> ---": 16 + 10 + 7 + 10 + 6 + 1 = 50 bytes at most.
    char line[96];
    const char *prefix = "--- launch: pid ";
    int at = static_cast<int>(strlen(strcpy(line, prefix)));
    at += WriteDecimal(line + at, GetCurrentProcessId());
    at += static_cast<int>(strlen(strcpy(line + at, ", tick ")));
    at += WriteDecimal(line + at, GetTickCount());
    strcpy(line + at, " ---\r\n");
    AppendLogLine(selfModule, line);
}

void WriteLoaderLog(HINSTANCE selfModule, bool succeeded, DWORD loadError)
{
    if (succeeded)
    {
        AppendLogLine(selfModule, "t7patch.dll loaded successfully.\r\n");
    }
    else
    {
        AppendWin32ErrorLine(selfModule, "t7patch.dll failed to load - Win32 error code ", loadError);
    }
}

// True once any top-level, visible window belongs to this process - a proxy for "the game has
// reached at least its main menu", by which point Arxan unpacking and subsystems like the
// lobby object are expected to have settled. GetWindowThreadProcessId's out-param is who owns
// hwnd, not this module - safe to check unconditionally, no null guard needed.
BOOL CALLBACK FindOwnVisibleWindow(HWND hwnd, LPARAM foundFlag)
{
    DWORD ownerProcessId = 0;
    GetWindowThreadProcessId(hwnd, &ownerProcessId);
    if (ownerProcessId == GetCurrentProcessId() && IsWindowVisible(hwnd))
    {
        *reinterpret_cast<bool *>(foundFlag) = true;
        return FALSE;
    }
    return TRUE;
}

bool ProcessHasVisibleWindow()
{
    bool found = false;
    EnumWindows(FindOwnVisibleWindow, reinterpret_cast<LPARAM>(&found));
    return found;
}

// Runs on its own thread, started (and not waited on) from DllMain - see the file header for
// why this can't run inline in DLL_PROCESS_ATTACH. Polls for the game's main window, adds a
// fixed safety margin, then activates. Gives up after maxWaitMs and activates anyway rather
// than never activating - a slow or windowless launch shouldn't mean the patch silently never
// turns on.
DWORD WINAPI ActivationThreadProc(LPVOID param)
{
    HINSTANCE selfModule = reinterpret_cast<HINSTANCE>(param);

    HMODULE patch = GetModuleHandleA("t7patch.dll");
    if (!patch)
    {
        // Should never happen - DllMain only starts this thread after its own LoadLibraryA of
        // t7patch.dll already succeeded - but the sibling failure paths below both log, so
        // silently bailing here would be the odd one out.
        AppendLogLine(selfModule, "activation: t7patch.dll module handle not found; aborting activation.\r\n");
        return 0;
    }

    const DWORD pollIntervalMs = 250;
    const DWORD maxWaitMs = 120000;
    DWORD waited = 0;
    // The exit reason is recorded, not re-derived from `waited` afterward: the loop can exit on
    // the window check with `waited` already at exactly maxWaitMs (window appeared during the
    // final sleep), and "waited < maxWaitMs" would then report a timeout on a launch that
    // actually found the window. `waited` also undercounts real wall time by however long each
    // EnumWindows sweep took, so it's a poll budget, not a true elapsed-time cap.
    bool foundWindow = false;
    for (;;)
    {
        if (ProcessHasVisibleWindow())
        {
            foundWindow = true;
            break;
        }
        if (waited >= maxWaitMs)
        {
            break;
        }
        Sleep(pollIntervalMs);
        waited += pollIntervalMs;
    }
    AppendLogLine(selfModule, foundWindow
                                  ? "activation: main window detected, arming patch.\r\n"
                                  : "activation: gave up waiting for main window, arming patch anyway.\r\n");

    // Extra head start once the window exists: it can appear before menu assets and subsystems
    // like the lobby object have finished settling.
    Sleep(3000);

    using VoidFn = void (*)();
    using StringArgFn = void (*)(const char *);

    auto enableInjectorlessInstall =
        reinterpret_cast<VoidFn>(GetProcAddress(patch, "EnableInjectorlessInstall"));
    if (enableInjectorlessInstall)
    {
        enableInjectorlessInstall();
    }
    else
    {
        // Should never happen against a real t7patch.dll build (AGENTS.md section 3 lists this as
        // one of the six required exports), but a DLL missing an export loads fine and silently does
        // nothing - indistinguishable from "not working" without a log line saying so.
        AppendLogLine(selfModule, "activation: EnableInjectorlessInstall not found in t7patch.dll.\r\n");
    }

    auto runGamemodeLui =
        reinterpret_cast<StringArgFn>(GetProcAddress(patch, "zbr_run_gamemode_lui"));
    if (runGamemodeLui)
    {
        runGamemodeLui("serious_anticrash_2023");
    }
    else
    {
        AppendLogLine(selfModule, "activation: zbr_run_gamemode_lui not found in t7patch.dll.\r\n");
    }

    return 0;
}

// Loads the real dsound.dll once, by an explicit System32 path - never a bare "dsound.dll",
// which the default search order would resolve back to this very file (it sits next to
// BlackOps3.exe, ahead of System32 in the search order that made the hijack possible at all).
HMODULE RealModule()
{
    // Deliberately no unlocked fast-path read of g_real: a plain HMODULE isn't atomic, so reading
    // it outside the lock while another thread may be writing it inside (e.g. two DirectSound
    // entry points invoked concurrently at startup) is a data race under the C++ memory model,
    // even though it'd likely be benign in practice on x86-64's strong ordering. These
    // trampolines aren't a hot path, so always taking the lock costs nothing measurable.
    EnterCriticalSection(&g_lock);
    if (!g_real)
    {
        char systemDir[MAX_PATH];
        UINT length = GetSystemDirectoryA(systemDir, MAX_PATH);
        // 12 is strlen("\\dsound.dll") + 1 for the terminator, so the append below always fits.
        if (length > 0 && length < MAX_PATH - 12)
        {
            lstrcatA(systemDir, "\\dsound.dll");
            g_real = LoadLibraryA(systemDir);
        }
        if (!g_real && !g_realModuleFailureLogged)
        {
            // Should never happen on a real Windows install (see the E_FAIL_HRESULT remark
            // below) - logged once, not per-call, since every one of the 12 trampolines hits
            // this same path for the rest of the process if it ever does happen.
            g_realModuleFailureLogged = true;
            AppendLogLine(g_selfModule, "RealModule: could not load the system dsound.dll.\r\n");
        }
    }
    HMODULE result = g_real;
    LeaveCriticalSection(&g_lock);
    return result;
}

FARPROC RealProc(const char *name)
{
    HMODULE real = RealModule();
    if (!real)
    {
        // RealModule() already logged this case.
        return nullptr;
    }

    FARPROC proc = GetProcAddress(real, name);
    if (!proc && !g_realProcFailureLogged)
    {
        char line[128];
        const char *prefix = "RealProc: export not found in system dsound.dll: ";
        size_t prefixLength = strlen(prefix);
        size_t nameLength = strlen(name);
        // Set inside the fit check, not before it: a name too long to format would otherwise
        // latch the de-dup flag without logging anything, silencing every later (loggable)
        // failure too. Unreachable with the 12 real names (longest is 27 characters against a
        // 76-character budget), but the flag exists only to avoid repeating a line, not to
        // suppress one that was never written.
        if (prefixLength + nameLength + 3 < sizeof(line))
        {
            g_realProcFailureLogged = true;
            strcpy(line, prefix);
            strcpy(line + prefixLength, name);
            strcpy(line + prefixLength + nameLength, "\r\n");
            AppendLogLine(g_selfModule, line);
        }
    }
    return proc;
}

} // namespace

// One trampoline per real DirectSound export (same 12 entry points as the genuine dsound.dll,
// exported by name - see the header remarks on why ordinals aren't preserved) - each resolves
// and tail-calls the real function. HRESULT is a 32-bit signed integer; E_FAIL (0x80004005) is
// returned only if the real system dsound.dll couldn't be found or loaded, which should never
// happen on a real Windows install.
#define E_FAIL_HRESULT ((HRESULT)0x80004005L)

extern "C" {

__declspec(dllexport) HRESULT WINAPI DirectSoundCreate(const void *guid, void **ppDS, void *pUnkOuter)
{
    using Fn = HRESULT(WINAPI *)(const void *, void **, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCreate"));
    return fn ? fn(guid, ppDS, pUnkOuter) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundEnumerateA(void *callback, void *context)
{
    using Fn = HRESULT(WINAPI *)(void *, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundEnumerateA"));
    return fn ? fn(callback, context) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundEnumerateW(void *callback, void *context)
{
    using Fn = HRESULT(WINAPI *)(void *, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundEnumerateW"));
    return fn ? fn(callback, context) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void)
{
    using Fn = HRESULT(WINAPI *)(void);
    auto fn = reinterpret_cast<Fn>(RealProc("DllCanUnloadNow"));
    return fn ? fn() : E_FAIL_HRESULT;
}

// <windows.h> (via combaseapi.h) already forward-declares this exact COM entry point for any
// DLL that might be a COM server - STDAPI DllGetClassObject(REFCLSID, REFIID, LPVOID*) - as
// "extern C" with REFCLSID/REFIID as C++ reference types, not pointers. Redeclaring it here
// with different (if ABI-compatible) parameter types is a hard conflicting-declaration error,
// not a style mismatch, so this one signature has to match exactly. DllCanUnloadNow above is
// pre-declared by the same header (STDAPI DllCanUnloadNow(void)) and already matches, so it
// needs nothing; the remaining 10 exports have no pre-existing declaration at all (DirectSound's
// types live in dsound.h, which nothing here includes) and keep the generic void* treatment.
__declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    using Fn = HRESULT(WINAPI *)(const void *, const void *, void **);
    auto fn = reinterpret_cast<Fn>(RealProc("DllGetClassObject"));
    return fn ? fn(&rclsid, &riid, ppv) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCaptureCreate(const void *guid, void **ppDSC, void *pUnkOuter)
{
    using Fn = HRESULT(WINAPI *)(const void *, void **, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCaptureCreate"));
    return fn ? fn(guid, ppDSC, pUnkOuter) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCaptureEnumerateA(void *callback, void *context)
{
    using Fn = HRESULT(WINAPI *)(void *, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCaptureEnumerateA"));
    return fn ? fn(callback, context) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCaptureEnumerateW(void *callback, void *context)
{
    using Fn = HRESULT(WINAPI *)(void *, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCaptureEnumerateW"));
    return fn ? fn(callback, context) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI GetDeviceID(const void *guidSrc, void *guidDest)
{
    using Fn = HRESULT(WINAPI *)(const void *, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("GetDeviceID"));
    return fn ? fn(guidSrc, guidDest) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundFullDuplexCreate(
    const void *captureGuid, const void *renderGuid, const void *captureFormat,
    const void *renderFormat, HWND hwnd, DWORD level, void **ppDSFD, void **ppCapture,
    void **ppRender, void *pUnkOuter)
{
    using Fn = HRESULT(WINAPI *)(const void *, const void *, const void *, const void *, HWND,
                                  DWORD, void **, void **, void **, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundFullDuplexCreate"));
    return fn ? fn(captureGuid, renderGuid, captureFormat, renderFormat, hwnd, level, ppDSFD,
                    ppCapture, ppRender, pUnkOuter)
              : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCreate8(const void *guid, void **ppDS8, void *pUnkOuter)
{
    using Fn = HRESULT(WINAPI *)(const void *, void **, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCreate8"));
    return fn ? fn(guid, ppDS8, pUnkOuter) : E_FAIL_HRESULT;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCaptureCreate8(const void *guid, void **ppDSC8, void *pUnkOuter)
{
    using Fn = HRESULT(WINAPI *)(const void *, void **, void *);
    auto fn = reinterpret_cast<Fn>(RealProc("DirectSoundCaptureCreate8"));
    return fn ? fn(guid, ppDSC8, pUnkOuter) : E_FAIL_HRESULT;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE selfModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_selfModule = selfModule;
        InitializeCriticalSection(&g_lock);
        BeginLaunchLog(selfModule);

        // Pin this DLL against FreeLibrary. It's an implicitly-linked search-order-hijack
        // replacement for a system DLL, so nothing should unload it before process exit - but
        // nothing enforces that, and ActivationThreadProc runs for up to two minutes after
        // this returns while the 12 trampolines stay reachable for the process's life, so an
        // unload landing on either would execute unmapped code. Pinning removes the only
        // realistic route to that: an explicit FreeLibrary from elsewhere in the process.
        // GetModuleHandleExA with an address inside this module is the standard way to pin
        // without a module name; the result is checked so a failure is visible in the log
        // rather than assumed away.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                reinterpret_cast<LPCSTR>(&DllMain), &pinned))
        {
            AppendLogLine(selfModule, "warning: could not pin this module against unload.\r\n");
        }

        // An absolute "<this DLL's own directory>\t7patch.dll", with the bare name as fallback.
        // The bare name alone did work, but for a reason this file used to state incorrectly:
        // the default search order for an unqualified LoadLibraryA name begins with the
        // *process executable's* directory, not the calling module's own directory (that needs
        // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR / SetDefaultDllDirectories, neither used here). Those
        // two directories coincide in a real install - the hijack requires this DLL next to
        // BlackOps3.exe - so it resolved correctly by coincidence, and would stop working in a
        // process that called SetDefaultDllDirectories. Deriving the path from this module
        // makes "the t7patch.dll next to me" the actual request. The leaf name stays literal
        // lowercase "t7patch.dll" either way, which a case-sensitive Proton filesystem needs
        // (AGENTS.md section 1).
        //
        // Calling LoadLibraryA from DLL_PROCESS_ATTACH is generally discouraged (loader-lock
        // deadlock risk if the loaded DLL does unusual things in its own DllMain) - provably
        // safe here though: t7patch.dll's DllMain does nothing but DisableThreadLibraryCalls
        // (patch/dllmain.cpp), so there's nothing in it that could hang or misbehave nested
        // inside this one.
        char patchPath[MAX_PATH];
        HMODULE patch = BuildSiblingPath(selfModule, "t7patch.dll", patchPath)
                            ? LoadLibraryA(patchPath)
                            : nullptr;
        if (!patch)
        {
            patch = LoadLibraryA("t7patch.dll");
        }
        DWORD loadError = patch ? 0 : GetLastError();
        WriteLoaderLog(selfModule, patch != nullptr, loadError);
        OutputDebugStringA(patch ? "t7patch: dsound proxy loaded t7patch.dll successfully."
                                  : "t7patch: dsound proxy could not load t7patch.dll.");

        // Activation (EnableInjectorlessInstall + zbr_run_gamemode_lui, reaching RunPatching) is
        // deliberately NOT called here - see the file header. It happens on ActivationThreadProc,
        // started here without waiting on it, so DllMain returns immediately either way.
        if (patch)
        {
            HANDLE activationThread = CreateThread(nullptr, 0, ActivationThreadProc, selfModule, 0, nullptr);
            if (activationThread)
            {
                CloseHandle(activationThread);
            }
            else
            {
                // Every other failure path in this file logs (WriteLoaderLog above, the two
                // GetProcAddress misses in ActivationThreadProc) - this one silently left the
                // patch never-activated with nothing in the log to explain why.
                AppendWin32ErrorLine(selfModule, "activation: CreateThread failed - Win32 error code ",
                                     GetLastError());
            }
        }
    }
    // Deliberately no DLL_PROCESS_DETACH handling. It used to DeleteCriticalSection(&g_lock),
    // documented undefined behavior if the section is still owned - and one real teardown path
    // reaches here with a live owner: ExitProcess *terminates* other threads where they stand
    // rather than draining them, so a thread killed inside RealModule() (which holds g_lock
    // across a LoadLibraryA) leaves the section owned by a thread that no longer exists. patch/'s
    // exception path calls exit(0) (AGENTS.md section 11), so this is reachable, not hypothetical.
    // Nothing needs releasing at process exit anyway: the address space goes away wholesale, so a
    // CRITICAL_SECTION left initialized costs nothing, and not touching it removes the entire
    // class of ordering questions.
    return TRUE;
}
