#include "framework.h"

typedef LONG(NTAPI* NtSuspendProcess)(IN HANDLE ProcessHandle);
void SuspendProcess()
{
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());

    NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(
        GetModuleHandleA("ntdll"), "NtSuspendProcess");

    // Both looked up rather than assumed: this runs from the crash handler, where a call through a
    // null pointer is a second fault inside the first.
    if (pfnNtSuspendProcess && processHandle)
    {
        pfnNtSuspendProcess(processHandle);
    }

    if (processHandle)
    {
        CloseHandle(processHandle);
    }
}

typedef unsigned long long(__fastcall* ZwContinue_t)(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);
ZwContinue_t ZwContinue = reinterpret_cast<ZwContinue_t>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwContinue"));

// Static rather than malloc'd on first use: the LuaCrash2/3 handlers below hand this to the game
// from inside the exception handler, where the faulting thread may hold the heap lock. They are
// live now that Protection::ExceptHook no longer pre-empts them (see there); the lazy malloc
// blocks are left in place and simply never run.
static char g_uiLocalizeDefaultStorage[4] = "";
char* UILocalizeDefaultText = g_uiLocalizeDefaultStorage;

struct debug_msg_info
{
    __int64 timeEmitted;
    __int32 numEmitted;
};

std::unordered_map<INT64, CONTEXT> SavedExceptions;
std::unordered_map<DWORD, bool> EncounteredIPs;
std::unordered_map<DWORD, debug_msg_info> PrevErrors;
#define DMSG_COOLDOWN_TICKS 5000
// Thread id of the crash-dump writer, claimed with an interlocked compare so two threads faulting
// at once cannot both write crashes.log or both walk SavedExceptions.
volatile LONG g_crashDumpOwnerTid = 0;
char error_msg_buff[1024];
const char clientfield_doesnt_exist[] = "Clientfield does not exist";
// Set by Unload() on its own thread, read by ExceptHook on whichever thread faults.
std::atomic<bool> is_unhooking{ false };

void __nullsub()
{
    return;
}

void ExceptHook(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{

    if (is_unhooking.load())
    {
        ContextRecord->Dr7 = 0;
        ContextRecord->Rip = ROP_RETN;
        is_unhooking.store(false);
        ZwContinue(ContextRecord, false);
    }

    Protection::ExceptHook(ExceptionRecord, ContextRecord);

    if ((INT64)ContextRecord->Rcx == 0xFFEEDDCC44332211)
    {
        return;
    }

    if (REBASE(0x22D2F0D) == (INT64)ExceptionRecord->ExceptionAddress) // killcam anim crash
    {
        ContextRecord->Rax = 0;
        ContextRecord->Rip = REBASE(0x22D389E);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (REBASE(0x464FEF) == (INT64)ExceptionRecord->ExceptionAddress) // CG_ZBarrierAttachWeapon
    {
        ContextRecord->Rax = 0;
        ContextRecord->Rip = REBASE(0x4651A2);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (REBASE(0x15E4B7A) == (INT64)ExceptionRecord->ExceptionAddress) // asmsetanimationrate
    {
        ContextRecord->Rip = REBASE(0x15E4BA3);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x12EE4EC)) // weird orphaned thread crash
    {
        ContextRecord->Rip = REBASE(0x12EE5E8);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x22C965C)) // character index crash
    {
        ContextRecord->Rip = REBASE(0x22C9686);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)LuaCrash1)
    {
        ContextRecord->Rip = LuaCrash1Rip; // LOL FUCK THIS GAME :) :)
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)LuaCrash2)
    {
        if (!UILocalizeDefaultText)
        {
            UILocalizeDefaultText = (char*)malloc(4);
            strcpy_s(UILocalizeDefaultText, 4, "");
        }
        ContextRecord->Rdx = (INT64)UILocalizeDefaultText; // TREYARCH WHY AM I FIXING YOUR GAME
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)LuaCrash3)
    {
        if (!UILocalizeDefaultText)
        {
            UILocalizeDefaultText = (char*)malloc(4);
            strcpy_s(UILocalizeDefaultText, 4, "");
        }
        ContextRecord->Rsi = (INT64)UILocalizeDefaultText; // TREYARCH WHY AM I FIXING YOUR GAME
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x22328F6))
    {
        if (!UILocalizeDefaultText)
        {
            UILocalizeDefaultText = (char*)malloc(4);
            strcpy_s(UILocalizeDefaultText, 4, "");
        }
        ContextRecord->Rcx = (INT64)UILocalizeDefaultText;
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1E9E657))
    {
        ContextRecord->Rip = REBASE(0x1E9E7E3);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A604C4)) // non-existent clientfield
    {
        ContextRecord->Rip = REBASE(0x1A6054B);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x133EC1) || (PVOID)REBASE(0x133EEB) == ExceptionRecord->ExceptionAddress) // non-existent clientfield
    {
        ContextRecord->Rip = REBASE(0x133F12);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x133F31)) // non-existent clientfield
    {
        ContextRecord->Rip = REBASE(0x133F42);
        ZwContinue(ContextRecord, false);
        return;
    }

    // CSC
    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0xC15B80) || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0xC15C50) // non-existent clientfield
        || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0xC18CF5))
    {
        ContextRecord->Rcx = 1;
        ContextRecord->Rdx = (__int64)clientfield_doesnt_exist;
        ContextRecord->R8 = 0;
        ContextRecord->Rip = REBASE(0x12EA450);
        ZwContinue(ContextRecord, false);
        return;
    }

    // GSC
    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5F94B) || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5FA5E) // non-existent clientfield
        || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5FB5E) || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5FBFD)
        || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5FE76) || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A5FF86)
        || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A6003D) || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A602C7)
        || ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x1A604C4))
    {
        ContextRecord->Rcx = 0;
        ContextRecord->Rdx = (__int64)clientfield_doesnt_exist;
        ContextRecord->R8 = 0;
        ContextRecord->Rip = REBASE(0x12EA450);
        ZwContinue(ContextRecord, false);
        return;
    }

    // some random crash we got on ZNS
    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x13591F3))
    {
        ContextRecord->Rip = REBASE(0x13591FA);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)OFFSET(0x11D2580)) // CG_GetEntityImpactType crash
    {
        ContextRecord->Rax = FALSE;
        ContextRecord->Rip = OFFSET(0x11D2592);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)OFFSET(0x22C4935)) // DObjGetBoneIndex crash
    {
        ContextRecord->Rax = FALSE;
        ContextRecord->Rip = OFFSET(0x22C49FE);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)SCRVM_Error)
    {
        ContextRecord->Rip += 4;
        ContextRecord->Rsp -= 0x30;

        bool isFatal = *(bool*)(REBASE(0x5124869) + 35392 * (BYTE)ContextRecord->Rcx);
        INT64 error_msg_ptr = *(INT64*)(REBASE(0x51A3710) + 0x78 * (BYTE)ContextRecord->Rcx);
        const char* error_msg = "unknown";
        if (error_msg_ptr && *(char*)error_msg_ptr)
        {
            error_msg = (char*)error_msg_ptr;
        }

        if (strstr(error_msg, "Invalid opcode"))
        {
            isFatal = true;
        }

        INT64* ip = 0;
        const char* name = 0;
        INT64* fs = (INT64*)SCRVM_FS;
        //report_script_error(ContextRecord->Rcx, error_msg, ip, name);

        if (isFatal) // fatal crash, dump vm
        {

#ifdef USE_NLOG
            ALOG("Script Fatal Exception at : %p\n", fs[(int)ContextRecord->Rcx * 4]);
            ALOG("Error Message: %s", error_msg);
#endif
            FILE* f;
            f = fopen(CRASH_LOG_NAME, "a+"); // a+ (create + append)
            if (!f)
            {
                // we cant log??? fuck.
                ExitProcess(-444);
            }

            fprintf(f, "Script Exception Type: %s", (ContextRecord->Rcx ? "Client" : "Server"));
            fprintf(f, "Script Fatal Exception at : %p\n", fs[(int)ContextRecord->Rcx * 4]);
            fprintf(f, "\t at: %s+%x", name, fs[(int)ContextRecord->Rcx * 4] - (INT64)ip);
            fprintf(f, "Error Message: %s\n", error_msg);

            std::fflush(f);
            std::fclose(f);

            MessageBoxA(NULL, "Unfortunately, a fatal script error has occured in Black Ops III. The error has been recorded in steamapps/common/Black Ops III/crashes.log.", "Fatal Script Error", MB_OK);
            exit(0);
        }

    exitLbl:
        ZwContinue(ContextRecord, false);
        return;
    }

    INT64 faultingModule = 0;
    char module_name[MAX_PATH] = "<Unknown Module>";

    bool b_fatal = false;
    if ((ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) || (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)/* || (strstr(module_name, "blackops3.exe") && STATUS_ILLEGAL_INSTRUCTION == ExceptionRecord->ExceptionCode)*/)
    {
        const LONG self = (LONG)GetCurrentThreadId();
        if (InterlockedCompareExchange(&g_crashDumpOwnerTid, self, 0) != 0)
        {
            // Deliberately records nothing. Upstream inserted this context into SavedExceptions,
            // but the owning thread is iterating that map and erasing as it goes: an insert from
            // here rehashes it under that iterator, an unsynchronised write that corrupts the heap.
            // No lock is usable on a path entered from an arbitrary faulting thread.
        }
        else
        {
            FILE* f;
            f = fopen(CRASH_LOG_NAME, "a+"); // a+ (create + append)
            if (!f)
            {
#ifdef USE_NLOG
                ALOG("File not opened");
#endif
                // we cant log??? fuck.
                ExitProcess(-444);
            }

            // Stamp the version so a report can be attributed to a release, and say plainly what the
            // stack dump below is: raw process memory, which can hold player names, Steam IDs and
            // session keys. Whoever is asked to attach this file should know that before posting it.
            fprintf(f, "%s\nCrash log %p\n\n"
                "NOTE: the [address] lines below are raw stack memory. They may contain player\n"
                "names, Steam IDs and session data. Review before posting this file publicly.\n\n",
                ZBR_VERSION_FULL, time(NULL));
            INT64 addy = (INT64)ExceptionRecord->ExceptionAddress;
            SavedExceptions[addy] = *ContextRecord;
            while (SavedExceptions.size())
            {
                auto kvp = *SavedExceptions.begin();
                RtlPcToFileHeader((PVOID)kvp.first, (PVOID*)&faultingModule);
                if (faultingModule)
                {
                    if (!GetModuleFileNameA((HMODULE)faultingModule, module_name, MAX_PATH))
                    {
                        strcpy_s(module_name, "<Unknown Module>");
                    }
                }
                else
                {
                    strcpy_s(module_name, "<Unknown Module>");
                }

                //ALOG("Game: %p\n", (void*)REBASE(0x0));
                fprintf(f, "Game: %p\n", (void*)REBASE(0x0));

                //ALOG("Module: %s\n", module_name);
                fprintf(f, "Module: %s\n", module_name);

                //ALOG("[%p]Exception at (%p) (RIP:%p) (Rsp:%p) (RBP: %p)\n", faultingModule, kvp.first - faultingModule, kvp.second.Rip, kvp.second.Rsp, kvp.second.Rbp);
                fprintf(f, "[%p]Exception at (%p) (RIP:%p) (Rsp: %p)\n", faultingModule, kvp.first - faultingModule, kvp.second.Rip, kvp.second.Rsp, kvp.second.Rbp);

                //ALOG("[%p]Rcx: (%p) Rdx: (%p) R8: (%p) R9: (%p)\n", kvp.first, kvp.second.Rcx, kvp.second.Rdx, kvp.second.R8, kvp.second.R9);
                fprintf(f, "[%p]Rcx: (%p) Rdx: (%p) R8: (%p) R9: (%p)\n", kvp.first, kvp.second.Rcx, kvp.second.Rdx, kvp.second.R8, kvp.second.R9);

                //ALOG("[%p]Rax: (%p) Rbx: (%p) Rsi: (%p) Rdi: (%p)\n", kvp.first, kvp.second.Rax, kvp.second.Rbx, kvp.second.Rsi, kvp.second.Rdi);
                fprintf(f, "[%p]Rax: (%p) Rbx: (%p) Rsi: (%p) Rdi: (%p)\n", kvp.first, kvp.second.Rax, kvp.second.Rbx, kvp.second.Rsi, kvp.second.Rdi);

                //ALOG("[%p]R10: (%p) R11: (%p) R12: (%p) R13: (%p)\n", kvp.first, kvp.second.R10, kvp.second.R11, kvp.second.R12, kvp.second.R13);
                fprintf(f, "[%p]R10: (%p) R11: (%p) R12: (%p) R13: (%p)\n", kvp.first, kvp.second.R10, kvp.second.R11, kvp.second.R12, kvp.second.R13);

                //ALOG("[%p]Rbp: (%p) R14: (%p) R15: (%p) Dr7: (%p)\n", kvp.first, kvp.second.Rbp, kvp.second.R14, kvp.second.R15, kvp.second.Dr7);
                fprintf(f, "[%p]Rbp: (%p) R14: (%p) R15: (%p) Dr7: (%p)\n", kvp.first, kvp.second.Rbp, kvp.second.R14, kvp.second.R15, kvp.second.Dr7);

                //ALOG("[%p]Dr0: (%p) Dr1: (%p) Dr2: (%p) Dr3: (%p)\n", kvp.first, kvp.second.Dr0, kvp.second.Dr1, kvp.second.Dr2, kvp.second.Dr3);
                fprintf(f, "[%p]Dr0: (%p) Dr1: (%p) Dr2: (%p) Dr3: (%p)\n", kvp.first, kvp.second.Dr0, kvp.second.Dr1, kvp.second.Dr2, kvp.second.Dr3);

                for (int i = 0; i < ((STATUS_ILLEGAL_INSTRUCTION == ExceptionRecord->ExceptionCode) ? 0x1200 : 0x400); i += 0x10)
                {
                    // A fault near the top of a stack, or with a corrupted Rsp, would otherwise fault
                    // again here, inside the handler, and lose the entry being written.
                    if (Protection::IsBadReadPtr((void*)(kvp.second.Rsp + i))
                        || Protection::IsBadReadPtr((void*)(kvp.second.Rsp + i + 8)))
                    {
                        fprintf(f, "[%p] <unreadable, stack dump truncated>\n", (void*)(kvp.second.Rsp + i));
                        break;
                    }

                    //ALOG("[%p] %p %p\n", kvp.second.Rsp + i, *(int64_t*)(kvp.second.Rsp + i), *(int64_t*)(kvp.second.Rsp + i + 8));
                    fprintf(f, "[%p] %p %p\n\n\n", kvp.second.Rsp + i, *(int64_t*)(kvp.second.Rsp + i), *(int64_t*)(kvp.second.Rsp + i + 8));
                }
                SavedExceptions.erase(kvp.first);
            }

            // dump script context
            //dump_script_context(f);

            std::fflush(f);
            std::fclose(f);
            g_crashDumpOwnerTid = 0;
        }

        if (STATUS_ILLEGAL_INSTRUCTION != ExceptionRecord->ExceptionCode)
        {
            b_fatal = true;
        }
    }

    if (b_fatal)
    {
        const LONG dumpOwner = g_crashDumpOwnerTid;
        if (dumpOwner && dumpOwner != (LONG)GetCurrentThreadId())
        {
            // Park this thread while the owner writes the log. With no handle, resuming into
            // SuspendThread(NULL) would fail and "return" to whatever is on top of the faulting
            // stack, so fall back to suspending the process instead.
            HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
            if (hThread)
            {
                ContextRecord->Rip = (INT64)SuspendThread;
                ContextRecord->Rcx = (INT64)hThread;
                ZwContinue(ContextRecord, false);
            }

            SuspendProcess();
        }
        else
        {
            SuspendProcess();
        }
    }
}

__int64 g_exceptionDispatchSlot = 0;
void* g_exceptionDispatchOriginal = nullptr;
bool wine_hook_installed = false;
// Cached at install so UninstallHook need not look ntdll up by name: that takes the loader lock,
// and UninstallHook runs with every other thread suspended.
__int64 g_kiUserExceptionDispatcher = 0;
// Counted, not logged, for the same reason; Unload() reports it after the resume.
unsigned int g_uninstallHookProtectFailures = 0;

#define HOOK_SIZE_WINE 0x1B
unsigned __int8 old_data[HOOK_SIZE_WINE];
unsigned __int8 new_data[HOOK_SIZE_WINE] = { 0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x48, 0x89, 0xE2, 0x48, 0x8D, 0x8C, 0x24, 0xF0, 0x04, 0x00, 0x00, 0xFF, 0xD0, 0x90, 0x90, 0x90, 0x90 };
bool wine_installhook(void* func, __int64 kiuserexceptiondispatcher)
{
    // We have 0x1A (26 bytes) of space to work with. we need to mov rdx, rsp and lea rcx, [rsp+arg_4E8], then mov rax, call and call rax
    auto begin_write = kiuserexceptiondispatcher + 0xB;

    // grab old asm
    memcpy(old_data, (void*)begin_write, HOOK_SIZE_WINE);

    // prep new_data by inserting pointer to func
    __int64 addy = (__int64)func;
    *(__int64*)(new_data + 2) = addy;

    auto OldProtection = 0ul;
    if (!VirtualProtect(reinterpret_cast<void*>(begin_write), HOOK_SIZE_WINE, PAGE_EXECUTE_READWRITE, &OldProtection))
    {
        ZLOG("installHook: wine VirtualProtect failed (err=%lu)", GetLastError());
        return false;
    }

    memcpy((void*)begin_write, new_data, HOOK_SIZE_WINE);

    // Read back before dropping write access, so a write that did not take is rolled back here
    // rather than left half-applied in ntdll.
    const bool written = memcmp((void*)begin_write, new_data, HOOK_SIZE_WINE) == 0;
    if (!written)
    {
        memcpy((void*)begin_write, old_data, HOOK_SIZE_WINE);
    }

    VirtualProtect(reinterpret_cast<void*>(begin_write), HOOK_SIZE_WINE, OldProtection, &OldProtection);

    if (!written)
    {
        ZLOG("installHook: wine patch did not verify, rolled back");
    }
    return written;
}

void wine_uninstallhook(__int64 kiuserexceptiondispatcher)
{
    auto begin_write = kiuserexceptiondispatcher + 0xB;

    auto OldProtection = 0ul;
    if (!VirtualProtect(reinterpret_cast<void*>(begin_write), HOOK_SIZE_WINE, PAGE_EXECUTE_READWRITE, &OldProtection))
    {
        ++g_uninstallHookProtectFailures;
        return;
    }

    memcpy((void*)begin_write, old_data, HOOK_SIZE_WINE);
    VirtualProtect(reinterpret_cast<void*>(begin_write), HOOK_SIZE_WINE, OldProtection, &OldProtection);
}

// Returns whether the hook is verifiably in place. RunPatching stores that in
// Protection::ExceptionHookInstalled, and install() arms the lobbymsgprints sentinel only if it is
// set. The empty old_windows_* stubs were dropped: an unrecognised prologue now reports false.
bool InstallHook(void* func)
{
    auto kiuserexceptiondispatcher = (__int64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "KiUserExceptionDispatcher");
    g_kiUserExceptionDispatcher = kiuserexceptiondispatcher;

    if (kiuserexceptiondispatcher)
    {
        ZLOG("installHook: dispatcher=%p prologue=%08X",
            (void*)kiuserexceptiondispatcher, *(__int32*)kiuserexceptiondispatcher);
        if (*(__int32*)kiuserexceptiondispatcher == 0x58B48FC)
        {
            // disp32 of a RIP-relative mov, so signed: read as DWORD it zero-extends, and a negative
            // displacement would resolve about 4 GB too high.
            auto distance = *(__int32*)(kiuserexceptiondispatcher + 4);
            auto ptr = (kiuserexceptiondispatcher + 8) + distance;

            auto OldProtection = 0ul;
            if (!VirtualProtect(reinterpret_cast<void*>(ptr), 8, PAGE_EXECUTE_READWRITE, &OldProtection))
            {
                ZLOG("installHook: VirtualProtect failed for dispatch slot (err=%lu)", GetLastError());
                return false;
            }

            // Recorded so UninstallHook can put back what was there. Upstream wrote __nullsub
            // instead, which left ntdll calling into this module after it was gone.
            g_exceptionDispatchOriginal = *reinterpret_cast<void**>(ptr);
            *reinterpret_cast<void**>(ptr) = func;
            const bool applied = (*reinterpret_cast<void**>(ptr) == func);
            VirtualProtect(reinterpret_cast<void*>(ptr), 8, OldProtection, &OldProtection);

            if (!applied)
            {
                g_exceptionDispatchOriginal = nullptr;
                ZLOG("installHook: dispatch slot write did not verify");
                return false;
            }

            g_exceptionDispatchSlot = ptr;
            return true;
        }
        else if (*(__int32*)kiuserexceptiondispatcher == 0x248C8B48)
        {
            ZLOG("installHook: wine path");

            if (!wine_installhook(func, kiuserexceptiondispatcher))
            {
                return false;
            }

            wine_hook_installed = true;
            return true;
        }
    }

    ZLOG("installHook: unrecognised KiUserExceptionDispatcher, exception hook NOT installed");
    return false;
}

// Runs from Unload() with every other thread suspended: no allocation, no logging.
void UninstallHook()
{
    // Nothing to undo, and the INT3 below must not fire with no handler to catch it: it would be
    // an unhandled breakpoint. Cleared here too, or the flag latches and the next exception of
    // any kind has its Rip rewritten to ROP_RETN.
    if (!g_exceptionDispatchSlot && !wine_hook_installed)
    {
        is_unhooking.store(false);
        return;
    }

    // unload current thread's dr7
    __int64 off = INT3_2_BO3;
    ((void(__fastcall*)())off)(); // force an exception to install the exception handler and setup debug registers

    // Restores from what InstallHook recorded rather than re-reading the prologue, so a dispatcher
    // something else patched in between cannot make both branches skip while the hook stays live.
    if (g_exceptionDispatchSlot)
    {
        auto ptr = g_exceptionDispatchSlot;
        auto OldProtection = 0ul;
        if (VirtualProtect(reinterpret_cast<void*>(ptr), 8, PAGE_EXECUTE_READWRITE, &OldProtection))
        {
            *reinterpret_cast<void**>(ptr) = g_exceptionDispatchOriginal;
            VirtualProtect(reinterpret_cast<void*>(ptr), 8, OldProtection, &OldProtection);
            g_exceptionDispatchSlot = 0;
            g_exceptionDispatchOriginal = nullptr;
        }
        else
        {
            ++g_uninstallHookProtectFailures;
        }
    }

    if (wine_hook_installed && g_kiUserExceptionDispatcher)
    {
        wine_uninstallhook(g_kiUserExceptionDispatcher);
        wine_hook_installed = false;
    }

    // Normally already cleared by ExceptHook when the INT3 above lands; unconditional so it cannot
    // latch if that exception went somewhere else.
    is_unhooking.store(false);
    Protection::ExceptionHookInstalled = false;
}

// The three header fields a maintainer needs to recognise (or add) a build. EntryPoint is not one
// bo3::detect_build() checks, but it is reported anyway: it is in every report users have already
// sent, and the injector's report block (injector/ui/src/lib/format.js formatBuildReport) prints the
// same three. Returns false, with the outputs zeroed, if the image has no readable PE header.
static bool ReadGameBuildIds(DWORD& timeDateStamp, DWORD& sizeOfImage, DWORD& entryPoint)
{
    timeDateStamp = sizeOfImage = entryPoint = 0;

    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0)
    {
        return false;
    }

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + static_cast<std::uintptr_t>(dosHeader->e_lfanew));
    timeDateStamp = ntHeaders->FileHeader.TimeDateStamp;
    sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    entryPoint = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    return true;
}

// Upstream's only signal here was one OutputDebugStringA line, so after a game update the patch
// silently stopped protecting anyone: the game runs, looks normal, and is fully exposed. Both
// failures below leave it in exactly that state, so both get a dialog.
//
// The unsupported-build text leads with "the game updated" rather than "your files are broken",
// since verifying files would only re-download the new build. The three values use the exact field
// names, alignment and 0x%08X of the injector's formatBuildReport, so a screenshot of this box and a
// paste from the tool can be compared without normalising either.
//
// Under Proton exclusive-fullscreen this box can sit behind the game window. It is kept anyway; the
// injector warns before launch, and the trace log carries the same values (see the ZLOG below).
// This runs on the dsound proxy's activation thread, not under the loader lock, so blocking on the
// dialog holds up only that thread.
static void ReportInstallFailure(bool supportedBuild, const std::string& reason)
{
    DWORD timeDateStamp = 0, sizeOfImage = 0, entryPoint = 0;
    if (ReadGameBuildIds(timeDateStamp, sizeOfImage, entryPoint))
    {
        ZLOG("arxan: patches NOT applied (%s). TimeDateStamp=%08X SizeOfImage=%08X EntryPoint=%08X",
            supportedBuild ? "supported build" : "UNSUPPORTED BUILD",
            (unsigned)timeDateStamp, (unsigned)sizeOfImage, (unsigned)entryPoint);
    }
    else
    {
        ZLOG("arxan: patches NOT applied - no readable PE header");
    }

    char message[1024]{};
    if (!supportedBuild)
    {
        snprintf(message, sizeof(message),
            "T7Patch does not support this Black Ops III build, so it has not loaded.\n\n"
            "This usually means the game was updated. Check for a new version of T7Patch - "
            "verifying your game files will not help.\n\n"
            "Please include these values in any report:\n"
            "  TimeDateStamp: 0x%08X\n"
            "  SizeOfImage:   0x%08X\n"
            "  EntryPoint:    0x%08X",
            (unsigned)timeDateStamp, (unsigned)sizeOfImage, (unsigned)entryPoint);
    }
    else
    {
        // A recognised build whose Arxan checksum sites did not match the expected pattern counts
        // (or could not be written). The build check passed, so pointing at "the game updated" would
        // mislead; the reason string from arxan_bypass::install is the useful part.
        snprintf(message, sizeof(message),
            "T7Patch could not apply its Arxan integrity bypass, so it has not loaded and the game "
            "is running without its protections.\n\n"
            "Reason: %s\n\n"
            "Please include these values in any report:\n"
            "  TimeDateStamp: 0x%08X\n"
            "  SizeOfImage:   0x%08X\n"
            "  EntryPoint:    0x%08X",
            reason.c_str(), (unsigned)timeDateStamp, (unsigned)sizeOfImage, (unsigned)entryPoint);
    }

    MessageBoxA(nullptr, message, "T7Patch", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

// Latched once RunPatching gets past the steps that can fail cleanly, and never cleared: Unload()
// removes the hooks but not everything install() did - it re-invokes a game function and shifts
// the live password into the previous rotation slot - so a second install would not be a clean
// one. Reload is refused rather than advertised. Upstream had no guard at all.
bool g_patched = false;

// Serialises RunPatching against Unload, so an Unload() landing mid-install cannot tear down
// state that is still being built (uninstall() writing Old_lobbymsgprints while it is still 0).
SRWLOCK g_lifecycleLock = SRWLOCK_INIT;

struct LifecycleLock
{
    LifecycleLock() { AcquireSRWLockExclusive(&g_lifecycleLock); }
    ~LifecycleLock() { ReleaseSRWLockExclusive(&g_lifecycleLock); }
    LifecycleLock(const LifecycleLock&) = delete;
    LifecycleLock& operator=(const LifecycleLock&) = delete;
};

void RunPatching()
{
    // Checked before the lock as well as after it. SRWLOCK is not recursive, so taking it first
    // would turn a same-thread re-entry from a no-op into a permanent self-deadlock.
    if (g_patched)
    {
        ZLOG("patch: already installed, refusing to reinstall (reload is not supported)");
        return;
    }

    LifecycleLock lifecycle;

    if (g_patched)
    {
        return;
    }

    // Before anything that can fail or bail out below, so those diagnostics are written at all.
    zbr_enable_trace_from_config();
    ZLOG("patch: RunPatching %s", ZBR_VERSION_FULL);

	// Set the process priority to above normal to help with performance
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	// Initialize MinHook. Ahead of the Arxan step, where upstream had it after: a failure here now
	// stops the install, and stopping before any game byte is written leaves nothing half-applied -
	// in particular no Arxan patches without the config watcher that maintains them.
    // ALREADY_INITIALIZED is not a failure: it is what a retry after a failed Arxan step sees.
    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
    {
        // Upstream logged nothing and carried on, which installs the exception hook, the memory
        // patches and the lobbymsgprints sentinel while every MH_CreateHook below fails: a session
        // that looks patched with none of the packet validation the detours provide. g_patched is
        // not set, so this can be retried in-process.
        ZLOG("hook: MH_Initialize FAILED (MH_STATUS=%d) - not installing", (int)initStatus);
        OutputDebugStringA("T7 Patch: MinHook could not be initialised, patch not applied\n");
        return;
    }

    std::string arxanMessage;
    if (!arxan_bypass::install(arxanMessage))
    {
        ZLOG("arxan: install FAILED: %s", arxanMessage.c_str());
        // Kept: upstream's only signal, and still what a debugger attached to the game shows.
        OutputDebugStringA(("T7 Patch: " + arxanMessage + "\n").c_str());
        ReportInstallFailure(bo3::supported_build(), arxanMessage);
        return;
    }
    ZLOG("arxan: %s", arxanMessage.c_str());

    g_patched = true;

	// Set a default player name if none is set
    if (!*Protection::CustomName) { snprintf(Protection::CustomName, 16, "Unknown Soldier"); }

    // Apply VMP Hooks
    hooks::ApplyVMTHooks();

    // Before the detours go live: they call through these pointers, which install() below would
    // otherwise only bind after them. See Protection::bind_game_pointers.
    Protection::bind_game_pointers();

	// Apply MinHook hooks
	hooks::ApplyHooks();

	// Apply Memory patches
	hooks::ApplyMemoryPatches();

	// Apply Exception Handler. install() reads the result: it arms a fault-driven sentinel that
	// would be an unhandled crash on the first lobby message with no handler to catch it.
    Protection::ExceptionHookInstalled = InstallHook((void*)ExceptHook);

	// Install the protection hooks
    Protection::install();
}

EXPORT void EnableInjectorlessInstall() // must be called BEFORE loading the patch via zbr_run_gamemode_lui
{
    Protection::IsInjectorlessInstall = true;
}

EXPORT void zbr_run_gamemode_lui(const char* input)
{
    // canon_hash walks the string unchecked; an exported entry point should not fault on null.
    if (!input)
    {
        return;
    }

    if (CONST32("serious_anticrash_2023") == GSCUHashing::canon_hash(input))
    {
        RunPatching();
    }

}

EXPORT void Unload()
{
    LifecycleLock lifecycle;

    // Everything below restores game memory and pointers that only exist once RunPatching got past
    // the Arxan step: UninstallHook() calls into a hardcoded game address, which on an unsupported
    // build is not even the right one. With nothing installed there is nothing to undo. g_patched
    // is only set under the lock above, so this also cannot see a half-finished install.
    if (!g_patched)
    {
        ZLOG("unload: ignored - the patch is not installed");
        return;
    }

    ZLOG("unload: begin");
    Protection::Unloading.store(true);

    // Recorded explicitly so the resume below touches exactly the threads this suspended. Upstream
    // walked a second, live snapshot to resume, which could resume threads it never suspended.
    // Reserved up front so push_back cannot allocate once other threads are suspended - one of
    // them may hold the process heap lock.
    std::vector<DWORD> suspendedThreadIds;
    suspendedThreadIds.reserve(1024);

    // Counted, not logged: everything from here to the resume runs with other threads suspended,
    // and the trace log takes a lock (and allocates) one of them may hold.
    unsigned int getContextFailures = 0;
    unsigned int setContextFailures = 0;

    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 3 && hThreadSnap == INVALID_HANDLE_VALUE; ++attempt)
    {
        hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    }

    if (hThreadSnap == INVALID_HANDLE_VALUE)
    {
        ZLOG("unload: CreateToolhelp32Snapshot FAILED (err=%lu) - tearing down with threads running", GetLastError());
    }
    else
    {
        THREADENTRY32 te32{};
        te32.dwSize = sizeof(THREADENTRY32);

        if (!Thread32First(hThreadSnap, &te32))
        {
            ZLOG("unload: Thread32First FAILED (err=%lu) - tearing down with threads running", GetLastError());
        }
        else
        {
            do
            {
                if (te32.th32OwnerProcessID == GetCurrentProcessId() && te32.th32ThreadID != GetCurrentThreadId())
                {
                    auto hThread = OpenThread(THREAD_ALL_ACCESS, false, te32.th32ThreadID);

                    if (hThread)
                    {
                        // Dr7 only where the suspend took: SetThreadContext on a running thread
                        // races whatever it is doing.
                        if (SuspendThread(hThread) != (DWORD)-1)
                        {
                            CONTEXT tContext{};
                            tContext.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                            if (!GetThreadContext(hThread, &tContext))
                            {
                                ++getContextFailures;
                            }
                            else
                            {
                                tContext.Dr7 = 0;
                                tContext.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                                if (!SetThreadContext(hThread, &tContext))
                                {
                                    ++setContextFailures;
                                }
                            }

                            // Past the reserve, Dr7 is still cleared - the half that matters, since a
                            // thread left with debug registers armed into this module faults once
                            // it is gone - but the thread is let go now rather than reallocating.
                            if (suspendedThreadIds.size() < suspendedThreadIds.capacity())
                            {
                                suspendedThreadIds.push_back(te32.th32ThreadID);
                            }
                            else
                            {
                                ResumeThread(hThread);
                            }
                        }
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hThreadSnap, &te32));
        }

        CloseHandle(hThreadSnap);
    }

    Protection::uninstall();
    hooks::RemoveVMTHooks();
    is_unhooking.store(true);
    UninstallHook();

    for (auto threadId : suspendedThreadIds)
    {
        auto hThread = OpenThread(THREAD_ALL_ACCESS, false, threadId);

        if (hThread)
        {
            ResumeThread(hThread);
            CloseHandle(hThread);
        }
    }

    // Safe to log again: every thread the loop above suspended has been resumed.
    if (getContextFailures || setContextFailures)
    {
        ZLOG("unload: Dr7 left armed on %u thread(s) (GetThreadContext failed %u, SetThreadContext failed %u)",
            getContextFailures + setContextFailures, getContextFailures, setContextFailures);
    }
    if (g_uninstallHookProtectFailures)
    {
        ZLOG("unload: VirtualProtect failed restoring the exception dispatcher (%u) - ntdll may still call into this module",
            g_uninstallHookProtectFailures);
    }
    if (Protection::UninstallIatRestoreFailures)
    {
        ZLOG("unload: FAILED to restore the IsProcessorFeaturePresent IAT entry");
    }
    if (Protection::UninstallVtableProtectFailures)
    {
        ZLOG("unload: VirtualProtect failed for %u Steam vtable slot(s), still pointing into this module",
            Protection::UninstallVtableProtectFailures);
    }

    // Wait for the config watcher to leave the module. Bounded: it stays until arxan_bypass::maintain
    // has done its one re-apply (see MainThread), and that waits on a game signal this cannot hurry.
    // If it is still running the handle is kept and the module must not be freed.
    if (Protection::MainThreadHandle)
    {
        if (WaitForSingleObject(Protection::MainThreadHandle, 5000) == WAIT_OBJECT_0)
        {
            CloseHandle(Protection::MainThreadHandle);
            Protection::MainThreadHandle = nullptr;
        }
        else
        {
            ZLOG("unload: config watcher still running (Arxan maintenance pending) - do not FreeLibrary this module");
        }
    }

    // After the resume, not before: MH_DisableHook allocates and suspends threads itself, so running
    // it while every other thread is suspended can deadlock on the process heap lock. Upstream's
    // Unload never disabled the detours at all.
    hooks::DestroyHooks();
    ZLOG("unload: done");
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        break;
    }
    }
    return TRUE;
}


