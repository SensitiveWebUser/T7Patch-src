#pragma once
#include "framework.h"

namespace Iat_hook_
{
	void** find(const char* function, HMODULE module);

	uintptr_t detour_iat_ptr(const char* function, void* newfunction, HMODULE module = 0);
};

typedef BOOL(__fastcall* tLobbyMsgRW_PackageInt)(void* lobbyMsg, const char* key, int32_t* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageUChar)(void* lobbyMsg, const char* key, char* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageString)(void* lobbyMsg, const char* key, const char* val, int maxLength);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageXuid)(void* lobbyMsg, const char* key, unsigned __int64* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageBool)(void* lobbyMsg, const char* key, char* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageUInt)(void* lobbyMsg, const char* key, unsigned __int32* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageShort)(void* lobbyMsg, const char* key, __int16* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageUInt64)(void* lobbyMsg, const char* key, unsigned __int64* val);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageArrayStart)(void* lobbyMsg, const char* key);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageElement)(void* lobbyMsg, BOOL addElement);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageGlob)(void* lobbyMsg, const char* key, const char* val, int maxLength);
typedef BOOL(__fastcall* tLobbyMsgRW_PackageFloat)(void* lobbyMsg, const char* key, float* val);
typedef BOOL(__fastcall* tMsgMutableClientInfo_Package)(void* outRequest, void* lobbyMsg);
typedef BOOL(__fastcall* tdwInstantHandleLobbyMessage)(__int64 senderID, int controllerIndex, char* message);
typedef bool(__fastcall* tLobbySession_GetControllingLobbySession)(LobbyModule lMod);
typedef LobbySession* (__fastcall* tLobbySession_GetSession)(unsigned int s);
typedef SessionClient* (__fastcall* tLobbySession_GetClientByClientNum)(LobbySession* session, int m);
typedef unsigned long long(__fastcall* tZwContinue)(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);
typedef int(__fastcall* tI_stricmp)(const char* str1, const char* str2);
typedef void(__fastcall* tMSG_ReadData)(msg_t* msg, char*, int);
typedef bool(__fastcall* tLobbyMsgRW_PrepReadData)(LobbyMsg*, char*, int);
typedef bool(__fastcall* tCbuf_AddText)(int controllerIndex, const char* input, int alwaysZero);

class Protection
{
public:
	static char CustomName[16];
	static __int64 Old_lobbymsgprints;
	static __int64 CachedRetnAddy;
	static __int64 CachedXUID;
	static char requestOut[0x20000];
	static char lobbyMsgCpy[0x50];
	static bool IsFriendsOnly;
	static bool IsInjectorlessInstall;
	static bool Installed;
	static std::atomic<bool> Unloading;
	static bool ExceptionHookInstalled;
	static void* MainThreadHandle;
	static thread_local int InspectorDepth;

	struct InspectorScope
	{
		InspectorScope() { ++Protection::InspectorDepth; }
		~InspectorScope() { --Protection::InspectorDepth; }
		
		InspectorScope(const InspectorScope&) = delete;
		InspectorScope& operator=(const InspectorScope&) = delete;
	};

	static void install();
	static void bind_game_pointers();
	static void RecordSteamOriginal(__int64 hLibrary, int vPointerIndex);

	static unsigned int UninstallIatRestoreFailures;
	static unsigned int UninstallVtableProtectFailures;
	static void uninstall();
	static bool IsBadReadPtr(void* p);
	static const char* GetUsernamePtr(INT64 a);
	static const char* GetUsernameXUIDPtr(INT64 a, INT64 b);
	static void SwapSteamAPIPointer(__int64 hLibrary, int vPointerIndex, void* CallFuncReplace, bool recordOriginal = true);
	static INT64 GetOriginalSteamPtr(__int64 hLibrary, int vtIndex);
	static __int32 CL_SwitchState_Idle_Update(INT64 sw);
	//static void SetThreadExceptions(PCONTEXT ThreadContext); // (Debug registers are now being watched, no bypass yet but protections and arxan evasion have been changed to stop relying on this)
	static void ExceptHook(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
	static void InspectLM(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
	static int MSG_JoinParty_Package_Inspect(char* _this, char* lobbyMsg);
	static int MSG_LobbyState_Package_Inspect(char* __this, char* lobbyMsg);
	static int MSG_LobbyStateGame_Package_Inspect(char* __this, char* lobbyMsg);
	static int MSG_HostHeartbeat_Inspect(char* __this, char* lobbyMsg);
	static bool CheckPendingInfoRequests(__int64 XUID, const char* message, unsigned int messageSize);
	static bool ReadP2PPacket(uintptr_t thisptr, void* pub_dest, unsigned int cub_dest, unsigned int* cub_msg_size, unsigned __int64* steam_id_remote, int n_channel);
	static bool GetOwnsContent(INT64 a, INT32 b);
	static bool GetOwnsContent2(INT64 a, INT32 b);
	static bool IsVacBanned(INT64 a);
	static void GetDlcDownloadProgress(INT64 a, INT32 b, INT64* c, INT64* d);
	static __int32 GetLobbyChatEntry(INT64 api, INT64 a, INT64 b, INT64 c, INT64 d, INT64 e, INT64 f);
	static bool IsFriendByXUIDUncached(__int64 xuid);
	static __int64 CreateLobby(INT64 api, __int32 a, __int32 b);
	static int MSG_InfoResponseSafe(Msg_InfoResponse* infoResponse, LobbyMsg* lm);
	static void SetNetworkPassword(__int64 pass);

	static tZwContinue ZwContinue;
	static tI_stricmp I_stricmp;
	static char* UILocalizeDefaultText;

	static tLobbyMsgRW_PackageInt LobbyMsgRW_PackageInt;
	static tLobbyMsgRW_PackageUChar LobbyMsgRW_PackageUChar;
	static tLobbyMsgRW_PackageString LobbyMsgRW_PackageString;
	static tLobbyMsgRW_PackageXuid LobbyMsgRW_PackageXuid;
	static tLobbyMsgRW_PackageBool LobbyMsgRW_PackageBool;
	static tLobbyMsgRW_PackageUInt LobbyMsgRW_PackageUInt;
	static tLobbyMsgRW_PackageShort LobbyMsgRW_PackageShort;
	static tLobbyMsgRW_PackageUInt64 LobbyMsgRW_PackageUInt64;
	static tLobbyMsgRW_PackageArrayStart LobbyMsgRW_PackageArrayStart;
	static tLobbyMsgRW_PackageElement LobbyMsgRW_PackageElement;
	static tLobbyMsgRW_PackageGlob LobbyMsgRW_PackageGlob;
	static tMsgMutableClientInfo_Package MsgMutableClientInfo_Package;
	static tdwInstantHandleLobbyMessage dwInstantHandleLobbyMessage;
	static tLobbyMsgRW_PackageFloat LobbyMsgRW_PackageFloat;
	static tMSG_ReadData MSG_ReadData;
	static tLobbyMsgRW_PrepReadData LobbyMsgRW_PrepReadData;
	static tLobbySession_GetControllingLobbySession LobbySession_GetControllingLobbySession;
	static tLobbySession_GetSession LobbySession_GetSession;
	static tLobbySession_GetClientByClientNum LobbySession_GetClientByClientNum;
	static tCbuf_AddText Cbuf_AddText;

	static __int64 PrivatePassword[3];
	static std::unordered_map<__int64, std::unordered_map<int, __int64>> SteamHAPIHooks;
	static std::unordered_map<BYTE, std::function<void(__int32* lobbyMsgTypePtr, __int64 lobbyMsg)>> handle_packet_callbacks;
};

#define STEAMVTO(x) (x / 8)

#define STEAMAPI_STEAMUSER_GETUSERNAME STEAMVTO(0)
#define STEAMAPI_INTERFACE_IS_VAC_BANNED STEAMVTO(0x18)
#define STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT2 STEAMVTO(0x30)
#define STEAMAPI_INTERFACE_CHECK_OWNS_CONTENT STEAMVTO(0x38)
#define STEAMAPI_INTERFACE_GET_DLC_DOWNLOAD_PROGRESS STEAMVTO(0xB0)
#define STEAMAPI_MATCHMAKING_GETLOBBYCHATENTRY STEAMVTO(0xD8)
#define STEAMAPI_MATCHMAKING_CREATELOBBY STEAMVTO(0x68)
#define STEAMAPI_NETWORKING_READP2PPACKET STEAMVTO(0x10)

// steamapi, who, inviteArg
#define STEAMAPI_STEAMUSER_VT_NAMEBYXUID STEAMVTO(0x38)
#define IHOOK_HEADER(fn_name, fn_return, fn_args) using fn_name ## _fn = fn_return (__stdcall*)fn_args; \
fn_name ## _fn o ## fn_name; \
fn_return WINAPI __ ## fn_name fn_args
#define IHOOK_INSTALL(fn_name, module__) o ## fn_name = (fn_name ## _fn)Iat_hook_::detour_iat_ptr(#fn_name, (void*)__ ## fn_name, module__);
#define IHOOK_ORIGINAL(fn_name, fn_args) o ## fn_name fn_args

#define ZBR_PREFIX_BYTE ((unsigned char)((Protection::PrivatePassword[1] & 0xFF0000) >> 16))
#define ZBR_PREFIX_BYTE2 ((unsigned char)((Protection::PrivatePassword[1] & 0xFF000000) >> 24))

#define ZBR_INJECTOR_PASSWORD_MAXLEN 15u

#define ZBR_HAS_PASSWORD (Protection::PrivatePassword[1] != 0)
#define ZBR_HAS_PREV_PASSWORD (Protection::PrivatePassword[0] != 0)