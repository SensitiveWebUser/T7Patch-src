#include "Hooks.h"
#include "CampaignFixes.h"
#include "LuaSymbols.h"

const char zbr_window_text[] = ZBR_WINDOW_TEXT;
void* pOriginalGSFailure = nullptr;
const char* bad_str = "bad";

namespace hooks {

	// From Scropts-QOL
	const std::vector<uintptr_t> isModLoadedAddrs =
	{
		{ 0x20EB3C4 },
		{ 0x20EB3E4 },
		{ 0x20EB324 },
		{ 0x20EB2F4 },
		{ 0x20EAD97 },
		{ 0x13E6A74 },
		{ 0x15E7EDB },
		{ 0x15E87DB },
		{ 0x15EBAE9 },
		{ 0x15F1F29 },
		{ 0x15F1FD9 },
		{ 0x15F20A0 },
		{ 0x15F7F60 },
		{ 0x1A80D1D },
		{ 0x1A9AD29 },
		{ 0x1E9432B },
		{ 0x1E9C8AE },
		{ 0x1EA5669 },
		{ 0x1EBF6CD },
		{ 0x20EAD5D },
	};

	uintptr_t GetCallsiteFromReturn(void* returnAddress)
	{
		uint8_t* ret = (uint8_t*)returnAddress;

		if (ret[-5] == 0xE8)
		{
			return (uintptr_t)(ret - 5);
		}

		for (int i = 1; i <= 16; i++)
		{
			if (ret[-i] == 0xE8)
			{
				return (uintptr_t)(ret - i);
			}
		}

		return (uintptr_t)returnAddress;
	}

	namespace functions
	{

		__int64 hkLiveInventory_GetItemQuantity(ControllerIndex_t controllerIndex, int itemId) {

			#if SPOOF_UNLOCK
				// Source: /gamedata/loot/zmlootitems.csv
				if (itemId >= 1000000010 && itemId < 1000000200) {
					return SPOOF_GUM_COUNT;
				} else {
					return 1;
				}
			#endif

			return LiveInventory_GetItemQuantity(controllerIndex, itemId);
		}

		bool hkLiveInventory_AreExtraSlotsPurchased(ControllerIndex_t controllerIndex) {
		
			#if SPOOF_UNLOCK
				return true;
			#endif

			return LiveInventory_AreExtraSlotsPurchased(controllerIndex);
		}

		const char* hkInfo_ValueForKey(char* a1, __int64 a2) {
			return Info_ValueForKey(a1, a2);
		}
		
		// Caused uninstalled content to show as available.
		bool hkLiveInventory_IsValid(ControllerIndex_t controllerIndex) {

			#if SPOOF_UNLOCK
				return true;
			#endif

			return LiveInventory_IsValid(controllerIndex);
		}

		// Source: /gamedata/store/common/incentives.csv
		bool hkLiveEntitlements_IsEntitlementActiveForController(ControllerIndex_t controllerIndex, int incentiveId) {

			#if SPOOF_UNLOCK

				// Invalid / Duplicates
				if (incentiveId == 29 || incentiveId == 30 || incentiveId == 34) {
					return false;
				}

				#if SPOOF_SKIP_CWL

					if (incentiveId == 21 || incentiveId == 22 || incentiveId == 23) {
						return false;
					}

				#endif

				return true;

			#endif

			return LiveEntitlements_IsEntitlementActiveForController(controllerIndex, incentiveId);
		}

		bool hkUserHasLicenseForApp(__int64 mapInfo, __int64* userObj) {
			#if SPOOF_UNLOCK
				if (userObj)
				{
					*((BYTE*)userObj + 13) = 1;
					*((BYTE*)userObj + 12) |= 0;
					*((DWORD*)userObj + 4) |= 8u;
				}
				return true;
			#endif

			return UserHasLicenseForApp(mapInfo, userObj);
		}

		const char* hkBG_Cache_GetScriptMenuNameForIndex(unsigned int inst, unsigned int index)
		{
			if (index >= 64u)
				return bad_str;

			return BG_Cache_GetScriptMenuNameForIndex(inst, index);
		}

		const char* hkBG_Cache_GetEventStringNameForIndex(unsigned int inst, unsigned int index)
		{
			if (index >= 256u)
				return bad_str;

			return BG_Cache_GetEventStringNameForIndex(inst, index);
		}

		const char* hkBG_Cache_GetLocStringNameForIndex(unsigned int inst, unsigned int index)
		{
			if (index >= 2048u)
				return bad_str;

			return BG_Cache_GetLocStringNameForIndex(inst, index);
		}

		const char* hkBG_Cache_GetLUIMenuForIndex(unsigned int inst, unsigned int index)
		{
			if (index >= 64u)
				return bad_str;

			return BG_Cache_GetLUIMenuForIndex(inst, index);
		}

		const char* __fastcall hkBG_Cache_GetLUIMenuDataForIndex(unsigned int inst, unsigned int index)
		{
			if (index >= 128u)
				return bad_str;

			return BG_Cache_GetLUIMenuDataForIndex(inst, index);
		}

		void hkLiveInvites_AcceptInvite(UINT controllerIndex, DWORD* message, __int64 recepientXUID) // Prevent non-friends from 'pulling' us into lobbies.
		{
			if (Protection::IsFriendsOnly)
			{
				if (!Protection::IsFriendByXUIDUncached(recepientXUID))
				{
					ZLOG("invite: DROP accept from non-friend xuid=%p", (void*)recepientXUID);
					return;
				}
			}
			return LiveInvites_AcceptInvite(controllerIndex, message, recepientXUID);
		}

		void hkLiveInvites_JoinMessageAction(UINT controllerIndex, DWORD* message, __int64 recepientXUID) // Prevent non-friends from 'pulling' us into lobbies.
		{
			if (Protection::IsFriendsOnly)
			{
				if (!Protection::IsFriendByXUIDUncached(recepientXUID))
				{
					ZLOG("invite: DROP join message from non-friend xuid=%p", (void*)recepientXUID);
					return;
				}
			}
			return LiveInvites_JoinMessageAction(controllerIndex, message, recepientXUID);
		}

		__int64 hkLiveInvites_SendJoinInfo(UINT controllerIndex, __int64 recepientXUID, __int64 a3) // Prevent non-friends from 'pulling' us into lobbies.
		{
			if (Protection::IsFriendsOnly)
			{
				if (!Protection::IsFriendByXUIDUncached(recepientXUID))
				{
					ZLOG("sendJoinInfo: DROP non-friend xuid=%p", (void*)recepientXUID);
					return 0;
				}
			}

			return LiveInvites_SendJoinInfo(controllerIndex, recepientXUID, a3);

		}

		char hkUI_BrowserOpen(__int64 a1)
		{
			return 0;
		}

		bool hkLive_UserGetName(ControllerIndex_t controllerIndex, char* buf, int bufSize) {

			__int64 v3; // rbx
			int* v6; // rax
			int v9; // [rsp+40h] [rbp+8h] BYREF

			v3 = bufSize;
			v9 = bufSize;
			Memset(buf, 0LL, bufSize);
			v6 = &v9;

			Live_Base_UserGetName((UINT8*)buf, *v6, 1);

			return 1;
		}

		float hkflsomeWeirdCharacterIndex(__int64 a1, int a2, int a3) {

			// The v2.04 "character index crash": a2 indexes a table the hook cannot size, so at least
			// refuse a negative index and an entry that is not mapped, and fall through to this hook's
			// own -1.0f rather than fault reading it.
			if (!a1 || a2 < 0) {
				return -1.0f;
			}

			auto slot = (DWORD64*)(a1 + 24LL * a2 + 56);
			if (Protection::IsBadReadPtr(slot) || Protection::IsBadReadPtr((char*)slot + 7)) {
				return -1.0f;
			}

			__int64 v3 = *slot; // r9
			if (v3) {
				return flsomeWeirdCharacterIndex(a1, a2, a3);
			}

			return -1.0f;

		}

		bool hkCL_ConnectionlessCMD(int clientNum, netadr_t* from, msg_t* msg) {

			auto v7 = *(int**)(Sys_GetTLS() + 24);
			auto v8 = *v7;
			auto message = **(CHAR***)&v7[2 * v8 + 34];

			// is_in_array below builds a std::string from it, which is undefined for null.
			if (!message)
			{
				return 1;
			}

			// From v2.04
			auto mode = Com_SessionMode_GetModeName();

			// mode can be null outside a session; test it before the comparisons rather than pass
			// it to I_stricmp. Same accept set as before.
			if (mode && !Protection::I_stricmp(mode, "CP") && (!Protection::I_stricmp(message, "requeststats") || !Protection::I_stricmp(message, "requeststats\n")))
			{
				return CL_ConnectionlessCMD(clientNum, from, msg);
			}

			// Filter packets, only allow legit packets through, block the rest
			if (is_in_array(message, legit_packets)) {
				return CL_ConnectionlessCMD(clientNum, from, msg);
			}

			return 1;
		}
		
		const char* __fastcall hkCL_GetConfigString(std::int32_t configStringIndex)
		{

			constexpr auto mspreload_command = "mspreload";
			// static: this runs per config-string read, so do not rebuild the list every call.
			static const std::vector<int> config_strings = { 3514, 3627 };

			auto mode = Com_SessionMode_GetModeName();
			if (mode && !Protection::I_stricmp(mode, "CP"))
			{
				return CL_GetConfigString(configStringIndex);
			}

			if (is_in_number_array(configStringIndex, config_strings))
			{
				// Config strings are server-sent; is_equal takes std::string, so null must not reach it.
				if (auto config_string{ CL_GetConfigString(configStringIndex) };
					config_string && is_equal(config_string, mspreload_command, std::strlen(mspreload_command), false))
				{
					// Loadside attempt caught, return empty string to prevent crash
					CL_StoreConfigString(configStringIndex, "");
				}
			}

			return CL_GetConfigString(configStringIndex);
		}

		void hkLobbyMsgRW_PrintMessage()
		{
			// do nothing, bugged function
		}

		__int32 hkLobbyMsgRW_PrintDebugMessage()
		{
			return 0;
		}

		void hkExecLuaCMD()
		{
			// do nothing, bugged function
		}

		const char* hkSEH_ReplaceDirectiveInStringWithBinding(int localClientNum, const char* translatedString, char* finalString)
		{
			char input[4096];

			if (!translatedString)
			{
				translatedString = "";
			}

			if (strlen(translatedString) > 4095)
			{
				return 0;
			}

			if (!finalString)
			{
				return 0;
			}

			strcpy_s(input, translatedString);
			input[4095] = 0;
			int max = strlen(input);

			for (int i = 0; i < max; i++)
			{
				if (input[i] != '^' || (i + 1) >= max)
				{
					continue;
				}
				if (input[i + 1] != 'B')
				{
					continue;
				}
				bool b_found = false;
				int j = i + 2;
				int count = 0;
				for (; j < max; j++, count++)
				{
					if (input[j] == '^')
					{
						b_found = true;
						if (count >= 0x40)
						{
							b_found = false;
						}
						break;
					}
				}
				if (!b_found)
				{
					input[i] = '.';
					i++;
				}
				else
				{
					i = j;
				}
			}

			auto tokenPos = strstr(input, "[{");
			while (tokenPos)
			{
				auto endTokenPos = strstr(tokenPos + 2, "}]");
				if (!endTokenPos || (endTokenPos - tokenPos) >= 0x100)
				{
					*tokenPos = '.';
					*(tokenPos + 1) = '.';
				}
				tokenPos = strstr(tokenPos + 2, "[{");
			}

			return SEH_ReplaceDirectiveInStringWithBinding(localClientNum, input, finalString);
		}

		__int64 hkqmemcpy(char* dest, char* source, __int32 size)
		{
			if (size < 0)
			{
				*dest = 0;
				*source = 0;
				return 0;
			}
			return qmemcpy(dest, source, size);
		}

		bool hkUI_DoModelStringReplacement(__int32 controllerIndex, char* element, const char* source, char* dest, unsigned int destSize)
		{
			if (!dest || !destSize)
			{
				return false;
			}

			// Truncating copies. strcpy_s with a source that does not fit is not a truncation, it is
			// the invalid-parameter handler, which fast-fails the process past our own exception
			// handler - and model strings carry peer-supplied text (names, chat).
			char input[4096]{};
			strncpy_s(input, sizeof(input), source ? source : "", _TRUNCATE);
			int max = strlen(input);

			bool b_replace = false;
			for (int i = 0; i < max; i++)
			{
				if (input[i] != '$' || (i + 1) >= max)
				{
					continue;
				}
				if (input[i + 1] != '(')
				{
					continue;
				}
				bool b_found = false;
				int j = i + 2;
				for (; j < max; j++)
				{
					if (input[j] == ')')
					{
						b_found = true;
						break;
					}
				}
				if (!b_found)
				{
					b_replace = true;
					input[i] = '.';
					input[i + 1] = '.';
					i++;
				}
				else
				{
					i = j;
				}
			}

			if (b_replace)
			{
				strncpy_s(dest, destSize, input, _TRUNCATE);
				return true;
			}

			return UI_DoModelStringReplacement(controllerIndex, element, input, dest, destSize);
		}

		const char* hkLobbyTypes_GetMsgTypeName(__int32 index)
		{
			if (index < -1)
			{
				return "invalid";
			}

			if (index > MESSAGE_TYPE_DEMO_STATE)
			{
				if (index >= MESSAGE_TYPE_ZBR_COUNT)
				{
					return "invalid";
				}

				return "invalid";
			}

			return LobbyTypes_GetMsgTypeName(index);
		}		
		
		__int64 hkSys_VerifyPacketChecksum(const char* payload, __int32 payloadLen)
		{
			if (payloadLen >= 2)
			{
				auto newLen = payloadLen - 2;
				auto checksum = (unsigned __int16)Protection::PrivatePassword[1] ^ Sys_Checksum((unsigned __int8*)payload, payloadLen - 2);
				if (*(unsigned __int16*)(newLen + payload) == (unsigned __int16)checksum)
				{
					return newLen;
				}
				else
				{
					// check if the last time we updated the private password was within the past second
					//
					// Only when there was a previous password. With none, PrivatePassword[0] is 0 and
					// this "previous key" is the unkeyed checksum, so for 1.5s after a password is set
					// (including at every launch, where install() clears it first) a peer with no
					// password, or no patch, was accepted. Receive-side only: what we send, and the
					// keys checked for a peer who does have a password, are unchanged.
					if (Protection::PrivatePassword[0] != 0
						&& GetTickCount64() <= Protection::PrivatePassword[2] + 1500)
					{
						checksum = checksum ^ (unsigned __int16)Protection::PrivatePassword[1] ^ (unsigned __int16)Protection::PrivatePassword[0];
						if (*(unsigned __int16*)(newLen + payload) == (unsigned __int16)checksum)
						{
							return newLen;
						}
					}
				}
				//ALOG("^6Dropping packet checksum %d != %d\n\n\n\n", checksum, *(__int16*)(newLen + payload));
			}
			return -1;
		}

		unsigned __int16 hkSys_ChecksumCopy(const char* desta, const char* srca, __int32 length) {
			auto val = Sys_ChecksumCopy(desta, srca, length);
			/*if ((*(__int32*)(LOCAL_CLIENT_CONSTATE + 8) >= 0xB))
			{
				val ^= (__int16)Protection::PrivatePassword[1];
			}*/
			val ^= (unsigned __int16)Protection::PrivatePassword[1];
			return val;
		}

		void __report_gsfailure_hook(__int64 security_cookie) { *(__int64*)(0x17) = 0; };

		const char* hkCOD_GetBuildTitle()
		{
			return zbr_window_text;
		}

		void hkLiveSteam_InitServer() 
		{
			return;
		}

		bool hkLive_SystemInfo(int controllerIndex, int infoType, char* outputString, const int outputLen)
		{
			if (infoType)
			{
				return Live_SystemInfo(controllerIndex, infoType, outputString, outputLen);
			}
			if (!outputString || outputLen <= 0)
			{
				return false;
			}

			strncpy_s(outputString, outputLen, ZBR_VERSION_FULL, _TRUNCATE);
			return true;
		}

		bool Global_InstantMessage(uintptr_t thisptr, unsigned __int64 sender_id, const char* sender_name, uintptr_t msg, unsigned int size)
		{
			return false; // Block all instant messages from this source
		}

		__int64 hkdwInstantDispatchMessage(__int64 senderXuid, unsigned int controllerIndex, const char* message, unsigned int messageSize)
		{
			msg_t msg{};
			MSG_InitReadOnly(&msg, message, messageSize);
			MSG_BeginReading(&msg);

			bool is_valid_packet = true;
			BYTE packetType = 0;

			if (MSG_ReadByte(&msg) == '1')
			{
				packetType = MSG_ReadByte(&msg);
			}

			// check packet for the following cases:
			if (packetType == 0x65 || packetType == 0x6D) // cbuf
			{
				is_valid_packet = false;
			}
			else if (packetType == 0x66) // is a joinRequest
			{
				// These three are read from the raw payload as if it began with a msg_t, as upstream
				// reads them: they are bytes the peer chose, msgData included. The test is kept, but
				// nothing is read unless it lies inside the payload that arrived. Upstream instead
				// dereferenced msgData + msgRead wherever it pointed - a remote crash, or a read of
				// our memory - and read the header past the end of a short payload. A payload whose
				// bytes point inside itself needs the receive buffer's address, which a peer does
				// not have, so in practice every 0x66 here is dropped, as ReadP2PPacket already
				// drops type 102 (0x66) on the Steam route.
				if (messageSize < sizeof(msg_t))
				{
					is_valid_packet = false;
				}
				else
				{
					char* msgData = *(char**)(message + 8);
					__int32 msgSize = *(__int32*)(message + 0x1C);
					__int32 msgRead = *(__int32*)(message + 0x24);

					if ((msgSize - msgRead) != 0x64) // has a bad message size which leads to error message
					{
						is_valid_packet = false;
					}

					const uintptr_t payloadBegin = (uintptr_t)message;
					const uintptr_t payloadEnd = payloadBegin + messageSize;
					const uintptr_t packetData = (uintptr_t)msgData + (uintptr_t)(intptr_t)msgRead;
					if (is_valid_packet && (packetData < payloadBegin || packetData > payloadEnd - sizeof(__int32)))
					{
						is_valid_packet = false;
					}

					if (is_valid_packet && !*(__int32*)packetData) // msg_type_join_request which leads to crash
					{
						is_valid_packet = false;
					}
				}
			}

			if (is_valid_packet && (packetType == 0x68) && Protection::CheckPendingInfoRequests(senderXuid, message, messageSize))
			{
				return 0;
			}

			if (is_valid_packet)
			{
				if (const auto size{ msg.cursize - msg.readcount }; size < 2048u)
				{
					char data[2048] = { 0 };
					MSG_ReadData(&msg, data, size);
					return LobbyMsg_HandleIM(0, senderXuid, data, size);
				}
				
			}

			return 0;
		}

		__int64 __fastcall hkLivePresence_Serialize(__int64 a1, __int64 a2)
		{
			if (!a1 || !a2)
				return 0;

			int* packedPtr = *reinterpret_cast<int**>(a1 + 16);

			if (!packedPtr)
				return 0;

			int packed = *packedPtr;
			int count = (packed >> 2) & 0x1F;

			constexpr int maxPlayers = 18;

			if (count > maxPlayers)
			{
				int sanitized = packed;

				sanitized &= ~(0x1F << 2);
				sanitized |= (maxPlayers & 0x1F) << 2;

				*packedPtr = sanitized;
			}

			return LivePresence_Serialize(a1, a2);
		}

		bool hkLobbyMsgRW_PrepWriteMsg(__int64 lobbyMsg, __int64 data, int length, int msgType)
		{
			if (LobbyMsgRW_PrepWriteMsg(lobbyMsg, data, length, msgType))
			{
				// ALOG("sending pkt %d", msgType);
				if (ZBR_PREFIX_BYTE)
				{
					((void(__fastcall*)(__int64, unsigned char))PTR_MSG_WriteByte)(lobbyMsg, ZBR_PREFIX_BYTE);
					((void(__fastcall*)(__int64, unsigned char))PTR_MSG_WriteByte)(lobbyMsg, ZBR_PREFIX_BYTE2);
				}
				return true;
			}
			return false;
		}

		bool hkLobbyMsgRW_PrepReadMsg(__int64 lm)
		{
			if (!LobbyMsgRW_PrepReadMsg(lm))
			{
				return false;
			}

			if (!ZBR_PREFIX_BYTE)
			{
				return true;
			}

			const auto prefix1 = ((unsigned char(__fastcall*)(__int64))PTR_MSG_ReadByte)(lm);
			const auto prefix2 = ((unsigned char(__fastcall*)(__int64))PTR_MSG_ReadByte)(lm);
			const bool matches = prefix1 == ZBR_PREFIX_BYTE && prefix2 == ZBR_PREFIX_BYTE2;
			if (!matches)
			{
				// Fires per rejected packet: with debuglog on, a flood of these is a frame-time cost.
				// No bytes logged: both prefixes are 16 bits of a password hash, and trace logs get
				// posted in public bug reports.
				ZLOG("prepReadMsg: REJECTED lobby message (password prefix mismatch)");
			}
			return matches;
		}

		bool hkLobbyMsgRW_PackageInt(LobbyMsg* lobbyMsg, const char* key, __int32* val)
		{
			
			bool result = LobbyMsgRW_PackageInt(lobbyMsg, key, val);

			if (result && (!Protection::I_stricmp(key, "lobbytype") || !Protection::I_stricmp(key, "srclobbytype") || !Protection::I_stricmp(key, "destlobbytype")))
			{

				if (*val < LOBBY_TYPE_FIRST || *val > LOBBY_TYPE_LAST)
				{
					//XLOG("DROP LOBBYTYPE");
					return false;
				}
			}
			return result;
		}

		bool hkLobbyMsgRW_PackageUInt(LobbyMsg* lobbyMsg, const char* key, unsigned __int32* val)
		{
			bool result = LobbyMsgRW_PackageUInt(lobbyMsg, key, val);
			if (result && (!Protection::I_stricmp(key, "lobbytype") || !Protection::I_stricmp(key, "srclobbytype") || !Protection::I_stricmp(key, "destlobbytype")))
			{
				if (*val > static_cast<unsigned __int32>(LOBBY_TYPE_LAST))
				{
					return false;
				}
			}
			if (result && (!Protection::I_stricmp(key, "datamask")))
			{
				if (*val == 0)
				{
					return false;
				}
				__int32 state = *(__int32*)REBASE(0x1686E874);
				state = state << 28 >> 28;
				if (state)
				{
					return result;
				}
				return !(*val & 512);
			}
			return result;
		}

		bool hkLobbyMsgRW_PackageUChar(LobbyMsg* lobbyMsg, const char* key, unsigned char* val)
		{
			bool result = LobbyMsgRW_PackageUChar(lobbyMsg, key, val);
			if (result && !Protection::I_stricmp(key, "nattype"))
			{
				if (*val > 4) // 4 is "unknown" which is the highest valid nat type
				{
					*val = 4;
				}
			}
			return result;
		}

		void hkCMD_MenuReponse_f(char* ent)
		{
			char mres[1024]{};
			char szMenuName[1024]{};

			// cant call original cause arxan pointer decryption routines, but its fine we can just recreate and omit some other useless stuff while we are at it.
			int nesting = *(__int32*)REBASE(0x1681BEB0);
			int num_args = ((__int32*)REBASE(0x1681BF14))[nesting];
			int menuIndex = 0;

			if (num_args != 4)
			{
				strcpy_s(mres, "bad");
			}
			else
			{
				SV_Cmd_ArgvBuffer(1, szMenuName, 1024);

				auto svId = atoi(szMenuName);

				if (svId != *(__int32*)REBASE(0x17679580))
				{
					return; // unamused
				}

				SV_Cmd_ArgvBuffer(2, szMenuName, 1024);

				menuIndex = atoi(szMenuName);

				if ((unsigned __int32)menuIndex > 0x3Fu)
				{
					szMenuName[0] = 0;
				}
				else
				{
					// Truncating, null-safe copy: strcpy_s on an over-long or null name is the
					// invalid-parameter handler, a fast-fail past our exception handler.
					const char* menuName = BG_Cache_GetScriptMenuNameForIndex(0, menuIndex);
					strncpy_s(szMenuName, sizeof(szMenuName), menuName ? menuName : "", _TRUNCATE);
				}

				SV_Cmd_ArgvBuffer(3, mres, 1024);
			}

			if (!Protection::I_stricmp(mres, "badspawn"))
			{
				return;
			}

			auto mode = Com_SessionMode_GetModeName();

			bool isCampaign = mode && !Protection::I_stricmp(mode, "CP");

			if (*(__int32*)ent) // not host
			{
				bool killServer = !Protection::I_stricmp(mres, "killserverpc");
				bool endGame = !Protection::I_stricmp(mres, "endgame");
				bool endRound = !Protection::I_stricmp(mres, "endround");
				bool restartLevelZM = !Protection::I_stricmp(mres, "restart_level_zm");

				if (killServer || restartLevelZM ||	(!isCampaign && (endGame || endRound)))
				{
					// someone who is not host tried to end the game
					snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent);
					Protection::Cbuf_AddText(0, mres, 0);
					return;
				}
			}

			bool killServer = !Protection::I_stricmp(mres, "killserverpc");
			bool endGame = !Protection::I_stricmp(mres, "endgame");
			bool endRound =	!Protection::I_stricmp(mres, "endround");
			bool campaignTransition = isCampaign && (endGame || endRound);

			if ((!killServer && !endGame && !endRound) || campaignTransition ||	LobbyHost_IsHost(1u))
			{
				Scr_AddString(0, mres);
				Scr_AddString(0, szMenuName);
				__int32 thread = Scr_ExecEntThread(ent, (void*)*(__int64*)REBASE(0xA5A6810), 2);
				Scr_FreeThread(0, thread);
			}
			else
			{
				// someone who is not host tried to end the game
				snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent);
				Protection::Cbuf_AddText(0, mres, 0);
				return;
			}
		}

		void hk_CMD_MenuResponseCached_f(char* ent)
		{
			char mres[1024]{};
			char szMenuName[1024]{};

			// cant call original cause arxan pointer decryption routines, but its fine we can just recreate and omit some other useless stuff while we are at it.
			int nesting = *(__int32*)REBASE(0x1681BEB0);
			int num_args = ((__int32*)REBASE(0x1681BF14))[nesting];
			int menuIndex = 0;

			if (num_args != 4)
			{
				strcpy_s(mres, "bad");
			}
			else
			{
				SV_Cmd_ArgvBuffer(1, szMenuName, 1024);
				auto svId = atoi(szMenuName);
				if (svId != *(__int32*)REBASE(0x17679580))
				{
					return; // unamused
				}
				SV_Cmd_ArgvBuffer(2, szMenuName, 1024);
				menuIndex = atoi(szMenuName);
				if ((unsigned __int32)menuIndex > 0x3Fu)
				{
					szMenuName[0] = 0;
				}
				else
				{
					// Truncating, null-safe copy: strcpy_s on an over-long or null name is the
					// invalid-parameter handler, a fast-fail past our exception handler.
					const char* menuName = BG_Cache_GetScriptMenuNameForIndex(0, menuIndex);
					strncpy_s(szMenuName, sizeof(szMenuName), menuName ? menuName : "", _TRUNCATE);
				}
				SV_Cmd_ArgvBuffer(3, mres, 1024);
				auto eventIndex = (unsigned int)atoi(mres);
				// eventIndex is a number a client sent us, and nothing checked it before it indexed
				// the event-string table. 256 is the bound upstream's own (disabled)
				// hkBG_Cache_GetEventStringNameForIndex uses; "bad" is the sentinel this function
				// already substitutes for unusable input.
				if (eventIndex >= 256u)
				{
					strcpy_s(mres, "bad");
				}
				else
				{
					const char* eventName = BG_Cache_GetEventStringNameForIndex(0, eventIndex);
					strncpy_s(mres, sizeof(mres), eventName ? eventName : "", _TRUNCATE);
				}
			}

			if (!Protection::I_stricmp(mres, "badspawn"))
			{
				return;
			}

			auto mode = Com_SessionMode_GetModeName();
			bool isCampaign = mode && !Protection::I_stricmp(mode, "CP");

			if (*(__int32*)ent) // not host
			{
				bool killServer = !Protection::I_stricmp(mres, "killserverpc");
				bool endGame = !Protection::I_stricmp(mres, "endgame");
				bool endRound = !Protection::I_stricmp(mres, "endround");
				bool restartLevelZM = !Protection::I_stricmp(mres, "restart_level_zm");

				if (killServer || restartLevelZM || (!isCampaign && (endGame || endRound)))
				{
					snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent);
					Protection::Cbuf_AddText(0, mres, 0);
					return;
				}
			}

			bool killServer = !Protection::I_stricmp(mres, "killserverpc");
			bool endGame = !Protection::I_stricmp(mres, "endgame");
			bool endRound = !Protection::I_stricmp(mres, "endround");

			bool campaignTransition =isCampaign && (endGame || endRound);

			if ((!killServer && !endGame && !endRound) || campaignTransition ||	LobbyHost_IsHost(1u))
			{
				Scr_AddString(0, mres);
				Scr_AddString(0, szMenuName);

				__int32 thread = Scr_ExecEntThread(ent, (void*)*(__int64*)REBASE(0xA5A6810), 2);

				Scr_FreeThread(0, thread);
			}
			else
			{
				snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent);

				Protection::Cbuf_AddText(0, mres, 0);
				return;
			}
		}

		__int32 hkUI_Model_GetModelFromPath_0(__int64 parentNodeIndex, const char* path)
		{
			if (!path)
			{
				return 0;
			}
			int len = strlen(path);
			int keySize = 0;
			for (int i = 0; i < len; i++)
			{
				if (path[i] != '.')
				{
					keySize++;
				}
				else
				{
					if (keySize >= 64)
					{
						return 0;
					}
					keySize = 0;
				}
			}
			if (keySize >= 64)
			{
				return 0;
			}
			return UI_Model_GetModelFromPath_0(parentNodeIndex, path);
		}

		__int32 hkUI_Model_GetModelFromPath(__int64 parentNodeIndex, const char* path)
		{
			if (!path)
			{
				return 0;
			}
			int len = strlen(path);
			int keySize = 0;
			for (int i = 0; i < len; i++)
			{
				if (path[i] != '.')
				{
					keySize++;
				}
				else
				{
					if (keySize >= 64)
					{
						return 0;
					}
					keySize = 0;
				}
			}
			if (keySize >= 64)
			{
				return 0;
			}
			return UI_Model_GetModelFromPath(parentNodeIndex, path);
		}

		__int32 hkUI_Model_CreateModelFromPath(__int64 parentNodeIndex, const char* path)
		{
			if (!path)
			{
				return 0;
			}
			int len = strlen(path);
			int keySize = 0;
			for (int i = 0; i < len; i++)
			{
				if (path[i] != '.')
				{
					keySize++;
				}
				else
				{
					if (keySize >= 64)
					{
						return 0;
					}
					keySize = 0;
				}
			}
			if (keySize >= 64)
			{
				return 0;
			}
			return UI_Model_CreateModelFromPath(parentNodeIndex, path);
		}

		__int32 hkUI_Model_AllocateNode(__int32 ancestorIndex, const char* path, bool persistent)
		{
			if (!path)
			{
				return 0;
			}
			if (!*(__int16*)REBASE(0x16293150)) // out of model paths to use (TODO: should we instead back this off and restart the UI?)
			{
				return 0;
			}
			if (strlen(path) >= 64)
			{
				return 0;
			}
			return UI_Model_AllocateNode(ancestorIndex, path, persistent);
		}

		// From Scropts-QOL
		bool hkMods_IsModsLoaded()
		{

			#if SPOOF_RANKED

				void* ret = _ReturnAddress();
				uintptr_t callsite = GetCallsiteFromReturn(ret);
				uintptr_t relativeCall = bo3::february_rva(callsite);

				for (auto addr : isModLoadedAddrs)
				{
					if (relativeCall == addr)
					{
						return false;
					}
				}

			#endif

			return Mods_IsModsLoaded();
		}

		bool hkMods_IsModsLoaded_1() 
		{
			#if SPOOF_RANKED
				return false;
			#endif

			return Mods_IsModsLoaded_1();
		}

		bool hkMods_SubscribeUGC(__int64 a1)
		{
			if (!Protection::IsAutoInstallModsEnabled)
			{
				return false;
			}

			return Mods_SubscribeUGC(a1);
		}

	}

	void ApplyVMTHooks()
	{

		auto ptr = *(uintptr_t*)DW_LOBBY;
		auto vmt = **(uintptr_t***)(ptr + 1384);

		auto vtable_buf = new uintptr_t[50];
		for (auto count = 0; count < 50; ++count) {
			vtable_buf[count] = vmt[count];
		}

		**(uintptr_t**)(ptr + 1384) = (uintptr_t)vtable_buf;
		*(uintptr_t*)(**(uintptr_t**)(ptr + 1384) + 192) = (uintptr_t)functions::Global_InstantMessage;

	}

	void ApplyMemoryPatches()
	{
		auto OldProtection = 0ul;
		// SL fix (nop out the calls to com_error)
		VirtualProtect((__int32*)REBASE(0x1964766), 5, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(__int32*)REBASE(0x1964766) = 0x90909090;
		*(char*)(REBASE(0x1964766) + 4) = 0x90;
		VirtualProtect((__int32*)REBASE(0x1964766), 5, OldProtection, &OldProtection);

		// SL fix (nop out the calls to com_error)
		VirtualProtect((__int32*)REBASE(0x19646A7), 5, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(__int32*)REBASE(0x19646A7) = 0x90909090;
		*(char*)(REBASE(0x19646A7) + 4) = 0x90;
		VirtualProtect((__int32*)REBASE(0x19646A7), 5, OldProtection, &OldProtection);

		VirtualProtect((__int32*)REBASE(0x1EEACF2), 4, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(char*)REBASE(0x1EEACF2 + 1) = 0xB7; // fix movsx issue with package readglob
		VirtualProtect((__int32*)REBASE(0x1EEACF2), 4, OldProtection, &OldProtection);

		VirtualProtect((__int32*)PTR_Cmp_TokenizeStringInternal, 1, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(char*)PTR_Cmp_TokenizeStringInternal = 0x2F;
		VirtualProtect((__int32*)PTR_Cmp_TokenizeStringInternal, 1, OldProtection, &OldProtection);

		// process priority
		VirtualProtect((__int32*)REBASE(0x22BC1CD), 4, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(__int32*)REBASE(0x22BC1CD) = 0x00008000; // above normal
		VirtualProtect((__int32*)REBASE(0x22BC1CD), 4, OldProtection, &OldProtection);

		VirtualProtect((__int32*)REBASE(0x22BC1D2), 4, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(__int32*)REBASE(0x22BC1D2) = 0x00008000; // above normal
		VirtualProtect((__int32*)REBASE(0x22BC1D2), 4, OldProtection, &OldProtection);

		// movsx -> movzx
		VirtualProtect((__int32*)REBASE(0x1F21EF8), 1, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(char*)REBASE(0x1F21EF8) = 0xb6;
		VirtualProtect((__int32*)REBASE(0x1F21EF8), 1, OldProtection, &OldProtection);

		// movsx -> movzx
		VirtualProtect((__int32*)REBASE(0x1CA4C84), 1, PAGE_EXECUTE_READWRITE, &OldProtection);
		*(char*)REBASE(0x1CA4C84) = 0xb6;
		VirtualProtect((__int32*)REBASE(0x1CA4C84), 1, OldProtection, &OldProtection);
	}

	void ApplyHooks()
	{
		campaign_fixes::InstallHooks();
		lua_symbols::InstallHooks();
		CREATE_HOOK((LPVOID)REBASE(0x1EEA560), (LPVOID)functions::hkLobbyMsgRW_PrepWriteMsg, (LPVOID*)&LobbyMsgRW_PrepWriteMsg);
		CREATE_HOOK((LPVOID)REBASE(0x1EEB8D0), (LPVOID)functions::hkLobbyMsgRW_PrepReadMsg, (LPVOID*)&LobbyMsgRW_PrepReadMsg);
		CREATE_HOOK((LPVOID)REBASE(0x20E31C0), (LPVOID)functions::hkCOD_GetBuildTitle, (LPVOID*)&COD_GetBuildTitle);
		CREATE_HOOK((LPVOID)REBASE(0x1E016C0), (LPVOID)functions::hkLive_SystemInfo, (LPVOID*)&Live_SystemInfo);
		CREATE_HOOK((LPVOID)REBASE(0x1EDFA60), (LPVOID)functions::hkLobbyTypes_GetMsgTypeName, (LPVOID*)&LobbyTypes_GetMsgTypeName);
		CREATE_HOOK((LPVOID)REBASE(0x2BC4EB0), (LPVOID)functions::hkqmemcpy, (LPVOID*)&qmemcpy);
		CREATE_HOOK((LPVOID)REBASE(0x22C9650), (LPVOID)functions::hkflsomeWeirdCharacterIndex, (LPVOID*)&flsomeWeirdCharacterIndex);
		CREATE_HOOK((LPVOID)REBASE(0x221CE90), (LPVOID)functions::hkSEH_ReplaceDirectiveInStringWithBinding, (LPVOID*)&SEH_ReplaceDirectiveInStringWithBinding);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA3E0), (LPVOID)functions::hkLobbyMsgRW_PackageInt, (LPVOID*)&LobbyMsgRW_PackageInt);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA490), (LPVOID)functions::hkLobbyMsgRW_PackageUInt, (LPVOID*)&LobbyMsgRW_PackageUInt);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA450), (LPVOID)functions::hkLobbyMsgRW_PackageUChar, (LPVOID*)&LobbyMsgRW_PackageUChar);
		CREATE_HOOK((LPVOID)REBASE(0x1F27400), (LPVOID)functions::hkUI_DoModelStringReplacement, (LPVOID*)&UI_DoModelStringReplacement);
		CREATE_HOOK((LPVOID)REBASE(0x1EA9C80), (LPVOID)functions::hkLiveSteam_InitServer, (LPVOID*)&LiveSteam_InitServer);
		CREATE_HOOK((LPVOID)REBASE(0x195F100), (LPVOID)functions::hkCMD_MenuReponse_f, (LPVOID*)&CMD_MenuResponse_f);
		CREATE_HOOK((LPVOID)REBASE(0x195EFA0), (LPVOID)functions::hk_CMD_MenuResponseCached_f, (LPVOID*)&CMD_MenuResponseCached_f);
		CREATE_HOOK((LPVOID)REBASE(0x200CF00), (LPVOID)functions::hkUI_Model_GetModelFromPath_0, (LPVOID*)&UI_Model_GetModelFromPath_0);
		CREATE_HOOK((LPVOID)REBASE(0x200D5B0), (LPVOID)functions::hkUI_Model_GetModelFromPath, (LPVOID*)&UI_Model_GetModelFromPath);
		CREATE_HOOK((LPVOID)REBASE(0x200CFC0), (LPVOID)functions::hkUI_Model_CreateModelFromPath, (LPVOID*)&UI_Model_CreateModelFromPath);
		CREATE_HOOK((LPVOID)REBASE(0x200CD00), (LPVOID)functions::hkUI_Model_AllocateNode, (LPVOID*)&UI_Model_AllocateNode);
		CREATE_HOOK((LPVOID)REBASE(0x211F5A0), (LPVOID)functions::hkSys_VerifyPacketChecksum, (LPVOID*)&Sys_VerifyPacketChecksum);
		CREATE_HOOK((LPVOID)REBASE(0x211F500), (LPVOID)functions::hkSys_ChecksumCopy, (LPVOID*)&Sys_ChecksumCopy);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA940), (LPVOID)functions::hkLobbyMsgRW_PrintMessage, (LPVOID*)&LobbyMsgRW_PrintMessage);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA680), (LPVOID)functions::hkLobbyMsgRW_PrintDebugMessage, (LPVOID*)&LobbyMsgRW_PrintDebugMessage);
		CREATE_HOOK((LPVOID)REBASE(0x1EF8A60), (LPVOID)functions::hkExecLuaCMD, (LPVOID*)&ExecLuaCMD);
		CREATE_HOOK((LPVOID)REBASE(0x1EBB200), (LPVOID)functions::hkLive_UserGetName, (LPVOID*)&Live_UserGetName);
		CREATE_HOOK((LPVOID)&__report_gsfailure, (LPVOID)functions::__report_gsfailure_hook, (LPVOID*)&pOriginalGSFailure);
		CREATE_HOOK((LPVOID)REBASE(0x143A620), (LPVOID)functions::hkdwInstantDispatchMessage, (LPVOID*)&dwInstantDispatchMessage);
		CREATE_HOOK((LPVOID)REBASE(0x134CD70), (LPVOID)functions::hkCL_ConnectionlessCMD, (LPVOID*)&CL_ConnectionlessCMD);
		CREATE_HOOK((LPVOID)REBASE(0x1E85450), (LPVOID)functions::hkLivePresence_Serialize, (LPVOID*)&LivePresence_Serialize);
		CREATE_HOOK((LPVOID)REBASE(0x1321130), (LPVOID)functions::hkCL_GetConfigString, (LPVOID*)&CL_GetConfigString);
		CREATE_HOOK((LPVOID)REBASE(0x20CB0C0), (LPVOID)functions::hkMods_SubscribeUGC, (LPVOID*)&Mods_SubscribeUGC);
		CREATE_HOOK((LPVOID)REBASE(0x1E19B30), (LPVOID)functions::hkLiveInvites_AcceptInvite, (LPVOID*)&LiveInvites_AcceptInvite);
		CREATE_HOOK((LPVOID)REBASE(0x1E724A0), (LPVOID)functions::hkLiveInvites_SendJoinInfo, (LPVOID*)&LiveInvites_SendJoinInfo);
		CREATE_HOOK((LPVOID)REBASE(0x1E72040), (LPVOID)functions::hkLiveInvites_JoinMessageAction, (LPVOID*)&LiveInvites_JoinMessageAction);
		CREATE_HOOK((LPVOID)REBASE(0x1EA4E30), (LPVOID)functions::hkUI_BrowserOpen, (LPVOID*)&UI_BrowserOpen);
		/*MH_CreateHook((LPVOID)REBASE(0xA7DE0), (LPVOID)functions::hkBG_Cache_GetScriptMenuNameForIndex, (LPVOID*)&BG_Cache_GetScriptMenuNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA78A0), (LPVOID)functions::hkBG_Cache_GetEventStringNameForIndex, (LPVOID*)&BG_Cache_GetEventStringNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7AB0), (LPVOID)functions::hkBG_Cache_GetLocStringNameForIndex, (LPVOID*)&BG_Cache_GetLocStringNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7A00), (LPVOID)functions::hkBG_Cache_GetLUIMenuForIndex, (LPVOID*)&BG_Cache_GetLUIMenuForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7990), (LPVOID)functions::hkBG_Cache_GetLUIMenuDataForIndex, (LPVOID*)&BG_Cache_GetLUIMenuDataForIndex);*/
		CREATE_HOOK((LPVOID)REBASE(0x1EAAD60), (LPVOID)functions::hkUserHasLicenseForApp, (LPVOID*)&UserHasLicenseForApp);
		CREATE_HOOK((LPVOID)REBASE(0x1DFCC60), (LPVOID)functions::hkLiveInventory_GetItemQuantity, (LPVOID*)&LiveInventory_GetItemQuantity);
		CREATE_HOOK((LPVOID)REBASE(0x1E06110), (LPVOID)functions::hkLiveEntitlements_IsEntitlementActiveForController, (LPVOID*)&LiveEntitlements_IsEntitlementActiveForController);
		CREATE_HOOK((LPVOID)REBASE(0x1DFC580), (LPVOID)functions::hkLiveInventory_AreExtraSlotsPurchased, (LPVOID*)&LiveInventory_AreExtraSlotsPurchased);
		CREATE_HOOK((LPVOID)REBASE(0x1DFDFE0), (LPVOID)functions::hkLiveInventory_IsValid, (LPVOID*)&LiveInventory_IsValid);
		CREATE_HOOK((LPVOID)REBASE(0x227BDA0), (LPVOID)functions::hkInfo_ValueForKey, (LPVOID*)&Info_ValueForKey);
		CREATE_HOOK((LPVOID)REBASE(0x20C8F60), (LPVOID)functions::hkMods_IsModsLoaded, (LPVOID*)&Mods_IsModsLoaded);
		CREATE_HOOK((LPVOID)REBASE(0x20C9AE0), (LPVOID)functions::hkMods_IsModsLoaded_1, (LPVOID*)&Mods_IsModsLoaded_1);

		const MH_STATUS enableStatus = MH_EnableHook(MH_ALL_HOOKS);
		if (enableStatus != MH_OK)
		{
			ZLOG("hook: MH_EnableHook FAILED (MH_STATUS=%d)", (int)enableStatus);
		}
	}

	void DestroyHooks()
	{
		MH_DisableHook(MH_ALL_HOOKS);
	}

}
