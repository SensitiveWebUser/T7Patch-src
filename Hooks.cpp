#include "Hooks.h"

const char zbr_window_text[] = ZBR_WINDOW_TEXT;
void* pOriginalGSFailure = nullptr;
const char* bad_str = "bad";

namespace hooks {

	namespace functions
	{

		__int64 hkLiveInventory_GetItemQuantity(ControllerIndex_t controllerIndex, int itemId) {

			if (SPOOF_UNLOCK_ALL == true) return 999;

			return LiveInventory_GetItemQuantity(controllerIndex, itemId);
		}

		bool hkLiveInventory_AreExtraSlotsPurchased(ControllerIndex_t controllerIndex) {

			// Always true, matching shipped 3.03+ behaviour: a stray `return true` already made the
			// call to the original unreachable. Do not "restore" that call.
			return true;
		}

		const char* hkInfo_ValueForKey(char* a1, __int64 a2)
		{
			// log a2


			return Info_ValueForKey(a1, a2);
		}

		bool hkLiveInventory_IsValid(ControllerIndex_t controllerIndex) {

			return true;
		}

		bool hkLiveEntitlements_IsEntitlementActiveForController(ControllerIndex_t controllerIndex, int incentiveId) {

			return true;
		}


		char hkUserHasLicenseForApp(__int64 mapInfo, __int64* userObj) {
			if (SPOOF_UNLOCK_ALL == true)
			{
				*((BYTE*)userObj + 13) = 1;
				*((BYTE*)userObj + 12) |= 0;
				*((DWORD*)userObj + 4) |= 8u;

				return 1;
			}

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
					ZLOG("invite: DROP non-friend xuid=%p", (void*)recepientXUID);
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
					ZLOG("invite: DROP non-friend xuid=%p", (void*)recepientXUID);
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

			if (!buf || bufSize <= 0)
			{
				return 0;
			}

			Memset(buf, 0LL, bufSize);
			Live_Base_UserGetName((UINT8*)buf, bufSize, 1);

			return 1;
		}

		float hkflsomeWeirdCharacterIndex(__int64 a1, int a2, int a3) {

			if (!a1 || a2 < 0) {
				return -1.0f;
			}

			auto slot = (DWORD64*)(a1 + 24LL * a2 + 56);
			if (Protection::IsBadReadPtr(slot) || Protection::IsBadReadPtr((char*)slot + 7)) {
				return -1.0f; // matches this hook's own fallthrough below
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

			if (!message)
			{
				return 1;
			}

			// From v2.04
			auto mode = Com_SessionMode_GetModeName();

			// mode can be null, and testing it first short-circuits before the message comparisons.
			if (mode && !Protection::I_stricmp(mode, "CP")
				&& (!Protection::I_stricmp(message, "requeststats") || !Protection::I_stricmp(message, "requeststats\n")))
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
			// static: this hook is on a per-frame path, so the list must not be rebuilt per call.
			static const std::vector<int> config_strings = { 3514, 3627 };

			// Campaign bypass from 859c4aa, left as written. Do not narrow it: the store below is
			// all this hook does and it is skipped in CP either way, so there is nothing to gain.
			auto mode = Com_SessionMode_GetModeName();
			if (mode && !Protection::I_stricmp(mode, "CP"))
			{
				return CL_GetConfigString(configStringIndex);
			}

			if (is_in_number_array(configStringIndex, config_strings))
			{
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

			// Returning 0 for an over-long string is shipped behaviour; leave it alone. No caller treats
			// the null return as an error, so "fixing" it to truncate would feed the game a string it
			// currently never sees.
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

			char input[4096]{};
			strncpy_s(input, source ? source : "", _TRUNCATE);
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
					if (ZBR_HAS_PREV_PASSWORD
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
				is_valid_packet = false;
			}

			ZLOG("dispatch: type=0x%02X valid=%d xuid=%p size=%u",
				packetType, (int)is_valid_packet, (void*)senderXuid, messageSize);
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

				if (ZBR_HAS_PASSWORD)
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
				return true;
			}

			// No password means no prefix was written, so there is nothing to check.
			if (!ZBR_HAS_PASSWORD)
			{
				return true;
			}

			const unsigned char wirePrefix1 = ((unsigned char(__fastcall*)(__int64))PTR_MSG_ReadByte)(lm);
			const unsigned char wirePrefix2 = ((unsigned char(__fastcall*)(__int64))PTR_MSG_ReadByte)(lm);

			if (wirePrefix1 == ZBR_PREFIX_BYTE && wirePrefix2 == ZBR_PREFIX_BYTE2)
			{
				if (g_trace_enabled.load(std::memory_order_relaxed))
				{
					static std::atomic<unsigned int> lastReported{ 0xFFFFFFFFu };
					const unsigned int prefixPair =
						((unsigned int)ZBR_PREFIX_BYTE << 8) | (unsigned int)ZBR_PREFIX_BYTE2;
					if (lastReported.exchange(prefixPair, std::memory_order_relaxed) != prefixPair)
					{
						ZLOG("prepReadMsg: ACCEPTED peer message (prefix=%02X,%02X)",
							ZBR_PREFIX_BYTE, ZBR_PREFIX_BYTE2);
					}
				}
				return true;
			}

			// Grace window, as in hkSys_VerifyPacketChecksum: peers rotate the password at
			// unsynchronised moments, so briefly accept the previous one. PrivatePassword[0] holds
			// it, [2] is the rotation time.
			const unsigned char prevPrefix1 = (unsigned char)((Protection::PrivatePassword[0] & 0xFF0000) >> 16);
			const unsigned char prevPrefix2 = (unsigned char)((Protection::PrivatePassword[0] & 0xFF000000) >> 24);

			if (ZBR_HAS_PREV_PASSWORD
				&& GetTickCount64() <= (unsigned __int64)Protection::PrivatePassword[2] + 1500
				&& wirePrefix1 == prevPrefix1 && wirePrefix2 == prevPrefix2)
			{
				return true;
			}

			msg_t* msg = (msg_t*)lm;
			msg->readcount = msg->cursize;

			ZLOG("prepReadMsg: DROPPED wire=%02X,%02X expected=%02X,%02X",
				wirePrefix1, wirePrefix2, ZBR_PREFIX_BYTE, ZBR_PREFIX_BYTE2);
			return true;
		}

		bool hkLobbyMsgRW_PackageInt(LobbyMsg* lobbyMsg, const char* key, __int32* val)
		{
			
			bool result = LobbyMsgRW_PackageInt(lobbyMsg, key, val);

			if (result && (!Protection::I_stricmp(key, "lobbytype") || !Protection::I_stricmp(key, "srclobbytype") || !Protection::I_stricmp(key, "destlobbytype")))
			{
				if (*val < 0 || *val > 1)
				{
					return false;
				}
			}

			// The inspectors are best-effort reimplementations of the game's parsers: a failure
			// there means "could not model this message", not "malformed packet", and propagating
			// it drops legitimate join traffic. The game's own calls get the real result.
			if (Protection::InspectorDepth > 0)
			{
				return true;
			}

			return result;
		}

		bool hkLobbyMsgRW_PackageUInt(LobbyMsg* lobbyMsg, const char* key, unsigned __int32* val)
		{
			bool result = LobbyMsgRW_PackageUInt(lobbyMsg, key, val);
			if (result && (!Protection::I_stricmp(key, "lobbytype") || !Protection::I_stricmp(key, "srclobbytype") || !Protection::I_stricmp(key, "destlobbytype")))
			{
				if (*val > 1)
				{
					return false;
				}
			}
			if (result && (!Protection::I_stricmp(key, "datamask")))
			{
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
			int nesting = *(__int32*)REBASE(0x1681BEB0); // REBASE(0x1689AE94)
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
					const char* menuName = BG_Cache_GetScriptMenuNameForIndex(0, menuIndex);
					strncpy_s(szMenuName, menuName ? menuName : "", _TRUNCATE);
				}
				SV_Cmd_ArgvBuffer(3, mres, 1024);
			}

			if (!Protection::I_stricmp(mres, "badspawn"))
			{
				return;
			}

			if (*(__int32*)ent) // not host
			{
				if (!(Protection::I_stricmp(mres, "killserverpc") && Protection::I_stricmp(mres, "endgame") && Protection::I_stricmp(mres, "endround") && Protection::I_stricmp(mres, "restart_level_zm")))
				{
					// someone who is not host tried to end the game
					snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent); // kick them
					Protection::Cbuf_AddText(0, mres, 0);
					return;
				}
			}

			if (Protection::I_stricmp(mres, "killserverpc") && Protection::I_stricmp(mres, "endgame") && Protection::I_stricmp(mres, "endround")
				|| LobbyHost_IsHost(1u))
			{
				Scr_AddString(0, mres);
				Scr_AddString(0, szMenuName);
				__int32 thread = Scr_ExecEntThread(ent, (void*)*(__int64*)REBASE(0xA5A6810), 2);
				Scr_FreeThread(0, thread);
			}
			else
			{
				// someone who is not host tried to end the game
				snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent); // kick them
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
					const char* menuName = BG_Cache_GetScriptMenuNameForIndex(0, menuIndex);
					strncpy_s(szMenuName, menuName ? menuName : "", _TRUNCATE);
				}
				SV_Cmd_ArgvBuffer(3, mres, 1024);
				auto eventIndex = (unsigned int)atoi(mres);
				// eventIndex comes off the wire and the table has 256 entries; "bad" is the
				// sentinel this function already uses for unusable input.
				if (eventIndex >= 256u)
				{
					strcpy_s(mres, "bad");
				}
				else
				{
					const char* eventName = BG_Cache_GetEventStringNameForIndex(0, eventIndex);
					strncpy_s(mres, eventName ? eventName : "", _TRUNCATE);
				}
			}

			if (!Protection::I_stricmp(mres, "badspawn"))
			{
				return;
			}

			if (*(__int32*)ent) // not host
			{
				if (!(Protection::I_stricmp(mres, "killserverpc") && Protection::I_stricmp(mres, "endgame") && Protection::I_stricmp(mres, "endround") && Protection::I_stricmp(mres, "restart_level_zm")))
				{
					// someone who is not host tried to end the game
					snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent); // kick them
					Protection::Cbuf_AddText(0, mres, 0);
					return;
				}
			}

			if (Protection::I_stricmp(mres, "killserverpc") && Protection::I_stricmp(mres, "endgame") && Protection::I_stricmp(mres, "endround")
				|| LobbyHost_IsHost(1u))
			{
				Scr_AddString(0, mres);
				Scr_AddString(0, szMenuName);
				__int32 thread = Scr_ExecEntThread(ent, (void*)*(__int64*)REBASE(0xA5A6810), 2);
				Scr_FreeThread(0, thread);
			}
			else
			{
				// someone who is not host tried to end the game
				snprintf(mres, 1024, "tempBanClient %i\n", *(__int32*)ent); // kick them
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

		bool hkMods_SubscribeUGC(__int64 a1)
		{			
			return false;
		}				
	}

	// The object's vptr slot and the game's real vtable address, so unload can restore it.
	static uintptr_t* g_lobbyVptrSlot = nullptr;
	static uintptr_t g_origLobbyVtable = 0;

	void ApplyVMTHooks()
	{
		// Redirect this object's vptr to a private copy of the vtable and patch the copy. Never
		// write the game's own vtable: it sits in Arxan-checksummed read-only data, and patching
		// slot 24 in place derailed execution to a bogus address on the lobby path.
		auto ptr = *(uintptr_t*)DW_LOBBY;
		if (!ptr)
		{
			ZLOG("vmt: SKIPPED lobby object not ready");
			return;
		}

		auto vptrSlot = *(uintptr_t**)(ptr + 1384);
		if (!vptrSlot)
		{
			ZLOG("vmt: SKIPPED null vptr slot");
			return;
		}

		auto vmt = (uintptr_t*)*vptrSlot;
		if (!vmt)
		{
			ZLOG("vmt: SKIPPED null vtable");
			return;
		}

		auto vtable_buf = new uintptr_t[50];
		for (auto count = 0; count < 50; ++count)
		{
			vtable_buf[count] = vmt[count];
		}

		vtable_buf[24] = (uintptr_t)functions::Global_InstantMessage;

		g_lobbyVptrSlot = vptrSlot;
		g_origLobbyVtable = (uintptr_t)vmt;
		*vptrSlot = (uintptr_t)vtable_buf;
	}

	void RemoveVMTHooks()
	{
		if (!g_lobbyVptrSlot || !g_origLobbyVtable)
		{
			return;
		}

		// Point the object back at the game's real vtable, otherwise unloading leaves it
		// dispatching through a table in a freed module. The slot is in the game's own object,
		// not its read-only image, so no VirtualProtect is needed.
		*g_lobbyVptrSlot = g_origLobbyVtable;

		// The copy is leaked deliberately: 400 bytes once per process, and another thread may
		// still be inside a call dispatched through it.
		g_lobbyVptrSlot = nullptr;
		g_origLobbyVtable = 0;
	}

	// Skips the write if VirtualProtect fails, rather than writing anyway as 3.04 did: these are
	// hardcoded RVAs and this runs before the exception handler is installed, so a write to an
	// unmapped address is an unrecoverable startup crash.
	//
	// The bool is returned but no caller reads it, so a failure here is silent. Left that way
	// deliberately: the Arxan build check runs first, so reaching this with a bad address means
	// something has already gone wrong that a log line would not help with.
	static bool patch_bytes(void* address, const void* bytes, size_t size)
	{
		DWORD oldProtect = 0;
		if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			return false;
		}

		memcpy(address, bytes, size);
		FlushInstructionCache(GetCurrentProcess(), address, size);

		DWORD dummy = 0;
		VirtualProtect(address, size, oldProtect, &dummy);
		return true;
	}

	void ApplyMemoryPatches()
	{
		static const unsigned char nop5[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
		static const unsigned char movzx_word = 0xB7; // 0F BF movsx r32,r/m16 -> 0F B7 movzx
		static const unsigned char movzx_byte = 0xB6; // 0F BE movsx r32,r/m8  -> 0F B6 movzx
		static const unsigned char token_limit = 0x2F;
		static const __int32 above_normal = 0x00008000;

		// SL fix (nop out the calls to com_error)
		patch_bytes((void*)REBASE(0x1964766), nop5, sizeof(nop5));
		patch_bytes((void*)REBASE(0x19646A7), nop5, sizeof(nop5));

		// fix movsx issue with package readglob
		patch_bytes((void*)REBASE(0x1EEACF2 + 1), &movzx_word, 1);

		patch_bytes((void*)PTR_Cmp_TokenizeStringInternal, &token_limit, 1);

		// process priority: above normal
		patch_bytes((void*)REBASE(0x22BC1CD), &above_normal, sizeof(above_normal));
		patch_bytes((void*)REBASE(0x22BC1D2), &above_normal, sizeof(above_normal));

		// movsx -> movzx
		patch_bytes((void*)REBASE(0x1F21EF8), &movzx_byte, 1);
		patch_bytes((void*)REBASE(0x1CA4C84), &movzx_byte, 1);
	}

	// MH_CreateHook's status was discarded at every call site below. A hook that fails to install
	// removes whatever it was protecting with no crash and no message.
	static void CreateHookChecked(LPVOID target, LPVOID detour, LPVOID* original, const char* detourName)
	{
		const MH_STATUS status = MH_CreateHook(target, detour, original);
		if (status != MH_OK)
		{
			ZLOG("hook: FAILED %s (MH_STATUS=%d)", detourName, (int)status);
		}
	}

	// A macro only so the log carries the detour's name: #detour needs the preprocessor.
#define CREATE_HOOK(target, detour, original) CreateHookChecked((target), (LPVOID)(detour), (original), #detour)

	void ApplyHooks()
	{
		CREATE_HOOK((LPVOID)REBASE(0x1EEA560), functions::hkLobbyMsgRW_PrepWriteMsg, (LPVOID*)&LobbyMsgRW_PrepWriteMsg);
		CREATE_HOOK((LPVOID)REBASE(0x1EEB8D0), functions::hkLobbyMsgRW_PrepReadMsg, (LPVOID*)&LobbyMsgRW_PrepReadMsg);
		CREATE_HOOK((LPVOID)REBASE(0x20E31C0), functions::hkCOD_GetBuildTitle, (LPVOID*)&COD_GetBuildTitle);
		CREATE_HOOK((LPVOID)REBASE(0x1E016C0), functions::hkLive_SystemInfo, (LPVOID*)&Live_SystemInfo);
		CREATE_HOOK((LPVOID)REBASE(0x1EDFA60), functions::hkLobbyTypes_GetMsgTypeName, (LPVOID*)&LobbyTypes_GetMsgTypeName);
		CREATE_HOOK((LPVOID)REBASE(0x2BC4EB0), functions::hkqmemcpy, (LPVOID*)&qmemcpy);
		CREATE_HOOK((LPVOID)REBASE(0x22C9650), functions::hkflsomeWeirdCharacterIndex, (LPVOID*)&flsomeWeirdCharacterIndex);
		CREATE_HOOK((LPVOID)REBASE(0x221CE90), functions::hkSEH_ReplaceDirectiveInStringWithBinding, (LPVOID*)&SEH_ReplaceDirectiveInStringWithBinding);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA3E0), functions::hkLobbyMsgRW_PackageInt, (LPVOID*)&LobbyMsgRW_PackageInt);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA490), functions::hkLobbyMsgRW_PackageUInt, (LPVOID*)&LobbyMsgRW_PackageUInt);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA450), functions::hkLobbyMsgRW_PackageUChar, (LPVOID*)&LobbyMsgRW_PackageUChar);
		CREATE_HOOK((LPVOID)REBASE(0x1F27400), functions::hkUI_DoModelStringReplacement, (LPVOID*)&UI_DoModelStringReplacement);
		CREATE_HOOK((LPVOID)REBASE(0x1EA9C80), functions::hkLiveSteam_InitServer, (LPVOID*)&LiveSteam_InitServer);
		CREATE_HOOK((LPVOID)REBASE(0x195F100), functions::hkCMD_MenuReponse_f, (LPVOID*)&CMD_MenuResponse_f);
		CREATE_HOOK((LPVOID)REBASE(0x195EFA0), functions::hk_CMD_MenuResponseCached_f, (LPVOID*)&CMD_MenuResponseCached_f);
		CREATE_HOOK((LPVOID)REBASE(0x200CF00), functions::hkUI_Model_GetModelFromPath_0, (LPVOID*)&UI_Model_GetModelFromPath_0);
		CREATE_HOOK((LPVOID)REBASE(0x200D5B0), functions::hkUI_Model_GetModelFromPath, (LPVOID*)&UI_Model_GetModelFromPath);
		CREATE_HOOK((LPVOID)REBASE(0x200CFC0), functions::hkUI_Model_CreateModelFromPath, (LPVOID*)&UI_Model_CreateModelFromPath);
		CREATE_HOOK((LPVOID)REBASE(0x200CD00), functions::hkUI_Model_AllocateNode, (LPVOID*)&UI_Model_AllocateNode);
		CREATE_HOOK((LPVOID)REBASE(0x211F5A0), functions::hkSys_VerifyPacketChecksum, (LPVOID*)&Sys_VerifyPacketChecksum);
		CREATE_HOOK((LPVOID)REBASE(0x211F500), functions::hkSys_ChecksumCopy, (LPVOID*)&Sys_ChecksumCopy);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA940), functions::hkLobbyMsgRW_PrintMessage, (LPVOID*)&LobbyMsgRW_PrintMessage);
		CREATE_HOOK((LPVOID)REBASE(0x1EEA680), functions::hkLobbyMsgRW_PrintDebugMessage, (LPVOID*)&LobbyMsgRW_PrintDebugMessage);
		CREATE_HOOK((LPVOID)REBASE(0x1EF8A60), functions::hkExecLuaCMD, (LPVOID*)&ExecLuaCMD);
		CREATE_HOOK((LPVOID)REBASE(0x1EBB200), functions::hkLive_UserGetName, (LPVOID*)&Live_UserGetName);
		CREATE_HOOK((LPVOID)&__report_gsfailure, functions::__report_gsfailure_hook, (LPVOID*)&pOriginalGSFailure);
		CREATE_HOOK((LPVOID)REBASE(0x143A620), functions::hkdwInstantDispatchMessage, (LPVOID*)&dwInstantDispatchMessage);
		CREATE_HOOK((LPVOID)REBASE(0x134CD70), functions::hkCL_ConnectionlessCMD, (LPVOID*)&CL_ConnectionlessCMD);
		CREATE_HOOK((LPVOID)REBASE(0x1E85450), functions::hkLivePresence_Serialize, (LPVOID*)&LivePresence_Serialize);
		CREATE_HOOK((LPVOID)REBASE(0x1321130), functions::hkCL_GetConfigString, (LPVOID*)&CL_GetConfigString);
		CREATE_HOOK((LPVOID)REBASE(0x20CB0C0), functions::hkMods_SubscribeUGC, (LPVOID*)&Mods_SubscribeUGC);
		CREATE_HOOK((LPVOID)REBASE(0x1E19B30), functions::hkLiveInvites_AcceptInvite, (LPVOID*)&LiveInvites_AcceptInvite);
		CREATE_HOOK((LPVOID)REBASE(0x1E724A0), functions::hkLiveInvites_SendJoinInfo, (LPVOID*)&LiveInvites_SendJoinInfo);
		CREATE_HOOK((LPVOID)REBASE(0x1E72040), functions::hkLiveInvites_JoinMessageAction, (LPVOID*)&LiveInvites_JoinMessageAction);
		CREATE_HOOK((LPVOID)REBASE(0x1EA4E30), functions::hkUI_BrowserOpen, (LPVOID*)&UI_BrowserOpen);
		/*MH_CreateHook((LPVOID)REBASE(0xA7DE0), functions::hkBG_Cache_GetScriptMenuNameForIndex, (LPVOID*)&BG_Cache_GetScriptMenuNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA78A0), functions::hkBG_Cache_GetEventStringNameForIndex, (LPVOID*)&BG_Cache_GetEventStringNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7AB0), functions::hkBG_Cache_GetLocStringNameForIndex, (LPVOID*)&BG_Cache_GetLocStringNameForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7A00), functions::hkBG_Cache_GetLUIMenuForIndex, (LPVOID*)&BG_Cache_GetLUIMenuForIndex);
		MH_CreateHook((LPVOID)REBASE(0xA7990), functions::hkBG_Cache_GetLUIMenuDataForIndex, (LPVOID*)&BG_Cache_GetLUIMenuDataForIndex);*/
		CREATE_HOOK((LPVOID)REBASE(0x1EAAD60), functions::hkUserHasLicenseForApp, (LPVOID*)&UserHasLicenseForApp);
		CREATE_HOOK((LPVOID)REBASE(0x1DFCC60), functions::hkLiveInventory_GetItemQuantity, (LPVOID*)&LiveInventory_GetItemQuantity);
		CREATE_HOOK((LPVOID)REBASE(0x1E06110), functions::hkLiveEntitlements_IsEntitlementActiveForController, (LPVOID*)&LiveEntitlements_IsEntitlementActiveForController);
		CREATE_HOOK((LPVOID)REBASE(0x1DFC580), functions::hkLiveInventory_AreExtraSlotsPurchased, (LPVOID*)&LiveInventory_AreExtraSlotsPurchased);
		CREATE_HOOK((LPVOID)REBASE(0x1DFDFE0), functions::hkLiveInventory_IsValid, (LPVOID*)&LiveInventory_IsValid);
		CREATE_HOOK((LPVOID)REBASE(0x227BDA0), functions::hkInfo_ValueForKey, (LPVOID*)&Info_ValueForKey);

		const MH_STATUS enableStatus = MH_EnableHook(MH_ALL_HOOKS);
		if (enableStatus != MH_OK)
		{
			ZLOG("hook: MH_EnableHook FAILED (MH_STATUS=%d)", (int)enableStatus);
		}
	}

	void DestroyHooks()
	{
		const MH_STATUS disableStatus = MH_DisableHook(MH_ALL_HOOKS);
		if (disableStatus != MH_OK)
		{
			ZLOG("hook: MH_DisableHook FAILED (MH_STATUS=%d) - detours may still be live", (int)disableStatus);
		}
		RemoveVMTHooks();
	}

}