#include "Protection.h"

bool has_set_window_text = false;
bool Protection::IsFriendsOnly = false;
bool Protection::IsAutoInstallModsEnabled = false;
bool Protection::IsInjectorlessInstall = true;
bool Protection::Installed = false;
std::atomic<bool> Protection::Unloading{ false };
bool Protection::ExceptionHookInstalled = false;
void* Protection::MainThreadHandle = nullptr;
unsigned int Protection::UninstallIatRestoreFailures = 0;
unsigned int Protection::UninstallVtableProtectFailures = 0;
__int64 Protection::PrivatePassword[3] = { 0, 0 };
char Protection::CustomName[16] = { 0 };
std::unordered_map<BYTE, std::function<void(__int32* lobbyMsgTypePtr, __int64 lobbyMsg)>> Protection::handle_packet_callbacks;
__int64 Protection::Old_lobbymsgprints = 0;
__int64 Protection::CachedRetnAddy = 0;
__int64 Protection::CachedXUID = 0;
tZwContinue Protection::ZwContinue = NULL;
tI_stricmp Protection::I_stricmp = NULL;
char* Protection::UILocalizeDefaultText = NULL;
tLobbyMsgRW_PackageInt Protection::LobbyMsgRW_PackageInt = NULL;
tLobbyMsgRW_PackageUChar Protection::LobbyMsgRW_PackageUChar = NULL;
tLobbyMsgRW_PackageString Protection::LobbyMsgRW_PackageString = NULL;
tLobbyMsgRW_PackageXuid Protection::LobbyMsgRW_PackageXuid = NULL;
tLobbyMsgRW_PackageBool Protection::LobbyMsgRW_PackageBool = NULL;
tLobbyMsgRW_PackageUInt Protection::LobbyMsgRW_PackageUInt = NULL;
tLobbyMsgRW_PackageShort Protection::LobbyMsgRW_PackageShort = NULL;
tLobbyMsgRW_PackageUInt64 Protection::LobbyMsgRW_PackageUInt64 = NULL;
tLobbyMsgRW_PackageArrayStart Protection::LobbyMsgRW_PackageArrayStart = NULL;
tLobbyMsgRW_PackageElement Protection::LobbyMsgRW_PackageElement = NULL;
tLobbyMsgRW_PackageGlob Protection::LobbyMsgRW_PackageGlob = NULL;
tMsgMutableClientInfo_Package Protection::MsgMutableClientInfo_Package = NULL;
tProbeLobbyInfo Protection::ProbeLobbyInfo = NULL;
tdwInstantHandleLobbyMessage Protection::dwInstantHandleLobbyMessage = NULL;
tNET_OutOfBandPrint Protection::NET_OutOfBandPrint = NULL;
tdwCommonAddrToNetadr Protection::dwCommonAddrToNetadr = NULL;
tdwRegisterSecIDAndKey Protection::dwRegisterSecIDAndKey = NULL;
tLobbyMsgRW_PrepWriteMsg Protection::LobbyMsgRW_PrepWriteMsg = NULL;
tLobbyMsgRW_PackageUShort Protection::LobbyMsgRW_PackageUShort = NULL;
tLobbyMsgRW_PackageFloat Protection::LobbyMsgRW_PackageFloat = NULL;
tMSG_Init Protection::MSG_Init = NULL;
tMSG_WriteString Protection::MSG_WriteString = NULL;
tMSG_WriteShort Protection::MSG_WriteShort = NULL;
tMSG_WriteByte Protection::MSG_WriteByte = NULL;
tMSG_WriteData Protection::MSG_WriteData = NULL;
tCom_ControllerIndex_GetLocalClientNum Protection::Com_ControllerIndex_GetLocalClientNum = NULL;
tCom_LocalClient_GetNetworkID Protection::Com_LocalClient_GetNetworkID = NULL;
tNET_OutOfBandData Protection::NET_OutOfBandData = NULL;
tLobbyMsgTransport_SendToAdr Protection::LobbyMsgTransport_SendToAdr = NULL;
tMSG_ReadData Protection::MSG_ReadData = NULL;
tLobbyMsgRW_PrepReadData Protection::LobbyMsgRW_PrepReadData = NULL;
tMSG_InfoResponse Protection::MSG_InfoResponse = NULL;
tLobbyMsgRW_PackageChar Protection::LobbyMsgRW_PackageChar = NULL;
tdwInstantSendMessage Protection::dwInstantSendMessage = NULL;
tLobbySession_GetControllingLobbySession Protection::LobbySession_GetControllingLobbySession = NULL;
tLobbySession_GetSession Protection::LobbySession_GetSession = NULL;
tLobbySession_GetClientByClientNum Protection::LobbySession_GetClientByClientNum = NULL;
tLobbySession_GetClientNetAdrByIndex Protection::LobbySession_GetClientNetAdrByIndex = NULL;
tLobbyJoin_Reserve Protection::LobbyJoin_Reserve = NULL;
tCL_GetConfigString Protection::CL_GetConfigString = NULL;
tCbuf_AddText Protection::Cbuf_AddText = NULL;

const char* sMstart;
const char* sMdata;
const char* sMhead;
const char* sMstate;
const char* sConnectResponse;
const char* sRcon;
const char* sRequestStats;
const char* sRequestStats2;
const char* sLoading;
const char* sRA;
const char* sV;
const char* sVT;
const char* sRelay;
const char* sLMGI;

SD(targetlobby)
SD(sourcelobby)
SD(jointype)
SD(probedxuid)
SD(playlistid)
SD(playlistver)
SD(ffotdver)
SD(networkmode)
SD(netchecksum)
SD(protocol)
SD(changelist)
SD(pingband)
SD(dlcbits)
SD(joinnonce)
SD(chunk)
SD(isStarterPack)
SD(password)
SD(membercount)
SD(members)
SD(xuid)
SD(lobbyid)
SD(skillrating)
SD(skillvariance)
SD(pprobation)
SD(aprobation)
SD(statenum)
SD(mainmode)
SD(partyprivacy)
SD(lobbytype)
SD(lobbymode)
SD(sessionstatus)
SD(uiscreen)
SD(leaderactivity)
SD(key)
SD(leader)
SD(platformsession)
SD(maxclients)
SD(isadvertised)
SD(clientcount)
SD(sessionid)
SD(sessioninfo)
SD(ugcName)
SD(ugcVersion)
SD(clientlist)
SD(clientNum)
SD(gamertag)
SD(isGuest)
SD(connectbit)
SD(score)
SD(address)
SD(qport)
SD(band)
SD(netsrc)
SD(joinorder)
SD(dlcBits)
SD(migratebits)
SD(lasthosttimems)
SD(nomineelist)
SD(serverstatus)
SD(launchnonce)
SD(matchhashlow)
SD(matchhashhigh)
SD(status)
SD(statusvalue)
SD(gamemode)
SD(gametype)
SD(map)
SD(cpqueuedlevel)
SD(movieskipped)
SD(team)
SD(mapvote)
SD(readyup)
SD(plistid)
SD(plistcurr)
SD(plistentries)
SD(plistnext)
SD(plistprev)
SD(plistprevcount)
SD(votecount)
SD(votes)
SD(itemtype)
SD(item)
SD(itemgroup)
SD(attachment)
SD(votetype)
SD(votexuid)
SD(pregamepos)
SD(pregamestate)
SD(clvotecount)
SD(character)
SD(loadout)
SD(settingssize)
SD(compstate)
SD(heartbeatnum)
SD(nonce)
SD(nattype)
SD(lobbies)
SD(valid)
SD(hostxuid)
SD(hostname)
SD(secid)
SD(seckey)
SD(addrbuff)

EXPORT void SetFriendsOnly(bool isFriendsOnly)
{
    Protection::IsFriendsOnly = isFriendsOnly;
    ZLOG("cfg: friendsOnly=%d", (int)isFriendsOnly);
}

EXPORT void SetAutoInstallMods(bool isEnabled)
{
    Protection::IsAutoInstallModsEnabled = isEnabled;
    ZLOG("cfg: autoInstallMods=%d", (int)isEnabled);
}

EXPORT void SetPlayerName(const char* name)
{
    if (!name || strlen(name) > 15)
    {
        return;
    }
    memset(Protection::CustomName, 0, sizeof(Protection::CustomName));
    strcpy_s(Protection::CustomName, name);
    Protection::CustomName[sizeof(Protection::CustomName) - 1] = 0;

    // The name is kept above either way; Protection::install() reapplies it from t7patch.conf once
    // the patch is live. The two writes below are hardcoded game addresses, valid only on a build
    // the Arxan step accepted - on any other they land on unrelated memory. This export can be
    // called before RunPatching, or after it refused the build, by anything in the process.
    if (!arxan_bypass::active())
    {
        ZLOG("cfg: playername kept but not written to game memory - patch not installed");
        return;
    }

    strncpy_s((char*)(pUserData + 0x8), 16, Protection::CustomName, sizeof(Protection::CustomName));
    strncpy_s((char*)(pNameBuffer), 16, Protection::CustomName, sizeof(Protection::CustomName));
}

EXPORT void SetNetworkPassword(const char* pass)
{
    // Null is "no password", the same as "". Only the null test is new: the bytes that are hashed
    // stay exactly upstream's (no trimming), or the prefix and checksum key stop matching an
    // upstream peer typing the same password.
    if (!pass || strlen(pass) == 0 || !(*pass))
    {
        Protection::SetNetworkPassword(0);
    }
    else
    {
        Protection::SetNetworkPassword(GSCUHashing::canon_hash64(pass));
    }
    ZLOG("cfg: password %s", (pass && *pass) ? "SET" : "cleared");
}

IHOOK_HEADER(IsProcessorFeaturePresent, BOOL, (DWORD processorFeature))
{
    if (processorFeature == 0x17)
    {
        // force a crash
        *(__int64*)0x17 = processorFeature;
        *(__int64*)(uintptr_t)processorFeature = 0x17;
    }
    return IHOOK_ORIGINAL(IsProcessorFeaturePresent, (processorFeature));
}

bool Protection::ReadP2PPacket(uintptr_t thisptr, void* pub_dest, unsigned int cub_dest, unsigned int* cub_msg_size, unsigned __int64* steam_id_remote, int n_channel) {

	bool result = ((bool(__fastcall*)(uintptr_t, void*, unsigned int, unsigned int*, uint64_t*, int))GetOriginalSteamPtr(STEAMAPI_NETWORKING, STEAMAPI_NETWORKING_READP2PPACKET))(thisptr, pub_dest, cub_dest, cub_msg_size, steam_id_remote, n_channel);

    // cub_dest as well: *cub_msg_size is the size of the peer's message, not of our buffer, so it
    // alone does not make data[5] readable.
    if (result && pub_dest && cub_dest > 5 && cub_msg_size && *cub_msg_size > 5) {

        char* data = reinterpret_cast<char*>(pub_dest);

        auto game_im = data[5];

        if (game_im == 104) // info request
        {
            if (Protection::IsFriendsOnly && !Protection::IsFriendByXUIDUncached(*steam_id_remote)) {
                ZLOG("p2p: DROP infoReq from non-friend xuid=%p", (void*)*steam_id_remote);
                return false;
            }
        }

        if (game_im == 102) // join request
        {
            ZLOG("p2p: DROP joinRequest (game_im=102) xuid=%p", (void*)*steam_id_remote);
            return false;
        }

        if (game_im == 101 || game_im == 109) // cbuf
        {
            ZLOG("p2p: DROP cbuf (game_im=%d) xuid=%p", (int)game_im, (void*)*steam_id_remote);
            return false;
        }
    }

    return result;
}

const char* Protection::GetUsernamePtr(INT64 a)
{
    return CustomName;
}

unsigned __int64 next_update_friendslist_time = 0;
std::unordered_set<__int64> friends_set;
// Callers run on more than one thread - ReadP2PPacket on Steam's networking path, the invite hooks
// on the game's - and the refresh below used to clear() and refill the set in place while another
// thread was inside find(). unordered_set is not safe for that; the refresh now builds a new set
// outside the lock and swaps it in.
SRWLOCK friends_set_lock = SRWLOCK_INIT;

static bool FriendsSetContains(__int64 xuid)
{
    AcquireSRWLockShared(&friends_set_lock);
    const bool found = friends_set.find(xuid) != friends_set.end();
    ReleaseSRWLockShared(&friends_set_lock);
    return found;
}

bool Protection::IsFriendByXUIDUncached(__int64 xuid) // ok I say its "uncached" but thats because I don't want this running a billion times per second and I think it might hitch with huge friends lists.
{
    if (GetTickCount64() < next_update_friendslist_time || !(*(char*)OFF_s_runningUILevel)) // we will just not update the friends list in game because I really think this will hitch. STEAMAPI SUCKS
    {
        return FriendsSetContains(xuid);
    }

    // Null until Steam's friends interface exists; answer from the last list rather than crash.
    auto isteamfriends = *(__int64*)STEAMAPI_FRIENDS;
    if (!isteamfriends)
    {
        return FriendsSetContains(xuid);
    }

    auto fn_GetFriendCount = *(__int64*)(*(__int64*)isteamfriends + 0x18);
    int num_friends = ((int(__fastcall*)(__int64, int))fn_GetFriendCount)(isteamfriends, 4);

    std::unordered_set<__int64> refreshed;

    // GetFriendByIndex must have been different in the api they used back then
    auto fn_GetFriendByIndex = *(__int64*)(*(__int64*)isteamfriends + 0x20);
    for (int i = 0; i < num_friends; i++)
    {
        __int64 out_friend = 0;
        ((void(__fastcall*)(__int64, __int64&, int, int))fn_GetFriendByIndex)(isteamfriends, out_friend, i, 4);
        if (out_friend)
        {
            refreshed.insert(out_friend);
        }
    }

    next_update_friendslist_time = GetTickCount64() + 30 * 1000; // once every 30 seconds

    AcquireSRWLockExclusive(&friends_set_lock);
    friends_set.swap(refreshed);
    const bool found = friends_set.find(xuid) != friends_set.end();
    ReleaseSRWLockExclusive(&friends_set_lock);
    return found;
}

namespace
{
    bool IsModeContentFilePresent(INT32 appId)
    {
        const wchar_t* contentFile = nullptr;
        switch (appId)
        {
        case 366840:
            contentFile = L"cp_common.xpak";
            break;
        case 366841:
            contentFile = L"mp_common.xpak";
            break;
        case 366842:
            contentFile = L"zm_common.xpak";
            break;
        default:
            return true;
        }

        wchar_t executablePath[MAX_PATH]{};
        const auto pathLength = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        if (pathLength == 0 || pathLength >= MAX_PATH)
        {
            return false;
        }

        std::error_code error;
        const auto path = std::filesystem::path(executablePath).parent_path() / L"zone" / contentFile;
        return std::filesystem::is_regular_file(path, error);
    }
}

std::unordered_map<INT32, bool> installedDlcContent;
std::unordered_map<INT32, bool> subscribedAppContent;
// One lock for both caches and downloadProgress below. These Steam entry points are reached from
// more than one game thread, and a first-time insert can rehash a map another thread is reading.
// Steam itself is always called with the lock released.
SRWLOCK dlc_cache_lock = SRWLOCK_INIT;

static bool FindCachedOwnership(const std::unordered_map<INT32, bool>& cache, INT32 itemid, bool& owns)
{
    AcquireSRWLockShared(&dlc_cache_lock);
    const auto entry = cache.find(itemid);
    const bool found = entry != cache.end();
    if (found)
    {
        owns = entry->second;
    }
    ReleaseSRWLockShared(&dlc_cache_lock);
    return found;
}

// First answer wins, as it did when the check-then-insert was unlocked: two threads racing on the
// same itemid both ask Steam, and whichever stores first is what every later call sees.
static bool StoreCachedOwnership(std::unordered_map<INT32, bool>& cache, INT32 itemid, bool owns)
{
    AcquireSRWLockExclusive(&dlc_cache_lock);
    const bool stored = cache.emplace(itemid, owns).first->second;
    ReleaseSRWLockExclusive(&dlc_cache_lock);
    return stored;
}

bool Protection::GetOwnsContent(INT64 _interface, INT32 itemid)
{
    #if SPOOF_OWNERSHIP
        return IsModeContentFilePresent(itemid);
    #endif

    bool owns = false;
    if (FindCachedOwnership(installedDlcContent, itemid, owns))
    {
        return owns;
    }

    const auto steamReportsInstalled = ((bool(__fastcall*)(INT64, INT32))GetOriginalSteamPtr(
        STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT))(_interface, itemid);
    return StoreCachedOwnership(installedDlcContent, itemid,
        steamReportsInstalled && IsModeContentFilePresent(itemid));
}

bool Protection::GetOwnsContent2(INT64 _interface, INT32 itemid)
{
    #if SPOOF_OWNERSHIP
        return IsModeContentFilePresent(itemid);
    #endif

    bool owns = false;
    if (FindCachedOwnership(subscribedAppContent, itemid, owns))
    {
        return owns;
    }

    return StoreCachedOwnership(subscribedAppContent, itemid,
        ((bool(__fastcall*)(INT64, INT32))GetOriginalSteamPtr(
            STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2))(_interface, itemid));
}

bool Protection::IsVacBanned(INT64 a)
{
    return false;
}

struct download_progress
{
    __int64 a;
    __int64 b;
    unsigned __int64 next;
};

std::unordered_map<INT32, download_progress> downloadProgress;
void Protection::GetDlcDownloadProgress(INT64 a, INT32 b, INT64* c, INT64* d)
{
    auto now = GetTickCount64();

    // Same cache as before - a fresh entry is served for five minutes, then refreshed from Steam -
    // with the map only touched under dlc_cache_lock and Steam called outside it.
    AcquireSRWLockShared(&dlc_cache_lock);
    const auto cached = downloadProgress.find(b);
    if (cached != downloadProgress.end() && now < cached->second.next)
    {
        *c = cached->second.a;
        *d = cached->second.b;
        ReleaseSRWLockShared(&dlc_cache_lock);
        return;
    }
    ReleaseSRWLockShared(&dlc_cache_lock);

    ((void(__fastcall*)(INT64, INT32, INT64*, INT64*))GetOriginalSteamPtr(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS))(a, b, c, d);

    AcquireSRWLockExclusive(&dlc_cache_lock);
    download_progress& slot = downloadProgress[b];
    slot.next = GetTickCount64() + (60 * 5 * 1000);
    slot.a = *c;
    slot.b = *d;
    ReleaseSRWLockExclusive(&dlc_cache_lock);
}

__int32 Protection::GetLobbyChatEntry(INT64 api, INT64 csteamidlobby, INT64 chatid, INT64 psteamuserid, INT64 pvdata, INT64 cubdata, INT64 chatentrytype)
{
    // pump the queue anyways
    auto result = ((int(__fastcall*)(INT64, INT64, INT64, INT64, INT64, INT64, INT64))GetOriginalSteamPtr(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY))(api, csteamidlobby, chatid, psteamuserid, pvdata, cubdata, chatentrytype);
    

    auto steamid = *(__int64*)psteamuserid;

    LobbyType sessionType = LobbyType::LOBBY_TYPE_AUTO;

    if (Protection::LobbySession_GetControllingLobbySession(LOBBY_MODULE_CLIENT))
    {
        sessionType = LOBBY_TYPE_GAME;
    }
    else
    {
        sessionType = LOBBY_TYPE_PRIVATE;
    }

    const auto session = Protection::LobbySession_GetSession(sessionType);
    bool found_xuid = false;

    for (int i = 0; session && i < 18; i++)
    {
        const auto client = Protection::LobbySession_GetClientByClientNum(session, i);
        if (client && client->activeClient && client->activeClient->fixedClientInfo.xuid == steamid)
        {
            found_xuid = true;
        }
    }

    if (!found_xuid)
    {
        return 0; // user isnt in our bo3 lobby and thus cannot chat with us
    }

    if (result > 0 && pvdata && cubdata > 0 && chatentrytype && *(__int32*)(chatentrytype))
    {
        char* msg = (char*)pvdata;

        // The payload is whatever the peer sent and Steam does not terminate it, so the scan is
        // bounded by what was actually written (and by our buffer). This loop writes, so an
        // unterminated chat line was an out-of-bounds write, not just a read.
        size_t limit = (size_t)result;
        if ((size_t)cubdata < limit)
        {
            limit = (size_t)cubdata;
        }
        const int msgLen = (int)strnlen(msg, limit);

        for (int i = 0; i < msgLen; i++)
        {
            if (msg[i] == '^')
            {
                msg[i] = '.';
            }
            else if (msg[i] == '%')
            {
                msg[i] = '.';
            }
            else if (msg[i] == '$' && (i + 1) < msgLen && msg[i + 1] == '(')
            {
                msg[i] = '.';
            }
            else if (msg[i] == '[' && (i + 1) < msgLen && msg[i + 1] == '{')
            {
                msg[i] = '.';
            }
            else if (msg[i] < 0x20)
            {
                msg[i] = '.';
            }
            else if (msg[i] > 126)
            {
                msg[i] = '.';
            }
        }
    }

    return result;
}

__int64 Protection::CreateLobby(INT64 api, __int32 lobbyCreateType, __int32 maxplayers)
{
    return ((__int64(__fastcall*)(INT64, INT64, INT64))GetOriginalSteamPtr(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_CREATELOBBY))(api, 1, maxplayers);
}

// s_playerData_ptr is a fixed slot in the game image, so it is always readable, but the player data
// it points at is null before sign-in has allocated it. GetUsernameXUIDPtr runs whenever the game
// resolves any player's name, including peers', so it must not assume the local one exists yet.
static bool TryGetLocalXuid(__int64& out)
{
    const auto playerData = *(__int64**)s_playerData_ptr;
    if (!playerData)
    {
        return false;
    }

    out = *playerData;
    return true;
}

const char* Protection::GetUsernameXUIDPtr(INT64 a, INT64 b)
{
    __int64 localXuid = 0;
    if (TryGetLocalXuid(localXuid) && b == localXuid)
    {
        return CustomName;
    }
    return ((char* (__fastcall*)(INT64, INT64))GetOriginalSteamPtr(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_VT_NAMEBYXUID))(a, b);
}

__int32 Protection::CL_SwitchState_Idle_Update(INT64 sw)
{
    if (strcmp(((const char* (__fastcall*)())REBASE(0x20EAD00))(), "CP"))
    {
        return 0;
    }
    return ((__int32(__fastcall*)(__int64))REBASE(0x131E350))(sw);
}

void Protection::SetNetworkPassword(__int64 pass)
{
    //XLOG("PASSWORD: %p", pass);
    Protection::PrivatePassword[0] = Protection::PrivatePassword[1];
    Protection::PrivatePassword[1] = pass;
    Protection::PrivatePassword[2] = GetTickCount64();
}

bool Protection::IsBadReadPtr(void* p)
{
    MEMORY_BASIC_INFORMATION mbi = { 0 };
    if (::VirtualQuery(p, &mbi, sizeof(mbi)))
    {
        DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
        bool b = !(mbi.Protect & mask);
        // check the page is not a guard page
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) b = true;

        return b;
    }
    return true;
}

std::unordered_map<__int64, std::unordered_map<int, __int64>> Protection::SteamHAPIHooks;
void Protection::SwapSteamAPIPointer(__int64 hLibrary, int vPointerIndex, void* CallFuncReplace)
{
    // Only uninstall() can pass null: GetOriginalSteamPtr for a slot that was never recorded.
    // Writing it would put 0 into a live Steam vtable. There is deliberately no matching guard on
    // the Steam globals below: if Steam is not up, install() faults here as upstream always has,
    // rather than carrying on with ReadP2PPacket and the friends-only gate silently absent.
    if (!CallFuncReplace)
    {
        return;
    }

    auto steamLibrary = *(INT64*)hLibrary;
    auto OldProtection = 0ul;
    INT64* vtable = *(INT64**)steamLibrary;

    // Recorded once per slot, never overwritten. install() records every slot before it swaps the
    // first one (RecordSteamOriginal), so once any hook is live this map is only ever read: the
    // hooks call GetOriginalSteamPtr on every packet and every chat line, from Steam's threads,
    // and an insert here could rehash it underneath them. Overwriting was also wrong on its own:
    // uninstall() passes back the original, but the slot still holds our hook when this reads it,
    // so the map would end up naming our own function as the original.
    auto& slots = SteamHAPIHooks[hLibrary];
    if (slots.find(vPointerIndex) == slots.end())
    {
        slots[vPointerIndex] = *(vtable + vPointerIndex);
    }

    if (!VirtualProtect(reinterpret_cast<void*>(vtable + vPointerIndex), 8, PAGE_EXECUTE_READWRITE, &OldProtection))
    {
        if (Protection::Unloading.load())
        {
            // Every other thread is suspended (see Unload), so count rather than log, and leave the
            // slot alone rather than fault inside the teardown.
            ++Protection::UninstallVtableProtectFailures;
            return;
        }

        // On install the write below still happens, as it always has. Skipping it would leave this
        // slot unhooked with nothing but a trace line to show for it; writing either works (the
        // page was already writable) or faults at startup, where it is seen.
        ZLOG("steam: VirtualProtect failed for vtable slot %d (err=%lu)", vPointerIndex, GetLastError());
    }

    *reinterpret_cast<void**>(vtable + vPointerIndex) = CallFuncReplace;
    VirtualProtect(reinterpret_cast<void*>(vtable + vPointerIndex), 8, OldProtection, &OldProtection);
}

// Records a slot's original without swapping it, so install() can fill the map before any hook is live.
static void RecordSteamOriginal(__int64 hLibrary, int vPointerIndex)
{
    auto steamLibrary = *(INT64*)hLibrary;
    INT64* vtable = *(INT64**)steamLibrary;

    auto& slots = Protection::SteamHAPIHooks[hLibrary];
    if (slots.find(vPointerIndex) == slots.end())
    {
        slots[vPointerIndex] = *(vtable + vPointerIndex);
    }
}

INT64 Protection::GetOriginalSteamPtr(__int64 hLibrary, int vtIndex)
{
    // find() on both levels: operator[] inserts a missing slot, which allocates and can rehash the
    // map while a hook on another thread is reading it.
    const auto lib = SteamHAPIHooks.find(hLibrary);
    if (lib == SteamHAPIHooks.end())
    {
        return 0;
    }

    const auto slot = lib->second.find(vtIndex);
    if (slot == lib->second.end())
    {
        return 0;
    }
    return slot->second;
}

bool fs_exists(const char* filename)
{
    if (!std::filesystem::exists(filename))
    {
        return false;
    }
    if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(filename) && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        return false;
    }
    return true;
}

struct patch_config
{
    char playername[16];
    int isfriendsonly;
    int autoinstallmods;
    char* networkpassword;
    // Not one of upstream's keys: upstream's DLL ignores it, this one turns the trace log on.
    // has_debuglog records that the file carried the line at all, so saveto can keep it without
    // adding it to a file that never had one.
    bool debuglog;
    bool has_debuglog;
    bool exists;
    std::filesystem::file_time_type modified;

    patch_config()
    {
        networkpassword = (char*)malloc(4);
        memset(networkpassword, 0, 4);
        isfriendsonly = true;
        autoinstallmods = false;
        debuglog = false;
        has_debuglog = false;
        exists = false;
        modified = std::filesystem::file_time_type();
        __playername();
        strcat_s(playername, "Unknown Soldier");
    }

    void __playername()
    {
        memset(playername, 0, 16);
    }

    bool update_watcher_time(const char* path)
    {
        bool did_exist_before = exists;
        if (!fs_exists(path))
        {
            exists = false;
            return did_exist_before != exists;
        }

        exists = true;
        auto time = std::filesystem::last_write_time(path);

        bool was_same_time = modified == time;
        modified = time;

        return (did_exist_before != exists) || !was_same_time;
    }

    void saveto(const char* path)
    {
        std::ofstream outfile(path);

        if (!outfile.is_open())
        {
            update_watcher_time(path);
            return;
        }

        outfile << "playername=" << playername << std::endl;
        outfile << "isfriendsonly=" << isfriendsonly << std::endl;
        outfile << "autoinstallmods=" << autoinstallmods << std::endl;
        outfile << "networkpassword=" << (networkpassword ? networkpassword : "") << std::endl;
        // After upstream's four keys, and only if the file being replaced had it: a fresh default
        // conf stays byte-identical to upstream's, and a debuglog=1 the user wrote is not lost.
        if (has_debuglog)
        {
            outfile << "debuglog=" << (debuglog ? 1 : 0) << std::endl;
        }

        outfile.close();
        update_watcher_time(path);
    }

    void loadfrom(const char* path)
    {
        std::ifstream infile(path);

        if (!infile.is_open())
        {
            update_watcher_time(path);
            return;
        }

        // Reset per read, unlike upstream's keys: debuglog absent means off, so deleting the line
        // must turn the trace log off rather than leave the last value in place.
        debuglog = false;
        has_debuglog = false;
        bool firstLine = true;

        std::string line;
        while (std::getline(infile, line))
        {
            // Through the shared helper so this and the early reader agree byte for byte; the
            // upstream keys below are still parsed exactly as upstream parses them.
            bool lineDebuglog = false;
            if (zbr_parse_debuglog_line(line, firstLine, lineDebuglog))
            {
                debuglog = lineDebuglog;
                has_debuglog = true;
            }
            firstLine = false;

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            auto sep = line.find("=");
            if (sep == std::string::npos) // must have a value
            {
                continue;
            }

            // is this config resilliant to whitespace issues? nope!
            auto token = line.substr(0, sep);
            auto val = line.substr(sep + 1);
            switch (fnv1a(token.c_str()))
            {
            case FNV32("playername"):
            {
                if (val.empty())
                {
                    break;
                }

                if (val.length() > 15)
                {
                    val = val.substr(0, 15);
                }
                __playername();
                strcpy_s(playername, sizeof(playername), val.data());
            }
            break;
            case FNV32("isfriendsonly"):
            {
                if (val.empty())
                {
                    isfriendsonly = false;
                    break;
                }

                std::istringstream ivalread(val);
                ivalread >> isfriendsonly;
                if (ivalread.fail())
                {
                    isfriendsonly = false; // its better to have it fail then to have people who cant disable this setting because of whatever reason
                }
            }
            break;
            case FNV32("autoinstallmods"):
            {
                if (val.empty())
                {
                    autoinstallmods = false;
                    break;
                }

                std::istringstream ivalread(val);
                ivalread >> autoinstallmods;
                if (ivalread.fail())
                {
                    autoinstallmods = false;
                }
                else
                {
                    autoinstallmods = autoinstallmods == 1;
                }
            }
            break;
            case FNV32("networkpassword"):
            {
                if (networkpassword)
                {
                    // Scrubbed before it goes back to the heap, where a crash dump would keep the
                    // plaintext. The bytes kept below are untouched: they are what gets hashed.
                    SecureZeroMemory(networkpassword, strlen(networkpassword));
                    free(networkpassword);
                    networkpassword = NULL;
                }
                if (val.length() > 1023)
                {
                    val.resize(1023); // seriously?!
                }
                auto bufsize = val.length() + 1;
                networkpassword = static_cast<char*>(malloc(bufsize));
                
                if (networkpassword)
                {
                    strcpy_s(networkpassword, bufsize, val.c_str());
                }

                break;
            }
            break;
            }
        }

        infile.close();
        update_watcher_time(path);
    }
};

patch_config user_config;

void apply_settings()
{
    // First, so the lines the setters below write are kept when this read is what turned it on.
    g_trace_enabled.store(user_config.debuglog, std::memory_order_relaxed);
    ZLOG("cfg: applying t7patch.conf");
    SetPlayerName(user_config.playername);
    SetFriendsOnly(user_config.isfriendsonly);
    SetAutoInstallMods(user_config.autoinstallmods);
    SetNetworkPassword(user_config.networkpassword);
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    std::srand(time(NULL));
    *(__int32*)OFFSET(0x11250898) = rand();

    for (;;)
    {
        arxan_bypass::maintain();

        // After Unload() the config is no longer applied: apply_settings writes the name into game
        // memory and the password into state uninstall() has just reset. The thread only exits once
        // maintain() has done its one re-apply, though. Unload() does not revert the Arxan patches,
        // so the game still needs it, and upstream's watcher never exited at all.
        if (Protection::Unloading.load())
        {
            if (!arxan_bypass::maintenance_pending())
            {
                break;
            }
            Sleep(1000);
            continue;
        }

        if (user_config.update_watcher_time(PATCH_CONFIG_LOCATION))
        {
            user_config.loadfrom(PATCH_CONFIG_LOCATION);
            apply_settings();
        }
        Sleep(1000);
    }

    return 0;
}

void load_settings_initial()
{
    if (!fs_exists(PATCH_CONFIG_LOCATION))
    {
        user_config.saveto(PATCH_CONFIG_LOCATION);
    }
    else
    {
        user_config.loadfrom(PATCH_CONFIG_LOCATION);
    }
    apply_settings();
}

__int64 old_IsProcessorFeaturePresent = 0;

// Plain loads of fixed game addresses, split out so RunPatching can bind them before
// hooks::ApplyHooks enables the detours. Those detours call through these (I_stricmp,
// MSG_ReadData, Cbuf_AddText...) on every inbound lobby message, connectionless command and
// menu response, and until install() ran they were null: a packet arriving in that window was
// a call through a null pointer. install() calls it again; it only re-stores the same values.
void Protection::bind_game_pointers()
{
    LobbyMsgRW_PackageInt = (tLobbyMsgRW_PackageInt)PTR_LobbyMsgRW_PackageInt;
    LobbyMsgRW_PackageUChar = (tLobbyMsgRW_PackageUChar)PTR_LobbyMsgRW_PackageUChar;
    LobbyMsgRW_PackageString = (tLobbyMsgRW_PackageString)PTR_LobbyMsgRW_PackageString;
    LobbyMsgRW_PackageXuid = (tLobbyMsgRW_PackageXuid)PTR_LobbyMsgRW_PackageXuid;
    LobbyMsgRW_PackageBool = (tLobbyMsgRW_PackageBool)PTR_LobbyMsgRW_PackageBool;
    LobbyMsgRW_PackageUInt = (tLobbyMsgRW_PackageUInt)PTR_LobbyMsgRW_PackageUInt;
    LobbyMsgRW_PackageShort = (tLobbyMsgRW_PackageShort)PTR_LobbyMsgRW_PackageShort;
    LobbyMsgRW_PackageUInt64 = (tLobbyMsgRW_PackageUInt64)PTR_LobbyMsgRW_PackageUInt64;
    LobbyMsgRW_PackageArrayStart = (tLobbyMsgRW_PackageArrayStart)PTR_LobbyMsgRW_PackageArrayStart;
    LobbyMsgRW_PackageElement = (tLobbyMsgRW_PackageElement)PTR_LobbyMsgRW_PackageElement;
    LobbyMsgRW_PackageGlob = (tLobbyMsgRW_PackageGlob)PTR_LobbyMsgRW_PackageGlob;
    MsgMutableClientInfo_Package = (tMsgMutableClientInfo_Package)PTR_MsgMutableClientInfo_Package;
    ProbeLobbyInfo = (tProbeLobbyInfo)PTR_ProbeLobbyInfo;
    dwInstantHandleLobbyMessage = (tdwInstantHandleLobbyMessage)PTR_dwInstantHandleLobbyMessage;
    NET_OutOfBandPrint = (tNET_OutOfBandPrint)PTR_NET_OutOfBandPrint;
    dwCommonAddrToNetadr = (tdwCommonAddrToNetadr)PTR_dwCommonAddrToNetadr;
    dwRegisterSecIDAndKey = (tdwRegisterSecIDAndKey)PTR_dwRegisterSecIDAndKey;
    LobbyMsgRW_PrepWriteMsg = (tLobbyMsgRW_PrepWriteMsg)PTR_LobbyMsgRW_PrepWriteMsg;
    LobbyMsgRW_PackageUShort = (tLobbyMsgRW_PackageUShort)PTR_LobbyMsgRW_PackageUShort;
    LobbyMsgRW_PackageFloat = (tLobbyMsgRW_PackageFloat)PTR_LobbyMsgRW_PackageFloat;
    MSG_Init = (tMSG_Init)PTR_MSG_Init;
    MSG_WriteString = (tMSG_WriteString)PTR_MSG_WriteString;
    MSG_WriteShort = (tMSG_WriteShort)PTR_MSG_WriteShort;
    MSG_WriteByte = (tMSG_WriteByte)PTR_MSG_WriteByte;
    MSG_WriteData = (tMSG_WriteData)PTR_MSG_WriteData;
    Com_ControllerIndex_GetLocalClientNum = (tCom_ControllerIndex_GetLocalClientNum)PTR_Com_ControllerIndex_GetLocalClientNum;
    Com_LocalClient_GetNetworkID = (tCom_LocalClient_GetNetworkID)PTR_Com_LocalClient_GetNetworkID;
    NET_OutOfBandData = (tNET_OutOfBandData)PTR_NET_OutOfBandData;
    LobbyMsgTransport_SendToAdr = (tLobbyMsgTransport_SendToAdr)PTR_LobbyMsgTransport_SendToAdr;
    MSG_ReadData = (tMSG_ReadData)PTR_MSG_ReadData;
    LobbyMsgRW_PrepReadData = (tLobbyMsgRW_PrepReadData)PTR_LobbyMsgRW_PrepReadData;
    MSG_InfoResponse = (tMSG_InfoResponse)PTR_MSG_InfoResponse;
    LobbyMsgRW_PackageChar = (tLobbyMsgRW_PackageChar)PTR_LobbyMsgRW_PackageChar;
    I_stricmp = (tI_stricmp)PTR_I_stricmp;
    dwInstantSendMessage = (tdwInstantSendMessage)PTR_dwInstantSendMessage;
    LobbySession_GetControllingLobbySession = (tLobbySession_GetControllingLobbySession)PTR_LobbySession_GetControllingLobbySession;
    LobbySession_GetSession = (tLobbySession_GetSession)PTR_LobbySession_GetSession;
    LobbySession_GetClientByClientNum = (tLobbySession_GetClientByClientNum)PTR_LobbySession_GetClientByClientNum;
    LobbySession_GetClientNetAdrByIndex = (tLobbySession_GetClientNetAdrByIndex)PTR_LobbySession_GetClientNetAdrByIndex;
    LobbyJoin_Reserve = (tLobbyJoin_Reserve)PTR_LobbyJoin_Reserve;
    CL_GetConfigString = (tCL_GetConfigString)PTR_CL_GetConfigString;
    Cbuf_AddText = (tCbuf_AddText)PTR_Cbuf_AddText;
    I_stricmp = (tI_stricmp)PTR_I_stricmp;
}

void Protection::install()
{
    // RunPatching's latch already stops a second call; this keeps install() itself from re-recording
    // Old_lobbymsgprints as the sentinel and re-running the one-shot game calls below.
    if (Protection::Installed)
    {
        return;
    }

    bind_game_pointers();
    if (!TryGetLocalXuid(CachedXUID))
    {
        CachedXUID = 0;
    }

    SetNetworkPassword(0);

    sMstart = "mstart";
    sMdata = "mdata";
    sMhead = "mhead";
    sMstate = "mstate";
    sConnectResponse = "connectResponseMigration";
    sRcon = "rcon";
    sRequestStats = "requeststats";
    sRequestStats2 = "requeststats\n";
    sLoading = "loadingnewmap";
    sRA = "RA";
    sV = "v";
    sVT = "vt";
    sRelay = "relay";
    sLMGI = "LMgetinfo";

    SS(targetlobby)
    SS(sourcelobby)
    SS(jointype)
    SS(probedxuid)
    SS(playlistid)
    SS(playlistver)
    SS(ffotdver)
    SS(networkmode)
    SS(netchecksum)
    SS(protocol)
    SS(changelist)
    SS(pingband)
    SS(dlcbits)
    SS(joinnonce)
    SS(chunk)
    SS(isStarterPack)
    SS(password)
    SS(membercount)
    SS(members)
    SS(xuid)
    SS(lobbyid)
    SS(skillrating)
    SS(skillvariance)
    SS(pprobation)
    SS(aprobation)
    SS(statenum)
    SS(mainmode)
    SS(partyprivacy)
    SS(lobbytype)
    SS(lobbymode)
    SS(sessionstatus)
    SS(uiscreen)
    SS(leaderactivity)
    SS(key)
    SS(leader)
    SS(platformsession)
    SS(maxclients)
    SS(isadvertised)
    SS(clientcount)
    SS(sessionid)
    SS(sessioninfo)
    SS(ugcName)
    SS(ugcVersion)
    SS(clientlist)
    SS(clientNum)
    SS(gamertag)
    SS(isGuest)
    SS(connectbit)
    SS(score)
    SS(address)
    SS(qport)
    SS(band)
    SS(netsrc)
    SS(joinorder)
    SS(dlcBits)
    SS(migratebits)
    SS(lasthosttimems)
    SS(nomineelist)
    SS(dlcBits)
    SS(dlcBits)
    SS(serverstatus)
    SS(launchnonce)
    SS(matchhashlow)
    SS(matchhashhigh)
    SS(status)
    SS(statusvalue)
    SS(gamemode)
    SS(gametype)
    SS(map)
    SS(cpqueuedlevel)
    SS(movieskipped)
    SS(team)
    SS(mapvote)
    SS(readyup)
    SS(plistid)
    SS(plistcurr)
    SS(plistentries)
    SS(plistnext)
    SS(plistprev)
    SS(plistprevcount)
    SS(votecount)
    SS(votes)
    SS(itemtype)
    SS(item)
    SS(itemgroup)
    SS(attachment)
    SS(votetype)
    SS(votexuid)
    SS(pregamepos)
    SS(pregamestate)
    SS(clvotecount)
    SS(character)
    SS(loadout)
    SS(settingssize)
    SS(compstate)
    SS(heartbeatnum)
    SS(nonce)
    SS(nattype)
    SS(lobbies)
    SS(valid)
    SS(hostxuid)
    SS(hostname)
    SS(secid)
    SS(seckey)
    SS(addrbuff)

    if (*CustomName)
    {
        memcpy((void*)REBASE(0x15E84638), CustomName, strlen(CustomName) + 1);
        memcpy((void*)PTR_Name1, CustomName, strlen(CustomName) + 1);
        memcpy((void*)PTR_Name2, CustomName, strlen(CustomName) + 1);
        if (!IsBadReadPtr((VOID*)s_playerData_ptr) && *(INT64*)s_playerData_ptr)
        {
            memset((void*)(*(INT64*)s_playerData_ptr + 0x8), 0, 16);
            memcpy((void*)(*(INT64*)s_playerData_ptr + 0x8), CustomName, strlen(CustomName) + 1);
        }
    }

    old_IsProcessorFeaturePresent = (__int64)&IsProcessorFeaturePresent;

    oReadP2PPacket = (decltype(&ReadP2PPacket))((*(__int64*)(STEAMAPI_NETWORKING + 0x10)));
    IHOOK_INSTALL(IsProcessorFeaturePresent, GetModuleHandleA(0));

    // Every original first, so the swaps below - and the hooks they make live - only ever read
    // SteamHAPIHooks. See SwapSteamAPIPointer.
    RecordSteamOriginal(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_GETUSERNAME);
    RecordSteamOriginal(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_VT_NAMEBYXUID);
    RecordSteamOriginal(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT);
    RecordSteamOriginal(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2);
    RecordSteamOriginal(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_IS_VAC_BANNED);
    RecordSteamOriginal(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS);
    RecordSteamOriginal(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY);
    RecordSteamOriginal(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_CREATELOBBY);
    RecordSteamOriginal(STEAMAPI_NETWORKING, STEAMAPI_NETWORKING_READP2PPACKET);

    SwapSteamAPIPointer(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_GETUSERNAME, (void*)GetUsernamePtr);
    SwapSteamAPIPointer(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_VT_NAMEBYXUID, (void*)GetUsernameXUIDPtr);

    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT, (void*)GetOwnsContent);
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2, (void*)GetOwnsContent2);
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_IS_VAC_BANNED, (void*)IsVacBanned);
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS, (void*)GetDlcDownloadProgress);

    SwapSteamAPIPointer(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY, (void*)GetLobbyChatEntry);
    SwapSteamAPIPointer(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_CREATELOBBY, (void*)CreateLobby);
    SwapSteamAPIPointer(STEAMAPI_NETWORKING, STEAMAPI_NETWORKING_READP2PPACKET, (void*)ReadP2PPacket);

    *(__int64*)PTR_UpdatePreloadIdleFN = (INT64)CL_SwitchState_Idle_Update;


    // join party
    Protection::handle_packet_callbacks[0x10] = [](__int32* lobbyMsgTypePtr, __int64 lobbyMsg)
        {
            memset(requestOut, 0, sizeof(requestOut));
            memset(lobbyMsgCpy, 0, sizeof(lobbyMsgCpy));
            memcpy_s(lobbyMsgCpy, sizeof(lobbyMsgCpy), (void*)lobbyMsg, 0x44);

            auto result = MSG_JoinParty_Package_Inspect(requestOut, lobbyMsgCpy);
            if (result)
            {
                // crash attempt
                *lobbyMsgTypePtr = 0xFF; // drop

            }
        };

    // modified stats
    Protection::handle_packet_callbacks[0xF] = [](__int32* lobbyMsgTypePtr, __int64 lobbyMsg)
        {
            *lobbyMsgTypePtr = 0xFF; // drop
        };

    // auto-drops
    Protection::handle_packet_callbacks[0x1E] = Protection::handle_packet_callbacks[0xF];
    Protection::handle_packet_callbacks[0x16] = Protection::handle_packet_callbacks[0xF];
    Protection::handle_packet_callbacks[0x17] = Protection::handle_packet_callbacks[0xF];
    Protection::handle_packet_callbacks[0x18] = Protection::handle_packet_callbacks[0xF];

    Protection::handle_packet_callbacks[MESSAGE_TYPE_LOBBY_HOST_HEARTBEAT] = [](__int32* lobbyMsgTypePtr, __int64 lobbyMsg)
        {
            memset(requestOut, 0, sizeof(requestOut));
            memset(lobbyMsgCpy, 0, sizeof(lobbyMsgCpy));
            memcpy_s(lobbyMsgCpy, sizeof(lobbyMsgCpy), (void*)lobbyMsg, 0x44);
            __int32* _this = (__int32*)requestOut;

            BOOL packageOK =
                LobbyMsgRW_PackageInt((void*)lobbyMsgCpy, "heartbeatnum", (__int32*)_this) &&
                LobbyMsgRW_PackageInt((void*)lobbyMsgCpy, "lobbytype", (__int32*)_this) &&
                LobbyMsgRW_PackageInt((void*)lobbyMsgCpy, "lasthosttimems", (__int32*)_this);

            if (!packageOK)
            {
                return;
            }

            LobbyMsgRW_PackageArrayStart((void*)lobbyMsgCpy, "nomineelist");

            unsigned __int64 xuidValue = 1;
            for (int i = 0; i < 0x12; i++)
            {
                bool res = LobbyMsgRW_PackageElement((void*)lobbyMsgCpy, true);
                if (!res)
                {
                    return;
                }
                res = LobbyMsgRW_PackageXuid((void*)lobbyMsgCpy, "xuid", &xuidValue);

                if (!res)
                {
                    return;
                }
            }

            if (LobbyMsgRW_PackageElement((void*)lobbyMsgCpy, true))
            {
                *lobbyMsgTypePtr = 0xFF;
            }
        };

    Protection::handle_packet_callbacks[0x2] = [](__int32* lobbyMsgTypePtr, __int64 lobbyMsg)
        {
            memset(requestOut, 0, sizeof(requestOut));
            memset(lobbyMsgCpy, 0, sizeof(lobbyMsgCpy));
            memcpy_s(lobbyMsgCpy, sizeof(lobbyMsgCpy), (void*)lobbyMsg, 0x44);

            auto result = MSG_LobbyState_Package_Inspect(requestOut, lobbyMsgCpy);
            if (result)
            {
                // crash attempt
                *lobbyMsgTypePtr = 0xFF;
            }
        };

    Protection::handle_packet_callbacks[0x3] = [](__int32* lobbyMsgTypePtr, __int64 lobbyMsg)
        {
            memset(requestOut, 0, sizeof(requestOut));
            memset(lobbyMsgCpy, 0, sizeof(lobbyMsgCpy));
            memcpy_s(lobbyMsgCpy, sizeof(lobbyMsgCpy), (void*)lobbyMsg, 0x44);

            auto result = MSG_LobbyStateGame_Package_Inspect(requestOut, lobbyMsgCpy);
            if (result)
            {
                // crash attempt
                *lobbyMsgTypePtr = 0xFF;
            }
        };

    // The sentinel is a deliberately invalid call target that only the exception hook can catch.
    // With no hook installed (InstallHook did not recognise or could not verify the dispatcher),
    // arming it would turn the first call through lobbymsgprints into an unhandled crash.
    if (Protection::ExceptionHookInstalled)
    {
        Protection::Old_lobbymsgprints = *(__int64*)PTR_lobbymsgprints;
        *(__int64*)PTR_lobbymsgprints = 0xFFEEDDCC44332212;
    }
    else
    {
        ZLOG("install: exception hook not installed, lobbymsgprints sentinel left unarmed");
    }
    Protection::CachedRetnAddy = PTR_saveLobbyMsgExceptAddy;

    // Call create lobby again
    ((void(__fastcall*)(__int64))REBASE(0x1EA6010))(REBASE(0x113A4A60));

    INT64 ptrDvar = *(INT64*)(REBASE(0x1686ED20));
    *(DWORD*)(ptrDvar + 0x18) = 0; // clear flags

    Dvar_SetFromStringByName("ui_error_callstack_ship", "1", true);

    ptrDvar = *(INT64*)(REBASE(0xA0378B8));
    *(DWORD*)(ptrDvar + 0x18) = 0; // clear flags

    Dvar_SetFromStringByName("g_allowvote", "0", true);

    //Dvar_SetFromStringByName("sv_mapswitch", "0", true); // Caused inf black screen when loading campaign maps.

    Dvar_SetFromStringByName("maxvoicepacketsperframe", "0", true);

    if (IsInjectorlessInstall)
    {
        load_settings_initial();
        // Handle retained so Unload() can wait for the watcher; see Protection::MainThreadHandle.
        Protection::MainThreadHandle = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }

    ZLOG("session: %s | route=%s | name=\"%s\" | friendsOnly=%d | autoInstallMods=%d | password=%s",
        ZBR_VERSION_FULL, Protection::IsInjectorlessInstall ? "injectorless" : "injector",
        Protection::CustomName, (int)Protection::IsFriendsOnly, (int)Protection::IsAutoInstallModsEnabled,
        ZBR_PREFIX_BYTE ? "set" : "none");
    Protection::Installed = true;
}

// Runs from Unload() with every other thread suspended: nothing here may allocate or log.
void Protection::uninstall()
{
    if (!Protection::Installed)
    {
        return;
    }
    Protection::Installed = false;
    Protection::Unloading.store(true);

    // Only if install() armed it: otherwise Old_lobbymsgprints is still 0, and writing it would
    // replace a live game pointer with null.
    if (*(__int64*)PTR_lobbymsgprints == (__int64)0xFFEEDDCC44332212)
    {
        *(__int64*)PTR_lobbymsgprints = Protection::Old_lobbymsgprints;
    }
    SetNetworkPassword(0);
    // 0 means the IAT slot was not restored and still points into this module.
    if (!Iat_hook_::detour_iat_ptr("IsProcessorFeaturePresent", (void*)old_IsProcessorFeaturePresent))
    {
        ++Protection::UninstallIatRestoreFailures;
    }

    // Each original here is the one recorded before the first swap: SwapSteamAPIPointer never
    // overwrites a recorded slot, so restoring cannot record our own hook as the "original".
    SwapSteamAPIPointer(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_GETUSERNAME, (void*)GetOriginalSteamPtr(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_GETUSERNAME));
    SwapSteamAPIPointer(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_VT_NAMEBYXUID, (void*)GetOriginalSteamPtr(STEAMAPI_STEAMUSER, STEAMAPI_STEAMUSER_VT_NAMEBYXUID));

    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT, (void*)GetOriginalSteamPtr(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT));
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2, (void*)GetOriginalSteamPtr(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2));
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_IS_VAC_BANNED, (void*)GetOriginalSteamPtr(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_IS_VAC_BANNED));
    SwapSteamAPIPointer(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS, (void*)GetOriginalSteamPtr(STEAMAPI_INTERFACE, STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS));

    SwapSteamAPIPointer(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY, (void*)GetOriginalSteamPtr(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY));
    SwapSteamAPIPointer(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_CREATELOBBY, (void*)GetOriginalSteamPtr(STEAMAPI_MATCHMAKING, STEAMAPI_MATCHMAKING_CREATELOBBY));
    SwapSteamAPIPointer(STEAMAPI_NETWORKING, STEAMAPI_NETWORKING_READP2PPACKET, (void*)GetOriginalSteamPtr(STEAMAPI_NETWORKING, STEAMAPI_NETWORKING_READP2PPACKET));

    *(__int64*)PTR_UpdatePreloadIdleFN = REBASE(0x131E350);
}

char Protection::requestOut[0x20000]{};
char Protection::lobbyMsgCpy[0x50]{};
void Protection::InspectLM(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
    __int64 retnAddy = *(__int64*)(ContextRecord->Rsp + 0x28);

    // we expect a very specific return addy to operate on
    if (retnAddy != CachedRetnAddy)
    {
        goto ExitCleanly;
    }

    {
        __int64 ctxSuperRsp = ContextRecord->Rsp + 0x30; // calc old Rsp
        __int64 ctxSuperRbx = *(__int64*)(ContextRecord->Rsp + 0x20); // grab old rbx off the stack
        __int32 msgType = *(__int32*)(ctxSuperRsp + 0xB0); // grab the message type [0=host,1=client,3=peer]
        __int64 lobbyMsg = (ctxSuperRsp + 0x88 - 0x58); // lobby message is on the stack (its a LEA)
        __int32* lobbyMsgTypePtr = (__int32*)(lobbyMsg + 0x38);
        __int32 lobbyMsgType = *lobbyMsgTypePtr;

        if (Protection::handle_packet_callbacks.find(lobbyMsgType) != Protection::handle_packet_callbacks.end())
        {
            Protection::handle_packet_callbacks[lobbyMsgType](lobbyMsgTypePtr, lobbyMsg);
            if (*lobbyMsgTypePtr == 0xFF)
            {
                //XLOG("^6DROPPED LOBBYMESSAGE %d", lobbyMsgType);
            }
        }
    }

ExitCleanly:
    ContextRecord->Rcx = Protection::Old_lobbymsgprints;
    ContextRecord->Rbx = Protection::Old_lobbymsgprints;
}

int Protection::MSG_JoinParty_Package_Inspect(char* _this, char* lobbyMsg)
{
    BOOL packageOK =
        LobbyMsgRW_PackageInt(lobbyMsg, "targetlobby", (__int32*)_this)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(sourcelobby), (__int32*)(_this + 4))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(jointype), (__int32*)(_this + 8))
        && LobbyMsgRW_PackageXuid(lobbyMsg, STR(probedxuid), (unsigned __int64*)(_this + 16))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(playlistid), (__int32*)(_this + 612))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(playlistver), (__int32*)(_this + 616))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(ffotdver), (__int32*)(_this + 620))
        && LobbyMsgRW_PackageShort(lobbyMsg, STR(networkmode), (__int16*)(_this + 624))
        && LobbyMsgRW_PackageUInt(lobbyMsg, STR(netchecksum), (unsigned __int32*)(_this + 628))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(protocol), (__int32*)(_this + 632))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(changelist), (__int32*)(_this + 636))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(pingband), (__int32*)(_this + 640))
        && LobbyMsgRW_PackageUInt(lobbyMsg, STR(dlcbits), (unsigned __int32*)(_this + 644))
        && LobbyMsgRW_PackageUInt64(lobbyMsg, STR(joinnonce), (unsigned __int64*)(_this + 648));

    for (int i = 0; packageOK && (i < 3); i++)
    {
        packageOK = packageOK && LobbyMsgRW_PackageUChar(lobbyMsg, STR(chunk), (char*)(_this + i + 689));
    }

    packageOK = packageOK &&
        LobbyMsgRW_PackageBool(lobbyMsg, STR(isStarterPack), (char*)(_this + 656))
        && LobbyMsgRW_PackageString(lobbyMsg, STR(password), (char*)(_this + 657), 0x20)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(membercount), (__int32*)(_this + 24))
        && LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(members));

    if (!packageOK)
    {
        // bad packet
        return 1;
    }

    if (*(__int32*)(_this + 24) > 18)
    {
        // crash attempt via BoF
        return 2;
    }

    if (LobbyMsgRW_PackageElement(lobbyMsg, *(__int32*)(_this + 24) > 0))
    {
        if (*(__int32*)(_this + 24) <= 0) // this is an element even though the packet claims to have 0 members... BoF attempt!
        {
            // crash attempt via BoF
            return 3;
        }

        for (int i = 0; i < *(__int32*)(_this + 24); i++)
        {
            packageOK = packageOK
                && LobbyMsgRW_PackageXuid(lobbyMsg, STR(xuid), (unsigned __int64*)(_this + 616))
                && LobbyMsgRW_PackageUInt64(lobbyMsg, STR(lobbyid), (unsigned __int64*)(_this + 616))
                && LobbyMsgRW_PackageFloat(lobbyMsg, STR(skillrating), (float*)(_this + 616))
                && LobbyMsgRW_PackageFloat(lobbyMsg, STR(skillvariance), (float*)(_this + 616))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(pprobation), (unsigned __int32*)(_this + 616))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(aprobation), (unsigned __int32*)(_this + 616));

            if (!packageOK)
            {
                return 4; // bad packet
            }

            bool expected = (i + 1) < *(__int32*)(_this + 24);
            bool result = LobbyMsgRW_PackageElement(lobbyMsg, expected);
            if (result && !expected)
            {
                return 5; // BoF attempt via package element overflow
            }
        }
    }

    return 0; // packet is fine
}

int Protection::MSG_LobbyState_Package_Inspect(char* __this, char* lobbyMsg)
{
    __int32* _this = (__int32*)__this;

    BOOL packageOK =
        LobbyMsgRW_PackageInt(lobbyMsg, STR(statenum), (__int32*)_this)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(networkmode), (__int32*)(_this + 1))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(mainmode), (__int32*)(_this + 2))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(partyprivacy), (__int32*)(_this + 3))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(lobbytype), (__int32*)(_this + 4))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(lobbymode), (__int32*)(_this + 5))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(sessionstatus), (__int32*)(_this + 6))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(uiscreen), (__int32*)(_this + 7))
        && LobbyMsgRW_PackageUChar(lobbyMsg, STR(leaderactivity), (char*)_this + 32)
        && LobbyMsgRW_PackageString(lobbyMsg, STR(key), (char*)(_this + 9), 0x20)
        && LobbyMsgRW_PackageXuid(lobbyMsg, STR(leader), (unsigned __int64*)(_this + 9))
        && LobbyMsgRW_PackageXuid(lobbyMsg, STR(platformsession), (unsigned __int64*)(_this + 10))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(maxclients), (__int32*)(_this + 22))
        && LobbyMsgRW_PackageBool(lobbyMsg, STR(isadvertised), (char*)_this + 92)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(clientcount), (__int32*)(_this + 24))
        && LobbyMsgRW_PackageString(lobbyMsg, STR(sessionid), (char*)(_this + 5320), 0x40)
        && LobbyMsgRW_PackageString(lobbyMsg, STR(sessioninfo), (char*)(_this + 5336), 0x40)
        && LobbyMsgRW_PackageString(lobbyMsg, STR(ugcName), (char*)(_this + 5352), 0x20)
        && LobbyMsgRW_PackageUInt(lobbyMsg, STR(ugcVersion), (unsigned __int32*)(_this + 5360));

    if (!packageOK)
    {
        // bad packet
        return 1;
    }

    LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(clientlist));

    if (_this[24] > 18)
    {
        // crash attempt via BoF
        return 2;
    }

    int index = 0;
    bool hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, index < _this[24]);


    while (hasNextElement && index < _this[24])
    {
        __int32 offset = 292 * index;
        __int64 offset2 = (__int64)&_this[offset + 26];

        packageOK = packageOK
            && LobbyMsgRW_PackageXuid(lobbyMsg, STR(xuid), (unsigned __int64*)(offset2))
            && LobbyMsgRW_PackageUChar(lobbyMsg, STR(clientNum), (char*)(offset2 + 8))
            && LobbyMsgRW_PackageString(lobbyMsg, STR(gamertag), (char*)(offset2 + 9), 0x20)
            && LobbyMsgRW_PackageBool(lobbyMsg, STR(isGuest), (char*)(offset2 + 41))
            && LobbyMsgRW_PackageUInt64(lobbyMsg, STR(lobbyid), (unsigned __int64*)(offset2 + 48))
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(connectbit), (__int32*)(offset2 + 1108))
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(score), (__int32*)(offset2 + 1104))
            && LobbyMsgRW_PackageGlob(lobbyMsg, STR(address), (char*)(offset2 + 1113), 37)
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(qport), (__int32*)(offset2 + 56))
            && LobbyMsgRW_PackageUChar(lobbyMsg, STR(band), (char*)(offset2 + 60))
            && LobbyMsgRW_PackageUInt(lobbyMsg, STR(netsrc), (unsigned __int32*)(offset2 + 1152))
            && LobbyMsgRW_PackageUInt(lobbyMsg, STR(joinorder), (unsigned __int32*)(offset2 + 1156))
            && LobbyMsgRW_PackageUInt(lobbyMsg, STR(dlcBits), (unsigned __int32*)(offset2 + 1160))
            && MsgMutableClientInfo_Package((char*)(offset2 + 64), lobbyMsg);

        if (!packageOK)
        {
            return 100 + 1; // bad packet
        }

        index++;
        hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, index < _this[24]);
    }

    if (hasNextElement)
    {
        return 100 + 2; // Crash attempt via BoF and PackElem RO exploit
    }

    packageOK = packageOK
        && LobbyMsgRW_PackageUChar(lobbyMsg, STR(migratebits), (char*)_this + 21128)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(lasthosttimems), (__int32*)(_this + 5283));

    if (!packageOK)
    {
        // bad packet
        return 3;
    }

    LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(nomineelist));

    __int32 count = 0;
    hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (count < 18));

    while (hasNextElement && (count < 18))
    {
        packageOK = packageOK && LobbyMsgRW_PackageXuid(lobbyMsg, STR(xuid), (unsigned __int64*)(&_this[0x14A4 + (2 * count)]));
        count++;
        hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (count < 18));
    }

    if (hasNextElement)
    {
        return 100 + 3; // crash attempt via PERO nominee
    }

    if (!packageOK)
    {
        // bad packet
        return 4;
    }

    return 0;
}

int Protection::MSG_LobbyStateGame_Package_Inspect(char* __this, char* lobbyMsg)
{
    int lspiResult = MSG_LobbyState_Package_Inspect(__this, lobbyMsg);

    if (lspiResult)
    {
        return lspiResult;
    }

    __int32* _this = (__int32*)__this;
    bool packageOK = LobbyMsgRW_PackageInt(lobbyMsg, STR(serverstatus), (__int32*)(_this + 5362));

    if (!packageOK)
    {
        return 11;
    }

    packageOK = packageOK
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(launchnonce), (__int32*)(_this + 5363))
        && LobbyMsgRW_PackageUInt64(lobbyMsg, STR(matchhashlow), (unsigned __int64*)(_this + 2682))
        && LobbyMsgRW_PackageUInt64(lobbyMsg, STR(matchhashhigh), (unsigned __int64*)(_this + 2683))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(status), (__int32*)(_this + 5394))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(statusvalue), (__int32*)(_this + 5395))
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(gamemode), (__int32*)(_this + 5368))
        && LobbyMsgRW_PackageString(lobbyMsg, STR(gametype), (char*)(_this + 5369), 0x20)
        && LobbyMsgRW_PackageString(lobbyMsg, STR(map), (char*)(_this + 5377), 0x20)
        && LobbyMsgRW_PackageString(lobbyMsg, STR(ugcName), (char*)(_this + 5385), 0x20)
        && LobbyMsgRW_PackageUInt(lobbyMsg, STR(ugcVersion), (unsigned __int32*)(_this + 5393));

    if (!_this[2])
    {
        packageOK = packageOK
            && LobbyMsgRW_PackageString(lobbyMsg, STR(cpqueuedlevel), (char*)(_this + 7293), 0x20)
            && LobbyMsgRW_PackageBool(lobbyMsg, STR(movieskipped), (char*)_this + 29204);
    }

    __int32 lobbyMode = _this[5];

    if (!lobbyMode)
    {
        LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(clientlist));
        int index = 0;
        bool hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));
        while (hasNextElement && (index < 18))
        {
            __int64 currentClient = (__int64)&_this[9 * index];
            packageOK = packageOK
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(team), (__int32*)(currentClient + 28524))
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(mapvote), (__int32*)(currentClient + 28528))
                && LobbyMsgRW_PackageBool(lobbyMsg, STR(readyup), (char*)(currentClient + 28532));

            if (!packageOK)
            {
                // bad pack
                return 20 + 1;
            }

            index++;
            hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));
        }

        if (hasNextElement)
        {
            // PERO
            return 20 + 2;
        }

        packageOK = packageOK
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(plistid), (__int32*)(_this + 7126))
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(plistcurr), (__int32*)(_this + 7127))
            && LobbyMsgRW_PackageGlob(lobbyMsg, STR(plistentries), (char*)_this + 28512, 8)
            && LobbyMsgRW_PackageUChar(lobbyMsg, STR(plistnext), (char*)_this + 28520)
            && LobbyMsgRW_PackageUChar(lobbyMsg, STR(plistprev), (char*)_this + 28521)
            && LobbyMsgRW_PackageUChar(lobbyMsg, STR(plistprevcount), (char*)_this + 28522);

        if (!packageOK)
        {
            // bad packet
            return 5;
        }

        goto PackageOK;
    }

    lobbyMode--;
    if (!lobbyMode)
    {
        packageOK = packageOK && LobbyMsgRW_PackageInt(lobbyMsg, STR(votecount), (__int32*)(_this + 5397));
        LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(votes));

        int numVotes = *(__int32*)(_this + 5397);
        if (packageOK && (numVotes > 216))
        {
            // crash attempt via votecount BoF
            return 20 + 6;
        }

        int index = 0;
        bool hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < numVotes));

        while (hasNextElement && (index < numVotes))
        {
            __int64 currentVote = (__int64)(&_this[8 * index + 5398]);
            packageOK = packageOK
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(itemtype), (__int16*)(currentVote + 8))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(item), (unsigned int*)(currentVote + 12))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(itemgroup), (__int16*)(currentVote + 16))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(attachment), (__int16*)(currentVote + 20))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(votetype), (__int16*)(currentVote + 24))
                && LobbyMsgRW_PackageXuid(lobbyMsg, STR(votexuid), (unsigned __int64*)(currentVote));

            if (!packageOK)
            {
                return 6; // bad packet
            }

            index++;
            hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < numVotes));
        }

        if (hasNextElement)
        {
            // PERO
            return 20 + 3;
        }

        LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(clientlist));
        index = 0;
        hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));
        while (hasNextElement && (index < 18))
        {
            __int64 currentClient = (__int64)&_this[index + 8 * index + 7131];
            packageOK = packageOK
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(team), (__int32*)(currentClient))
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(pregamepos), (__int32*)(currentClient + 4))
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(pregamestate), (__int32*)(currentClient + 3))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(clvotecount), (unsigned __int32*)(currentClient + 6))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(character), (unsigned __int32*)(currentClient + 7))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(loadout), (unsigned __int32*)(currentClient + 8));

            if (!packageOK)
            {
                // bad packet
                return 7;
            }

            index++;
            hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));
        }

        if (hasNextElement)
        {
            // PERO
            return 20 + 4;
        }

        packageOK = packageOK && LobbyMsgRW_PackageInt(lobbyMsg, STR(settingssize), (__int32*)(_this + 7350));

        if ((_this[7350] <= 0) || (_this[7350] > 0xC000))
        {
            // BoF attempt
            return 20 + 5;
        }

        goto PackageOK;
    }

    lobbyMode--;
    if (!lobbyMode)
    {
        goto PackageOK;
    }

    if (lobbyMode == 1)
    {
        packageOK = packageOK
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(compstate), _this + 5396)
            && LobbyMsgRW_PackageInt(lobbyMsg, STR(votecount), _this + 5397);

        if (!packageOK)
        {
            // bad packet
            return 10;
        }


        __int32 numVotes = _this[5397];
        if (_this[5397] > 216)
        {
            return 20 + 7;
        }

        int index = 0;
        bool hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < numVotes));

        while (hasNextElement && (index < numVotes))
        {
            __int64 currentVote = (__int64)(&_this[8 * index + 5398]);
            packageOK = packageOK
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(itemtype), (__int16*)(currentVote + 8))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(item), (unsigned int*)(currentVote + 12))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(itemgroup), (__int16*)(currentVote + 16))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(attachment), (__int16*)(currentVote + 20))
                && LobbyMsgRW_PackageShort(lobbyMsg, STR(votetype), (__int16*)(currentVote + 24))
                && LobbyMsgRW_PackageXuid(lobbyMsg, STR(votexuid), (unsigned __int64*)(currentVote));

            if (!packageOK)
            {
                return 8; // bad packet
            }

            index++;
            hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < numVotes));
        }

        if (hasNextElement)
        {
            // PERO
            return 20 + 8;
        }

        LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(clientlist));
        index = 0;
        hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));

        while (hasNextElement && (index < 18))
        {
            __int64 currentClient = (__int64)&_this[index + 8 * index + 7131];
            packageOK = packageOK
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(team), (__int32*)(currentClient))
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(pregamepos), (__int32*)(currentClient + 4))
                && LobbyMsgRW_PackageInt(lobbyMsg, STR(pregamestate), (__int32*)(currentClient + 3))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(clvotecount), (unsigned __int32*)(currentClient + 6))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(character), (unsigned __int32*)(currentClient + 7))
                && LobbyMsgRW_PackageUInt(lobbyMsg, STR(loadout), (unsigned __int32*)(currentClient + 8));

            if (!packageOK)
            {
                // bad packet
                return 9;
            }

            index++;
            hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (index < 18));
        }

        if (hasNextElement)
        {
            // PERO
            return 20 + 9;
        }

        goto PackageOK;
    }

PackageOK:

    return 0;
}

int Protection::MSG_HostHeartbeat_Inspect(char* __this, char* lobbyMsg)
{
    bool packageOK = LobbyMsgRW_PackageInt(lobbyMsg, STR(heartbeatnum), (__int32*)__this)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(lobbytype), (__int32*)__this)
        && LobbyMsgRW_PackageInt(lobbyMsg, STR(lasthosttimems), (__int32*)__this);

    if (!packageOK) // invalid packet
    {
        return 1;
    }

    LobbyMsgRW_PackageArrayStart(lobbyMsg, STR(nomineelist));

    __int32 count = 0;
    bool hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (count < 18));
    while (hasNextElement && (count < 18))
    {
        packageOK = packageOK && LobbyMsgRW_PackageXuid(lobbyMsg, STR(xuid), (unsigned __int64*)__this);
        count++;
        hasNextElement = LobbyMsgRW_PackageElement(lobbyMsg, (count < 18));
    }

    if (hasNextElement)
    {
        return 100 + 1; // crash attempt via PERO nominee
    }

    if (!packageOK)
    {
        // bad packet
        return 2;
    }

    return 0;
}

// `message` is the raw instant-message payload, and it is still reinterpreted as a msg_t header
// exactly as upstream does - see AGENTS.md section 8 before changing that: handing this function
// a real msg_t makes the body below run for the first time, and it faults inside the game's
// lobby-message parser. What changed is that the reinterpreted header is no longer trusted.
// data, cursize and readcount are bytes the peer chose, and MSG_ReadData below copies from
// data + readcount, so a peer could pick a small size and any address and have this read
// wherever they liked (and a payload shorter than a msg_t over-read the receive buffer itself).
// Now the header is only used when it describes bytes inside the payload that actually arrived.
// Real payloads essentially never do, and for them upstream's size guard below almost always
// failed anyway, so they get the same false; the trace line firing on most 0x68s is normal.
bool Protection::CheckPendingInfoRequests(__int64 XUID, const char* message, unsigned int messageSize)
{
    if (!message || messageSize < sizeof(msg_t))
    {
        return false;
    }

    msg_t cpyMsg;
    memcpy(&cpyMsg, message, sizeof(msg_t));

    const uintptr_t payloadBegin = (uintptr_t)message;
    const uintptr_t payloadEnd = payloadBegin + messageSize;
    const uintptr_t claimedData = (uintptr_t)cpyMsg.data;
    const bool headerDescribesThisPayload =
        cpyMsg.readcount <= cpyMsg.cursize
        && cpyMsg.cursize <= messageSize
        && claimedData >= payloadBegin
        && claimedData <= payloadEnd
        && payloadEnd - claimedData >= cpyMsg.cursize;

    if (!headerDescribesThisPayload)
    {
        ZLOG("checkPending: ignored header that does not describe the payload (len=%u)", messageSize);
        return false;
    }

    unsigned int size = cpyMsg.cursize - cpyMsg.readcount;
    if (size < 2048u)
    {
        char data[2048]{};
        MSG_ReadData(&cpyMsg, data, size);

        if (!cpyMsg.overflowed)
        {
            LobbyMsg lobby_msg{};
            if (!LobbyMsgRW_PrepReadData(&lobby_msg, data, size))
            {
                //XLOG("DROP LM: FAILED TO READ DATA");
                return false;
            }

            if (lobby_msg.msgType == MESSAGE_TYPE_INFO_RESPONSE)
            {
                Msg_InfoResponse response{};
                __int32 result = MSG_InfoResponseSafe(&response, &lobby_msg);
                if (result)
                {
                    //XLOG("DROP LM: REASON %d", result);
                    return true;
                }

                dwInstantHandleLobbyMessage(XUID, 0, (char*)message);
                return true;
            }
            else
            {
                __int64 localXuid = 0;
                if (!XUID || (TryGetLocalXuid(localXuid) && XUID == localXuid))
                {
                    //XLOG("KEEP LM: XUID LOCAL");
                    return false;
                }

                INT64 expectedMask = 0x110000000000000;
                INT64 andMask = 0xFFFF00000000000;

                if (Protection::IsFriendsOnly)
                {
                    if ((andMask & XUID) != expectedMask)
                    {
                        //XLOG("KEEP LM: SERVER OR SPLITSCREEN");
                        return false; // server or splitscreen
                    }

                    //if (((bool(__fastcall*)(int, __int64))PTR_LiveFriends_IsFriendByXUID)(0, XUID))
                    if (IsFriendByXUIDUncached(XUID))
                    {
                        //XLOG("KEEP LM: FRIEND");
                        return false; // they are a friend! lets respond
                    }

                    //XLOG("DROP LM: UNKNOWN");
                    // not a friend, dont respond
                    return true;
                }
            }
        }
    }

    return false;
}

int Protection::MSG_InfoResponseSafe(Msg_InfoResponse* infoResponse, LobbyMsg* lm)
{
    bool packIsOK = true;
    packIsOK = LobbyMsgRW_PackageUInt(lm, "nonce", (unsigned __int32*)&infoResponse->nonce)
        && LobbyMsgRW_PackageInt(lm, "uiscreen", &infoResponse->uiScreen)
        && LobbyMsgRW_PackageUChar(lm, "nattype", &infoResponse->natType);

    if (!packIsOK)
    {
        return 1;
    }

    LobbyMsgRW_PackageArrayStart(lm, "lobbies");


    bool hasNextElement = LobbyMsgRW_PackageElement(lm, true);
    bool hasNextElement2 = false;
    for (__int32 cResponse = 0; hasNextElement && (cResponse < 2); cResponse++)
    {
        LobbyMsgRW_PackageArrayStart(lm, "lobbies");

        hasNextElement2 = LobbyMsgRW_PackageElement(lm, true);
        for (__int32 i = 0; hasNextElement2 && (i < 2); i++)
        {
            packIsOK = packIsOK && LobbyMsgRW_PackageBool(lm, "valid", &infoResponse->lobby[cResponse].isValid);

            if (!packIsOK)
            {
                return 2;
            }

            if (!infoResponse->lobby[cResponse].isValid)
            {
                hasNextElement2 = LobbyMsgRW_PackageElement(lm, (i < 2));
                continue;
            }

            packIsOK = packIsOK
                && LobbyMsgRW_PackageXuid(lm, "hostxuid", (unsigned __int64*)&infoResponse->lobby[cResponse].hostXuid)
                && LobbyMsgRW_PackageString(lm, "hostname", infoResponse->lobby[cResponse].hostName, 32)
                && LobbyMsgRW_PackageInt(lm, "networkmode", &infoResponse->lobby[cResponse].lobbyParams.networkMode)
                && LobbyMsgRW_PackageInt(lm, "mainmode", &infoResponse->lobby[cResponse].lobbyParams.mainMode)
                && LobbyMsgRW_PackageGlob(lm, "secid", (char*)&infoResponse->lobby[cResponse].secId.id, 8)
                && LobbyMsgRW_PackageGlob(lm, "seckey", infoResponse->lobby[cResponse].secKey.ab, 16)
                && LobbyMsgRW_PackageGlob(lm, "addrbuff", infoResponse->lobby[cResponse].serializedAdr.addrBuf, 37)
                && LobbyMsgRW_PackageString(lm, "ugcName", infoResponse->lobby[cResponse].ugcName, 32)
                && LobbyMsgRW_PackageUInt(lm, "ugcVersion", (unsigned __int32*)&infoResponse->lobby[cResponse].ugcVersion);

            if (!packIsOK)
            {
                return 3;
            }
            hasNextElement2 = LobbyMsgRW_PackageElement(lm, (i < 2));
        }

        if (hasNextElement2)
        {
            return 5;
        }

        hasNextElement = LobbyMsgRW_PackageElement(lm, (cResponse < 2));
    }

    if (hasNextElement || lm->msg.overflowed)
    {
        return 4;
    }

    return 0;
}

void Protection::ExceptHook(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
    // Protection::ZwContinue is NULL and nothing assigns it, as in every release up to 3.04
    // (AGENTS.md §10). Every branch below ends by calling through it, so reaching any of them used
    // to fault again at RIP 0 inside this handler - the "RIP 0x0" crashes.log entry. Returning
    // instead hands the fault back to dllmain.cpp's ExceptHook, which uses its own resolved
    // ZwContinue: it has identical handlers for 0x22C965C and LuaCrash1-3, and everything else
    // here (0x22C9676, the sentinel) reaches its crash-log path at the real fault address.
    //
    // That also means InspectLM no longer runs. It used to, just before the null call, and its
    // result never mattered because the process stopped either way. Do NOT "fix" this by
    // resolving ZwContinue: the game would then resume with whatever InspectLM did. It finds the
    // lobby message through hardcoded stack-frame offsets (Rsp+0x20/+0x28/+0x30, then +0xB0 and
    // +0x88-0x58) that are BlackOps3.exe's compiler frame layout, and writes 0xFF through one of
    // them. Nothing pins those offsets and they have not been verified on either supported build;
    // wrong ones would make a peer-reachable fault a stack write the game keeps running on.
    // InspectLM stays unarmed until its offsets are verified on both builds.
    if (!ZwContinue)
    {
        return;
    }

    if ((INT64)ContextRecord->Rcx == 0xFFEEDDCC44332212)
    {
        InspectLM(ExceptionRecord, ContextRecord);
        ZwContinue(ContextRecord, false);
        return;
    }

    // From v2.04
    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x22C965C)) // character index crash
    {
        ContextRecord->Rip = REBASE(0x22C9686);
        ZwContinue(ContextRecord, false);
        return;
    }

    // From v2.04
    if (ExceptionRecord->ExceptionAddress == (PVOID)REBASE(0x22C9676)) // character index crash
    {
        ContextRecord->Rip = REBASE(0x22C9686);
        ZwContinue(ContextRecord, false);
        return;
    }

    if (ExceptionRecord->ExceptionAddress == (PVOID)LuaCrash1)
    {
        ContextRecord->Rip = LuaCrash1Rip;
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
        ContextRecord->Rdx = (INT64)UILocalizeDefaultText;
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
        ContextRecord->Rsi = (INT64)UILocalizeDefaultText;
        ZwContinue(ContextRecord, false);
        return;
    }

}


namespace Iat_hook_
{
    void** find(const char* function, HMODULE module)
    {
        if (!module)
            module = GetModuleHandle(0);

        if (!module || !function)
            return 0;

        PIMAGE_DOS_HEADER img_dos_headers = (PIMAGE_DOS_HEADER)module;
        if (img_dos_headers->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        PIMAGE_NT_HEADERS img_nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)img_dos_headers + img_dos_headers->e_lfanew);
        if (img_nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return 0;

        const auto importDirRva = img_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (!importDirRva)
            return 0;

        PIMAGE_IMPORT_DESCRIPTOR img_import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)img_dos_headers + importDirRva);

        for (IMAGE_IMPORT_DESCRIPTOR* iid = img_import_desc; iid->Name != 0; iid++) {
            // A bound descriptor can have no name array; without this the module base is read as
            // a thunk array.
            if (!iid->OriginalFirstThunk || !iid->FirstThunk)
                continue;

            for (int func_idx = 0; *(func_idx + (void**)(iid->FirstThunk + (size_t)module)) != nullptr; func_idx++) {
                char* mod_func_name = (char*)(*(func_idx + (size_t*)(iid->OriginalFirstThunk + (size_t)module)) + (size_t)module + 2);
                const intptr_t nmod_func_name = (intptr_t)mod_func_name;
                if (nmod_func_name >= 0) {
                    if (!::strcmp(function, mod_func_name))
                        return func_idx + (void**)(iid->FirstThunk + (size_t)module);
                }
            }
        }

        return 0;

    }

    uintptr_t detour_iat_ptr(const char* function, void* newfunction, HMODULE module)
    {
        auto&& func_ptr = find(function, module);
        // find() returns 0 for a function the module does not import; this dereferenced it.
        if (!func_ptr || *func_ptr == newfunction || *func_ptr == nullptr)
            return 0;

        DWORD old_rights, new_rights = PAGE_READWRITE;
        if (!VirtualProtect(func_ptr, sizeof(uintptr_t), new_rights, &old_rights))
            return 0;

        uintptr_t ret = (uintptr_t)*func_ptr;
        *func_ptr = newfunction;
        VirtualProtect(func_ptr, sizeof(uintptr_t), old_rights, &new_rights);
        return ret;
    }
};
