/* ==========================================================
 * d2warden
 * https://github.com/lolet/d2warden
 * ==========================================================
 * Copyright 2011-2013 Bartosz Jankowski
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ========================================================== */

#include "stdafx.h"
#include "D2Handlers.h"
#include "WardenClient.h"

#include "LSpectator.h"
#include "LRoster.h"
#include "LItems.h"
#include "LEvents.h"
#include "LMonsters.h"
#include "LWorldEvent.h"
#include "process.h"
#include "RC4.h"
#include "Build.h"
#include "LQuests.h"

using namespace std;

BYTE __fastcall LEVELS_GetActByLevel(Level* pLevel)
{
	ASSERT(pLevel)
		return LEVELS_GetActByLevelNo(pLevel->dwLevelNo);
}

BYTE __fastcall LEVELS_GetActByRoom2(int _1, Room2* pRoom2)
{
	ASSERT(pRoom2)
	ASSERT(pRoom2->pLevel)
	return LEVELS_GetActByLevelNo(pRoom2->pLevel->dwLevelNo);

}

/*
Modified beacuse the original function uses town lvl to determine act
*/
DWORD __stdcall LEVELS_GetActByLevelNo(DWORD nLevel)
{

	if (nLevel >= (*D2Vars.D2COMMON_sgptDataTables)->dwLevelsRecs)
	{
		DEBUGMSG("Invalid level id (%d)", nLevel)
		return 0;
	}
	LevelsTxt* pLevel = &(*D2Vars.D2COMMON_sgptDataTables)->pLevelsTxt[nLevel];
	if (pLevel)
	{
		return pLevel->nAct;
	}
	return 0;
}


BOOL __fastcall D2GAME_IsUnitDead(UnitAny* pUnit)
{
	if (!pUnit)
		return 0;

	if (pUnit->dwFlags & UNITFLAG_DEAD)
		return 1;

	if (pUnit->dwType == UNIT_MONSTER)
	{
		if (pUnit->dwMode == NPC_MODE_DEATH || pUnit->dwMode == NPC_MODE_DEAD)
			return 1;
	}
	else if (pUnit->dwType == UNIT_PLAYER)
	{
		if (pUnit->dwMode == PLAYER_MODE_DEATH || pUnit->dwMode == PLAYER_MODE_DEAD)
			return 1;

		if (D2Funcs.D2COMMON_GetUnitState(pUnit, D2EX_SPECTATOR_STATE))
		{
			return 1;
		}
	}
	return 0;
}

Room1* __stdcall D2GAME_PortalCrashFix(Act* ptAct, int LevelNo, int Unk0, int* xPos, int* yPos, int UnitAlign)
{
	Room1* ptRoom = D2Funcs.D2COMMON_GetRoomXYByLevel(ptAct, LevelNo, Unk0, xPos, yPos, UnitAlign);
	return ptRoom;

}

void  __stdcall OnLastHit(UnitAny* ptKiller, UnitAny * ptVictim, Damage * ptDamage)
{
	if (ptKiller->dwType == UNIT_PLAYER && ptVictim->dwType == UNIT_PLAYER) {
		if (D2Funcs.D2COMMON_GetStatSigned(ptVictim, STAT_HP, 0) <= 0)
		{
			int Dmg = (int)ptDamage->DamageTotal >> 8;
			if (Dmg > 50000) return;
			if (Dmg > ptKiller->pGame->DmgRekord) {
				ostringstream DmgStr;
				DmgStr << Dmg << "!";
				SendExEvent(ptKiller->pPlayerData->pClientData, COL_RED, D2EX_HOLYSHIT, 3, 150, -1, DmgStr.str(), DmgStr.str());
				if (!ptKiller->pGame->szRekordHolder[0]) {
					ostringstream EngMsg; EngMsg << ptKiller->pPlayerData->pClientData->AccountName << " has set a new damage record! (" << Dmg << " hp)";
					ostringstream PolMsg; PolMsg << ptKiller->pPlayerData->pClientData->AccountName << " ustanowil nowy rekord obrazen! (" << Dmg << " hp)";
					BroadcastEventMsgEx(ptKiller->pGame, COL_YELLOW, EngMsg.str(), PolMsg.str());
				}
				else
				{
					ostringstream EngMsg; EngMsg << ptKiller->pPlayerData->pClientData->AccountName << " has set a new damage record! (" << Dmg << " hp). Previous record belonged to " << ptKiller->pGame->szRekordHolder << " (" << ptKiller->pGame->DmgRekord << ')';
					ostringstream PolMsg; PolMsg << ptKiller->pPlayerData->pClientData->AccountName << " ustanowil nowy rekord obrazen! (" << Dmg << " hp). Poprzedni rekord nalezal do " << ptKiller->pGame->szRekordHolder << " (" << ptKiller->pGame->DmgRekord << ')';
					BroadcastEventMsgEx(ptKiller->pGame, COL_YELLOW, EngMsg.str(), PolMsg.str());
				}

				BroadcastExEvent(ptKiller->pGame, COL_WHITE, ptKiller->dwUnitId, 0, "data\\D2Ex\\Blobs");

				ptKiller->pGame->DmgRekord = Dmg;
				strcpy_s(ptKiller->pGame->szRekordHolder, 16, ptKiller->pPlayerData->pClientData->AccountName);
			}


			if ((GetTickCount() - ptVictim->pPlayerData->LastDamageTick < 5000) && ptVictim->pPlayerData->LastDamageId != 0 && ptVictim->pPlayerData->LastDamageId != ptKiller->dwUnitId) {
				UnitAny* pAssister = D2ASMFuncs::D2GAME_FindUnit(ptKiller->pGame, ptVictim->pPlayerData->LastDamageId, UNIT_PLAYER);
				if (pAssister) {
					LRoster::UpdateRoster(ptKiller->pGame, pAssister->pPlayerData->szName, 3);
					LRosterData * AssRoster = LRoster::Find(ptKiller->pGame, pAssister->pPlayerData->szName);
					if (AssRoster) LRoster::SyncClient(ptKiller->pGame, ptVictim->pPlayerData->LastDamageId, AssRoster);
					BroadcastExEvent(ptKiller->pGame, COL_WHITE, ptVictim->pPlayerData->LastDamageId, 5, "data\\D2Ex\\Blobs");
				}
			}
			ptVictim->pPlayerData->LastDamage = 0;
			ptVictim->pPlayerData->LastDamageId = 0;
			ptVictim->pPlayerData->LastDamageTick = 0;
		}
		else {
			int Percent = ((((int)ptDamage->DamageTotal >> 8) * 100) / (D2Funcs.D2COMMON_GetStatSigned(ptVictim, STAT_MAXHP, 0) >> 8));
			if (ptVictim->pPlayerData->LastDamageTick &&
				(GetTickCount() - ptVictim->pPlayerData->LastDamageTick < 5000)) ptVictim->pPlayerData->LastDamage += Percent; else	ptVictim->pPlayerData->LastDamage = Percent;
			ptVictim->pPlayerData->LastDamageId = ptKiller->dwUnitId;
			ptVictim->pPlayerData->LastDamageTick = GetTickCount();
		}
	}
}

void __fastcall DAMAGE_FireEnchanted(Game *pGame, UnitAny *pUnit, int a4, int a5)
{
	//Log("a4 =%d, a5 =%d",a4,a5);
}


BOOL __stdcall isPermStore(Game* ptGame, UnitAny* ptNPC, UnitAny* ptItem)
{
	Npc* ptVendor = ptGame->pNpcControl->pNpc;

	if (!ptNPC || !ptVendor)
	{
		DEBUGMSG("isPermStore: null ptNPC or ptVendor!");
		return FALSE;
	}

	for (int i = 0; i < 64; i++)
	{
		if (ptVendor[i].npcNo == ptNPC->dwClassId) { ptVendor = &ptVendor[i]; break; }
		if (i == 63) { ptVendor = 0; break; }
	}

	if (ptVendor)
	{
		DWORD iCode = D2Funcs.D2COMMON_GetItemCode(ptItem);
		//WORLD EVENT
		if (Warden::getInstance().wcfgEnableWE && WE_isKey(ptItem))
		{
			if (Warden::getInstance().SellCount == Warden::getInstance().NextDC) { WE_GenerateNextDC(); WE_CreateDCKey(ptNPC); }

			ptItem->pItemData->InvPage = 0xFF;
			return TRUE;
		}
		//OLD CODE
		if (ptGame->DifficultyLevel && (iCode == ' 4ph' || iCode == ' 5ph' || iCode == ' 4pm' || iCode == ' 5pm')) return TRUE;

		for (unsigned int i = 0; i < ptVendor->nPermStoreRecords; i++)
		{
			if (ptVendor->pPermStoreList[i] == iCode) return 1;
		}
	}

	return FALSE;
}


void __fastcall OnMonsterDeath(UnitAny* ptKiller, UnitAny * ptVictim, Game * ptGame)
{
	if (!ptVictim || !ptKiller) return;
	if (ptKiller->dwType == UNIT_MONSTER) return;

	switch (ptVictim->dwType)
	{
	case UNIT_MONSTER:
#if (_DEBUG)
		int nStr = ptVictim->pMonsterData->pMonStatsTxt->wNameStr;
		wchar_t* wStr = D2Funcs.D2LANG_GetLocaleText(nStr);
		char* szMonster = new char[wcslen(wStr) + 1];
		WideToChar(szMonster, wStr);
		DEBUGMSG("Killed a '%s'", szMonster);
		delete[] szMonster;
#endif
	break;
	}

	////TO MOze crashoac
	//if(ptKiller->dwType==0) 
	//LRoster::UpdateRoster(ptGame,ptKiller->pPlayerData->szName,0);
	//LRoster::UpdateRoster(ptGame,ptVictim->pPlayerData->szName,1);

	//LRoster::SendKills(ptGame);

}


void __fastcall OnNPCHeal(UnitAny* pUnit)
{
	StatList* pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 1); // FREEZE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 2); // PSN
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 54); // UNINTERUPTABLE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 87); // SLOW MISSILE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}

	if (pUnit->pGame, pUnit->pGame->dwGameState == 1) //FFA MODE
		CreateFFAItems(pUnit);


	if (pUnit->pGame, pUnit->pGame->bFestivalMode == 1)
		if (pUnit->pPlayerData->isPlaying)
		{
			pUnit->pPlayerData->CanAttack = 0;
			pUnit->pPlayerData->SaidGO = 0;

			DoRoundEndStuff(pUnit->pGame, pUnit);

			pUnit->pPlayerData->isPlaying = 0;
			pUnit->pGame->nPlaying--;
		}


}

DWORD __fastcall OnD2ExPacket(Game* ptGame, UnitAny* ptPlayer, BYTE *ptPacket, DWORD PacketLen) // Zostawione do wstecznej kompatybilnosci
{
	if (PacketLen != 5) return 3;
	DWORD Version = *(DWORD*)&ptPacket[1];
	//Log("Otrzymano pakiet wersji!");
	SendMsgToClient(ptPlayer->pPlayerData->pClientData, ptPlayer->pPlayerData->pClientData->LocaleID == 10 ? "Uwaga masz starego klienta! Wpisz #update w celu aktualizacji" : "Warning, your client is outdated! Type #update to fix this!");
	return 0;
	//if(Version==1)
	//{
	//WardenClient_i ptCurrentClient = GetClientByID(ptPlayer->pPlayerData->pClientData->ClientID);
	//if(ptCurrentClient == Warden::getInstance().clients.end()) return 0;
	//ptCurrentClient->NewPatch=1;
	//
	//return 0;
	//}
	//return 3;
}



DWORD __fastcall SetDRCap(Attack *pAttack) //Unused atm but will be
{
	int DR = 50;
	//	
	//if(pAttack->dwAttackerType==0 && pAttack->dwDefenderType==0 && ( pAttack->ptDamage->ResultFlags & RESULTFLAG_DEATH))
	//{ 
	//	ExEvent hEvent = {0};
	//	hEvent.MsgType = 1;
	//	hEvent.Color = COL_RED;
	//	hEvent.wX = -1;
	//	hEvent.wY = 50;
	//	hEvent.Argument = 2;
	//	hEvent.Sound = D2EX_HOLYSHIT;
	//	hEvent.P_A6 = 0xA6;
	//	int Dmg = pAttack->ptDamage->PhysicalDamage +  pAttack->ptDamage->FireDamage +  pAttack->ptDamage->ColdDamage +  pAttack->ptDamage->LightningDamage +  pAttack->ptDamage->MagicDamage;
	//	sprintf_s(hEvent.szMsg,255,"%d !",Dmg);
	//	hEvent.PacketLen = 0xE + strlen(hEvent.szMsg) +1;
	//	BroadcastPacket(pAttack->pGame,(BYTE*)&hEvent,hEvent.PacketLen);
	//	SendMsgToClient(pAttack->pAttacker->pPlayerData->pClientData,"Last hit : %d",Dmg);
	//}

	return DR;
}

Act* __stdcall OnActLoad(DWORD ActNumber, DWORD InitSeed, DWORD Unk0, Game *pGame, DWORD DiffLvl, D2PoolManager* pMemPool, DWORD TownLevelId, DWORD Func1, DWORD Func2)
{
	int MySeed = 0;
	BYTE MyDiff = (BYTE)DiffLvl;
	if (strlen(pGame->GameDesc) > 0)
	{
		char *nt = 0;
		char tk[32];
		strcpy_s(tk, 32, pGame->GameDesc);
		char * ret = strtok_s(tk, "- ", &nt);
		while (ret)
		{
			if (ret[0] == 'm' && strlen(ret) > 1 && Warden::getInstance().wcfgEnableSeed)
				MySeed = atoi(ret + 1);
			if (ret[0] == 't' && strlen(ret) == 1 && Warden::getInstance().wcfgAllowTourMode)
				pGame->bFestivalMode = 1;
			if (_strnicmp(ret, "ffa", 3) == 0 && Warden::getInstance().wcfgFFAMode)
				pGame->dwGameState = 1;
			ret = strtok_s(NULL, "- ", &nt);
		}
	}
	if (MySeed)
	{
		pGame->InitSeed = MySeed;
		return D2Funcs.D2COMMON_LoadAct(ActNumber, MySeed, Unk0, pGame, MyDiff, pMemPool, TownLevelId, Func1, Func2);
	}
	return D2Funcs.D2COMMON_LoadAct(ActNumber, InitSeed, Unk0, pGame, MyDiff, pMemPool, TownLevelId, Func1, Func2);
}


/*
 UNUSED: TODO: MIGRATE TO LPACKETS.CPP
*/
BOOL __fastcall OnReceivePacket(BYTE * ThePacket, PacketData * pClient) // return is currently ignored
{
	if (!pClient) return true;
	int ClientID = pClient->ClientID;

	switch (ThePacket[0])
	{
	case 0x1A: //EQUIP CHECK
	case 0x1D:
	case 0x16:
	{
		break;
		// 1a   9   Equip item         1a [DWORD id] [WORD position] 00 00
		DWORD ItemID = 0;
		DWORD Socket = D2Funcs.D2NET_GetClient(ClientID);
		if (!Socket) break;
		Game* pGame = D2Funcs.D2GAME_GetGameByNetSocket(Socket);
		if (!pGame) break;
		ClientData* ptClientData = FindClientDataById(pGame, ClientID);
		if (!ptClientData) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

		if (ThePacket[0] == 0x16)
			ItemID = *(DWORD*)&ThePacket[5];
		else
			ItemID = *(DWORD*)&ThePacket[1];
		UnitAny* ptItem = D2ASMFuncs::D2GAME_FindUnit(ptClientData->pGame, ItemID, 4);
		if (ptItem)
		{
			if (!ptItem->pItemData->dwItemFlags.bPersonalized) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

			if (!isAnAdmin(ptClientData->AccountName))
				if (ptClientData->AccountName != ptItem->pItemData->szPlayerName)
				{
					SendMsgToClient(ptClientData, ptClientData->LocaleID == LOC_PL ? "Nie mozesz zalozyc przedmiotu, ktory nie nalezy do ciebie!" : "You cant equip item bound to other player!");
					*(DWORD*)&ThePacket[1] = 0;
				}
		}
		D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame);
	}
		break;
	case 0x1F://STASH HACK
	{
		//GC 78:   0x1F SwapContainerItem; SubjectUID: 28; ObjectUID: 29; X: 5; Y: 0
		//GC 78:   17   1f [1c 00 00 00] [1d 00 00 00] [05 00 00 00] [00 00 00 00]
		DWORD Socket = D2Funcs.D2NET_GetClient(ClientID);
		if (!Socket) break;
		Game* pGame = D2Funcs.D2GAME_GetGameByNetSocket(Socket);
		if (!pGame) break;
		ClientData* ptClientData = FindClientDataById(pGame, ClientID);
		if (!ptClientData) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

		int LvlNo = D2Funcs.D2COMMON_GetLevelNoByRoom(ptClientData->ptRoom);
		if (LvlNo == 0) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		int ActNo = D2Funcs.D2COMMON_GetActNoByLevelNo(LvlNo);
		if (LvlNo == D2Funcs.D2COMMON_GetTownLevel(ActNo)) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		DWORD ItemID = *(DWORD*)&ThePacket[5];
		UnitAny* ptItem = D2ASMFuncs::D2GAME_FindUnit(ptClientData->pGame, ItemID, UNIT_ITEM);
		if (!ptItem) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		if (ptItem->pItemData->InvPage == 4) Log("HACK: %s (*%s) opened stash being out of town [STASH HACK]!", ptClientData->CharName, ptClientData->AccountName);
		D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame);
	}
		break;
	}
	return true;
}

/*
	Very early - client status & 1
	Don't use...
*/
int __fastcall OnGameEnter(ClientData* pClient, Game* ptGame, UnitAny* ptPlayer)
{
	//if(pClient->InitStatus!=4)
	//Log("NowyKlient: -> %s, InitStatus == %d",pClient->CharName,pClient->InitStatus);

	DEBUGMSG("Entering the game!")


	return 0;
}

int __fastcall ReparseChat(Game* pGame, UnitAny *pUnit, BYTE *ThePacket, int PacketLen)
{
	char Msg[256];
	char text[256];
	WORD MsgLen = 0;
	char *szName = pUnit->pPlayerData->szName;
	int nNameLen = strlen(szName);
	strncpy_s(Msg, 255, (char*)ThePacket + 3, -1);
	Msg[255] = 0;
	BYTE * aPacket = 0;

	if (Warden::getInstance().wcfgAllowLoggin) LogToFile("GameLog.txt", true, "%s\t%s\t%s\t%s", pGame->GameName, pUnit->pPlayerData->pClientData->AccountName, szName, Msg);

	if (nNameLen > 12)
	{
		_snprintf_s(text, 256, 256, "%s: ÿc%d%s", szName, COL_WHITE, Msg);

		MsgLen = 27 + strlen(text);
		aPacket = new BYTE[MsgLen];
		memset(aPacket, 0, MsgLen);
		aPacket[0] = 0x26;
		aPacket[1] = 0x04;
		aPacket[3] = 0x02; //MsgStyle
		aPacket[8] = GetColorNameByAcc(pUnit->pPlayerData->pClientData->AccountName);
		strcpy_s((char*)aPacket + 10, 16, "[administrator]");
		strcpy_s((char*)aPacket + 26, MsgLen - 26, text);
	}
	else
	{
		MsgLen = 12 + strlen(Msg) + nNameLen + 3;
		aPacket = new BYTE[MsgLen];
		memset(aPacket, 0, MsgLen);
		aPacket[0] = 0x26;
		aPacket[1] = 0x01;
		aPacket[3] = 0x02; //MsgStyle
		aPacket[9] = 0x53; //MsgFlags
		aPacket[10] = 0xFF;
		aPacket[11] = 'c';
		aPacket[12] = 0x30 + (BYTE)GetColorNameByAcc(pUnit->pPlayerData->pClientData->AccountName);
		strcpy_s((char*)aPacket + 13, nNameLen + 1, szName);
		strcpy_s((char*)aPacket + 14 + nNameLen, strlen(Msg) + 1, Msg);
	}

	ClientData * pClientList = pGame->pClientList;
	while (pClientList)
	{
		if (pClientList->InitStatus & 4) 
			D2ASMFuncs::D2GAME_SendPacket(pClientList, aPacket, MsgLen);
		if (!pClientList->ptPrevious) break;
		pClientList = pClientList->ptPrevious;
	}

	delete[] aPacket;
	return 0;
}


void __fastcall TestEvent(Game *pGame, UnitAny *pUnit, int nTimerType, void* pArg, void* pArgEx)
{
	SendMsgToClient(pUnit->pPlayerData->pClientData, "It worked!");
	DEBUGMSG(":~)")
}

BOOL __fastcall OnChat(UnitAny* pUnit, BYTE *ThePacket)
{
	int ClientID = pUnit->pPlayerData->pClientData->ClientID;

	if (ClientID == NULL) return TRUE;
	if (ThePacket[1] != 0x01) return TRUE;
	char Msg[500];
	strcpy_s(Msg, 500, (char*)ThePacket + 3);
	if (Msg[0] == '#')
	{
		char * str, *t;
		str = strtok_s(Msg, " ", &t);
		if (str)
		{
			if (!ParseItemsCmds(pUnit, str, t)) return false;
			if (!ParseMonCmds(pUnit, str, t)) return false;

			//if(_stricmp(str,"#players")==0)
			//{
			//str = strtok_s(NULL," ",&t);
			//if(!str) { SendMsgToClient(pUnit->pPlayerData->pClientData,"Type player count!"); return false;}
			//int NewPl = atoi(str);
			//SendMsgToClient(pUnit->pPlayerData->pClientData,"Prev players set from %d to %d.",PNo,NewPl);
			//PNo=NewPl;
			//return false;
			//}
			if (_stricmp(str, "#testevent") == 0)
			{
				D2Funcs.D2GAME_SetTimer(pUnit->pGame, pUnit, UNITEVENT_CUSTOM, pUnit->pGame->GameFrame + (5 * 25), (DWORD)&TestEvent, NULL, NULL);
				DEBUGMSG("Created an event...")
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Ok!");
				return false;
			}
			if (_stricmp(str, "#test2") == 0)
			{
				char* szlvl = strtok_s(NULL, " ", &t);
				if (!szlvl) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#test2 <lvl no>");  return false; }
				int nLvl = atoi(szlvl);
				MonsterRegion * pRegion = pUnit->pGame->pMonsterRegion[nLvl];
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Data for: %d", nLvl);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "nQuest: %d, nMonstersASSUME: %d, nActiveRooms: %d, nOtherMonsterCounter: %d, nSpawned: %d", pRegion->nQuest, pRegion->nMonstersASSUME, pRegion->nActiveRooms, pRegion->nOtherMonsterCounter, pRegion->nSpawned);
				return false;
			}
			if (_stricmp(str, "#cheer") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				char* Txt = strtok_s(NULL, " ", &t);
				if (!Txt) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				char* SoundId = strtok_s(NULL, " ", &t);
				if (!SoundId) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				char* Acc = strtok_s(NULL, " ", &t);
				if (!Acc) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				WORD sId = (WORD)atoi(SoundId);
				if (sId > 4713) sId = 0;
				ClientData* pDest = FindClientDataByName(pUnit->pGame, Acc);
				if (!pDest) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!");  return false; }

				SendExEvent(pDest, COL_WHITE, sId, 5, -1, 150, Txt, Txt);
				BroadcastExEvent(pUnit->pGame, COL_WHITE, pDest->pPlayerUnit->dwUnitId, 5, "data\\D2Ex\\Blobs");
				return false;
			}
			if (_stricmp(str, "#1") == 0)
			{
				if (!pUnit->pGame->LastKiller) return true;
				if (pUnit->pGame->LastKiller == pUnit->pPlayerData->pClientData->ClientID) return true;
				ClientData* pDest = FindClientDataById(pUnit->pGame, pUnit->pGame->LastKiller);
				if (!pDest) return true;
				SendExEvent(pDest, COL_WHITE, D2EX_ACHIVEMENT, 3, -1, 150, "#1", "#1");
				BroadcastExEvent(pUnit->pGame, COL_WHITE, pDest->pPlayerUnit->dwUnitId, 2, "data\\D2Ex\\Blobs");
				pUnit->pGame->LastKiller = 0;
				return true;
			}
			if (_stricmp(str, "#0") == 0)
			{
				if (!pUnit->pGame->LastVictim) return true;
				if (pUnit->pGame->LastVictim == pUnit->pPlayerData->pClientData->ClientID) return true;
				ClientData* pDest = FindClientDataById(pUnit->pGame, pUnit->pGame->LastVictim);
				if (!pDest) return true;
				SendExEvent(pDest, COL_PURPLE, D2EX_HUMILATION, 3, -1, 150, "Jestes zerem!", "Such a noob!");
				BroadcastExEvent(pUnit->pGame, COL_PURPLE, pDest->pPlayerUnit->dwUnitId, 2, "data\\D2Ex\\Blobs");
				pUnit->pGame->LastVictim = 0;
				return true;
			}
			if (_stricmp(str, "#roster") == 0)
			{
				LRosterData * LR = LRoster::Find(pUnit->pGame, pUnit->pPlayerData->szName);
				if (LR)	SendMsgToClient(pUnit->pPlayerData->pClientData, "Kills : %d | Deaths : %d | Assists : %d | Runaways : %d", 
					LR->Kills, LR->Deaths, LR->Assists, LR->Giveups);
				else SendMsgToClient(pUnit->pPlayerData->pClientData, "No roster data available!");
				return false;
			}
			if (_stricmp(str, "#we") == 0)
			{
				if (!Warden::getInstance().wcfgEnableWE) return true;
				int z = Warden::getInstance().NextDC - Warden::getInstance().SellCount; // 261 - 217 ( 44)
				int y = z - Warden::getInstance().MinSell; // 50 -40 =  10
				if (y < 0) y = Warden::getInstance().MinSell + y;  // 40 - 30 = 10
				else y = Warden::getInstance().MinSell;
				if (y == 0) y = 1;
				int x = y + (Warden::getInstance().MaxSell - Warden::getInstance().MinSell);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "World Event is %s, Sell count : %d, Need: %d-%d items", Warden::getInstance().wcfgEnableWE ? "On" : "Off", Warden::getInstance().SellCount, y, x);
				return false;
			}
			if (_stricmp(str, "#uptime") == 0)
			{
				int time = (GetTickCount() - Warden::getInstance().getUpTime()) / 1000;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "GS Uptime %.2d:%.2d:%.2d, WardenClients %d, pGame->nClients %d", time / 3600, (time / 60) % 60, time % 60, 
					Warden::getInstance().getClientCount(), pUnit->pGame->nClients);
				return false;
			}
			if (_stricmp(str, "#build") == 0)
			{
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Program build %d on %s, %s.", __BUILDNO__, __DATE__, __TIME__);
				return false;
			}
			if (_stricmp(str, "#dumpunits") == 0)
			{
				map<DWORD, UnitAny*> hmap;
				for (int i = 0; i < 128; ++i)
				{	
					for (UnitAny* u = pUnit->pGame->pUnitList[UNIT_OBJECT][i]; u; u = u->pListPrev)
					{
						hmap[u->dwUnitId] = u;
					}
					for (UnitAny* u = pUnit->pGame->pUnitList[UNIT_OBJECT][i]; u; u = u->pRoomPrev)
					{
						hmap[u->dwUnitId] = u;
					}
				}
				/*
				for (InactiveRoom * r = pUnit->pGame->pDrlgRoomList[D2ACT_V]; r; r = r->pNextRoom)
					for (InactiveObject *o = r->pObject; o; o = o->pNext)
				{
						DEBUGMSG("Class [%d], xy= [%d,%d]", o->dwClassId, o->xPos, o->yPos)
				}*/
				for (auto u = hmap.begin(); u != hmap.end(); ++u)
				{
					DEBUGMSG("%d:, %d @ [%d,%d]", u->second->dwUnitId, u->second->dwClassId, D2Funcs.D2GAME_GetUnitX(u->second), D2Funcs.D2GAME_GetUnitY(u->second));
				}
				SendMsgToClient(pUnit->pPlayerData->pClientData, "OK!");
				return false;
			}
			if (_stricmp(str, "#dumppresets") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName))  return TRUE;

				Room1* pRoom = D2Funcs.D2COMMON_GetUnitRoom(pUnit);
				if (pRoom) {
					for (PresetUnit* p = pRoom->pRoom2->pPreset; p; p = p->pPresetNext) {
						DEBUGMSG("hcIdx: %d[%s] @[%d, %d]", p->dwClassId, UnitTypeToStr(p->dwType), p->dwPosX, p->dwPosY)
					}
				}
				return false;
			}
			if (_stricmp(str, "#update") == 0)
			{
				if (Warden::getInstance().wcfgUpdateURL.empty()) return false;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Trying to download patch....");
				ExEventDownload pEvent;
				::memset(&pEvent, 0, sizeof(ExEventDownload));
				pEvent.P_A6 = 0xA6;
				pEvent.MsgType = EXEVENT_DOWNLOAD;
				pEvent.bExec = 1;
				strcpy_s(pEvent.szURL, 255, Warden::getInstance().wcfgUpdateURL.c_str());
				if (pEvent.szURL[0])
					pEvent.PacketLen = 14 + strlen(pEvent.szURL) + 1;
				else
					pEvent.PacketLen = 15;

				D2ASMFuncs::D2GAME_SendPacket(pUnit->pPlayerData->pClientData, (BYTE*)&pEvent, pEvent.PacketLen);
				return false;
			}
			if (_stricmp(str, "#map") == 0)
			{
				if (!pUnit) return TRUE;
				BroadcastMsg(pUnit->pPlayerData->pClientData->pGame, "Map Id : '%d'", pUnit->pPlayerData->pClientData->pGame->InitSeed);
				return true;
			}

			if (_stricmp(str, "#go") == 0 || _stricmp(str, "#g") == 0)
			{
				if (!pUnit->pGame->bFestivalMode) return true;

				int aLevel = D2Funcs.D2COMMON_GetTownLevel(pUnit->dwAct);
				int aCurrLevel = D2Funcs.D2COMMON_GetLevelNoByRoom(pUnit->pPath->pRoom1);
				if (aCurrLevel == aLevel)
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Najpierw opusc miasto!" : "Leave town first!");
					return true;
				}

				int PartyCount = 0;
				int nPlayers = 0;
				bool InParty = false;

				std::ostringstream pakiet;

				if (!pUnit->pGame->pPartyControl->ptLastParty)
				{
					for (ClientData* pSingle = pUnit->pGame->pClientList; pSingle; pSingle = pSingle->ptPrevious)
						pakiet << pSingle->CharName << '#' << pSingle->AccountName << '#' << ConvertClass(pSingle->ClassId) << '$';
				}
				else
					for (SubParty* pParty = pUnit->pGame->pPartyControl->ptLastParty; pParty; pParty = pParty->ptPrev)
					{
						for (PartyMembers* pMem = pParty->ptMembers; pMem; pMem = pMem->ptNext)
						{
							if (pMem->dwUnitId == pUnit->dwUnitId)  InParty = true;

							UnitAny* pMate = D2ASMFuncs::D2GAME_FindUnit(pUnit->pGame, pMem->dwUnitId, UNIT_PLAYER);
							if (pMate) {
								pakiet << pMate->pPlayerData->szName << '#' << pMate->pPlayerData->pClientData->AccountName << '#' << ConvertClass(pMate->dwClassId) << ':';
							}
							nPlayers++;
						}
						if (pParty->ptPrev) pakiet << '$';
						PartyCount++;
					}


				if (pUnit->pPlayerData->SaidGO) 
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Powiedziaÿeÿ juÿ to!" : "You've already said that", PartyCount);
					return false;
				}

				if (PartyCount == 0 && pUnit->pPlayerData->SaidGO == 0)
				{
					pUnit->pPlayerData->SaidGO = 1;
					pUnit->pPlayerData->isPlaying = 1;
					pUnit->pGame->nPlaying++;

					if (pUnit->pGame->nPlaying > 1)
					{
						ClearSayGoFlag(pUnit->pGame);

						for (ClientData* i = pUnit->pGame->pClientList; i; i = i->ptPrevious)
						{
							if (!i->pGame) continue;
							if (i->InitStatus != 4) continue;
							if (i->pGame == pUnit->pGame && i->pPlayerUnit->pPlayerData->isPlaying != 0)  i->pPlayerUnit->pPlayerData->CanAttack = 1;
						}
						BroadcastExEvent(pUnit->pGame, COL_RED, D2EX_PLAY, 3, -1, 200, "Runda rozpoczeta!", "Round begins!");
					}
					return true;
				}

				if (PartyCount == 2 && pUnit->pPlayerData->SaidGO == 0 && InParty)
				{
					pUnit->pPlayerData->SaidGO = 1;
					pUnit->pPlayerData->isPlaying = 1;
					pUnit->pGame->nPlaying++;

					if (pUnit->pGame->nPlaying >= nPlayers) {
						ClearSayGoFlag(pUnit->pGame);

						for (ClientData* i = pUnit->pGame->pClientList; i; i = i->ptPrevious)
						{
							if (!i->pGame) continue;
							if (i->InitStatus != 4) continue;
							if (i->pGame == pUnit->pGame && i->pPlayerUnit->pPlayerData->isPlaying != 0) i->pPlayerUnit->pPlayerData->CanAttack = 1;

						}

						BroadcastExEvent(pUnit->pGame, 1, D2EX_PLAY, 3, -1, 200, "Runda rozpoczeta!", "Round begins!");
					}
					return true;
				}
				SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Nieprawidlowa ilosc druzyn - %d " : "Incorrect party count - %d", PartyCount);

				return true;
			}
			if (_stricmp(str, "#gu") == 0)
			{
				if (Warden::getInstance().wcfgAllowGU && !pUnit->pPlayerData->isSpecing)
				{
					int aLevel = D2Funcs.D2COMMON_GetTownLevel(pUnit->dwAct);
					int aCurrLevel = D2Funcs.D2COMMON_GetLevelNoByRoom(pUnit->pPath->pRoom1);
					if (aCurrLevel != aLevel)
					{
						//Runaways feature
						Room1* mRoom = pUnit->pPath->pRoom1;
						unsigned int i = 0;
						if (mRoom->nNumClients > 1) {
							WORD unitParty = D2ASMFuncs::D2GAME_GetPartyID(pUnit);
							for (ClientData ** pClients = mRoom->pClients; i < mRoom->nNumClients; ++pClients, ++i) {
								ClientData * pClientNear = *pClients;
								if (!pClientNear || pUnit->pPlayerData->pClientData == pClientNear)
									continue;

								DEBUGMSG("Found client %s", pClientNear->CharName)
								UnitAny* pNearUnit = pClientNear->pPlayerUnit;
								if (D2GAME_IsUnitDead(pNearUnit))
									continue;
								if (unitParty != 0xFFFF && unitParty == D2ASMFuncs::D2GAME_GetPartyID(pNearUnit))
									continue;
								if (!D2ASMFuncs::D2GAME_isUnitInRange(pUnit->pGame, pUnit->dwUnitId, UNIT_PLAYER, pNearUnit, 15)) {
									LRoster::UpdateRoster(pUnit->pGame, pUnit->pPlayerData->szName, 4); // Increase runaways count
									LRosterData *pRoster = LRoster::Find(pUnit->pGame, pUnit->pPlayerData->szName);
									if (pRoster)
										LRoster::SyncClientRunaways(pUnit->pGame, pUnit->dwUnitId, pRoster);
									break;
								}
							}
						}
						
						SPECTATOR_RemoveFromQueue(pUnit->pGame, pUnit->dwUnitId);
						D2ASMFuncs::D2GAME_MoveUnitToLevelId(pUnit, aLevel, pUnit->pGame);
					}
				}
				if (pUnit->pGame->bFestivalMode == 1)
					if (pUnit->pPlayerData->isPlaying) {
						pUnit->pPlayerData->SaidGO = 0;
						pUnit->pPlayerData->CanAttack = 0;
						DoRoundEndStuff(pUnit->pGame, pUnit);
						pUnit->pPlayerData->isPlaying = 0;
						pUnit->pGame->nPlaying--;
					}

				return TRUE;
			}
			if (_stricmp(str, "#debug") == 0)
			{
				WardenClient_i ptCurrentClient = Warden::getInstance().findClientById(ClientID);
				if (ptCurrentClient == Warden::getInstance().getInvalidClient()) return TRUE;
				ptCurrentClient->DebugTrick = !ptCurrentClient->DebugTrick;
				return false;
			}
			if (_stricmp(str, "#reload") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				Warden::getInstance().loadConfig();
				SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Ustawienia przeÿadowane." : "Config reloaded.");
				return false;
			}
			if (_stricmp(str, "#reloadclans") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				Warden::getInstance().reloadClans();
				SendMsgToClient(pUnit->pPlayerData->pClientData, Warden::getInstance().clansAvailable ? "Clans reloaded successfully." : "Error in ini file!" );
				return false;
			}
			if (_stricmp(str, "#kick") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#kick <*account> or #kick [charname]!"); return false; }
				WardenClient_i psUnit = Warden::getInstance().getInvalidClient();
				if (str[0] == '*') {
					str++;
					psUnit = Warden::getInstance().findClientByAcc(str);
				}
				else
					psUnit = Warden::getInstance().findClientByName(str);

				if (psUnit == Warden::getInstance().getInvalidClient()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Wrong charname / Player is not in the game!"); return false; }
				BroadcastMsg(pUnit->pPlayerData->pClientData->pGame, "'%s' has been kicked by *%s", psUnit->CharName.c_str(), pUnit->pPlayerData->pClientData->AccountName);
				BootPlayer(psUnit->ClientID, BOOT_CONNECTION_INTERRUPTED);
				return false;
			}
			if (_stricmp(str, "#stats") == 0)
			{
				UnitAny * pDestUnit = pUnit;

				str = strtok_s(NULL, " ", &t);
				if (str)
				{
					if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

					WardenClient_i ptCurrentClient = Warden::getInstance().getInvalidClient();
					if (str[0] == '*') {
						str++;
						ptCurrentClient = Warden::getInstance().findClientByAcc(str);
					}
					else
						ptCurrentClient = Warden::getInstance().findClientByName(str);

					if (ptCurrentClient == Warden::getInstance().getInvalidClient()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!"); return false; }
					pDestUnit = ptCurrentClient->ptPlayer;
				}

				if (!pDestUnit) return false;

				int sFR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FIRERESIST, 0);
				int sCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_COLDRESIST, 0);
				int sLR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_LIGHTNINGRESIST, 0);
				int sPR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_POISONRESIST, 0);

				int sDR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_DAMAGEREDUCTION, 0);

				int sFCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERCAST, 0);
				int sFHR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERHITRECOVERY, 0);
				int sIAS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_IAS, 0);
				int sFRW = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERRUNWALK, 0);
				int sDS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_DEADLYSTRIKE, 0);
				int sOW = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_OPENWOUNDS, 0);
				int sCB = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_CRUSHINGBLOW, 0);

				int sCABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_COLDABSORBPERCENT, 0);
				int sLABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_LIGHTNINGABSORBPERCENT, 0);
				int sFABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FIREABSORBPERCENT, 0);

				int sMFR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXFIRERESIST, 0) + 75;
				int sMCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXCOLDRESIST, 0) + 75;
				int sMLR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXLIGHTNINGRESIST, 0) + 75;
				int sMPR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXPOISONRESIST, 0) + 75;

				int sMF = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAGICFIND, 0);
				int aLife = D2Funcs.D2COMMON_GetUnitMaxLife(pDestUnit) >> 8;
				int aMana = D2Funcs.D2COMMON_GetUnitMaxMana(pDestUnit) >> 8;

				int sREP = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_HPREGEN, 0);

				if (D2Funcs.D2COMMON_GetUnitState(pDestUnit, 83))  //Nat res
				{
					Skill* pSkill = D2Funcs.D2COMMON_GetSkillById(pDestUnit, 153, -1);
					if (pSkill)
					{
						int SkillLvl = D2Funcs.D2COMMON_GetSkillLevel(pDestUnit, pSkill, 1);
						int ResBonus = D2Funcs.D2COMMON_EvaluateSkill(pDestUnit, 3442, 153, SkillLvl);
						sFR -= ResBonus;
						sCR -= ResBonus;
						sLR -= ResBonus;
						sPR -= ResBonus;
					}
				}
				if (D2Funcs.D2COMMON_GetUnitState(pDestUnit, 8)) //Salv
				{
					StatList * Stats = D2Funcs.D2COMMON_GetStateStatList(pDestUnit, 8);
					int ResBonus = Stats->Stats.pStat->dwStatValue;
					sFR -= ResBonus;
					sCR -= ResBonus;
					sLR -= ResBonus;
				}

				SendMsgToClient(pUnit->pPlayerData->pClientData, "Resistances : ÿc1Fire '%d'/'%d%%', ÿc9Light '%d'/'%d%%', ÿc3Cold '%d'/'%d%%', ÿc2Poison '%d'/'%d%%'", sFR, sMFR, sLR, sMLR, sCR, sMCR, sPR, sMPR);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "FCR ÿc9'%d'ÿcc, FRW ÿc9'%d'ÿcc, FHR ÿc9'%d'ÿcc, IAS ÿc9'%d'", sFCR, sFRW, sFHR, sIAS);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "DS ÿc9'%d%%'ÿcc, OW ÿc9'%d%%'ÿcc, CB ÿc9'%d%%'ÿcc, DR ÿc9'%d%%'", sDS, sOW, sCB, sDR);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "ÿc1Fire Abs ÿc9'%d%%' ÿc3Cold Abs ÿc9'%d%%' ÿc9Light Abs ÿc9'%d%%'ÿc;, MF ÿc9'%d%%'", sFABS, sCABS, sLABS, sMF);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "ÿc1Life : '%d', ÿc3Mana : '%d', ÿccLife Replenish : ÿc9'%d'", aLife, aMana, sREP);
				return false;
			}
			if (_stricmp(str, "#move") == 0)
			{
				UnitAny* aUnit = pUnit;
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName))  return TRUE;
				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#move <levelid> [account]"); return false; }
				int LvlId = atoi(str);
				str = strtok_s(NULL, " ", &t);
				if (str) {
					WardenClient_i ptCurrentClient = Warden::getInstance().findClientByName(str);
					if (ptCurrentClient == Warden::getInstance().getInvalidClient()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!"); return false; }
					if (!ptCurrentClient->ptPlayer) { return false; }
					if (aUnit == ptCurrentClient->ptPlayer){ return false; }
					aUnit = ptCurrentClient->ptPlayer;
				}
				if (!LvlId) return false;
				if (LvlId >= (*D2Vars.D2COMMON_sgptDataTables)->dwLevelsRecs) return false;
				SendMsgToClient(aUnit->pPlayerData->pClientData, "Moving '%s' to level '%d'...", aUnit->pPlayerData->szName, LvlId);
				D2ASMFuncs::D2GAME_MoveUnitToLevelId(aUnit, LvlId, aUnit->pGame);
				return false;
			}
			if (_stricmp(str, "#moveall") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#move <levelid>"); return false; }
				int LvlId = atoi(str);
				if (!LvlId) return false;
				if (LvlId >= (*D2Vars.D2COMMON_sgptDataTables)->dwLevelsRecs) return false;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Moving '%d' clients to level '%d'...", pUnit->pPlayerData->pClientData->pGame->nClients, LvlId);
				for (ClientData* pClientList = pUnit->pPlayerData->pClientData->pGame->pClientList; pClientList; pClientList = pClientList->ptPrevious)
				{
					if (!(pClientList->InitStatus & 4))
						continue;
					D2ASMFuncs::D2GAME_MoveUnitToLevelId(pClientList->pPlayerUnit, LvlId, pClientList->pGame);
				}

				return false;
			}
			if (_stricmp(str, "#op") == 0)
			{
				UnitAny* aUnit = pUnit;
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName))  return TRUE;
				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#op <levelid> - opens a portal to levelid"); return false; }
				int LvlId = atoi(str);
				if (!LvlId) return false;
				if (LvlId >= (*D2Vars.D2COMMON_sgptDataTables)->dwLevelsRecs) return false;
				SendMsgToClient(aUnit->pPlayerData->pClientData, "Opening a portal to level '%d'...", LvlId);
				if (QUESTS_OpenPortal(pUnit->pGame, pUnit, LvlId)) {
					if (LvlId == UBER_TRISTRAM)
						pUnit->pGame->bUberQuestFlags.bOpenedTristramPortal = true;
					else if(LvlId == PANDEMONIUM_RUN_1)
						pUnit->pGame->bUberQuestFlags.bOpenedLilithPortal = true;
					else if (LvlId == PANDEMONIUM_RUN_2)
						pUnit->pGame->bUberQuestFlags.bOpenedDurielPortal = true;
					else if (LvlId == PANDEMONIUM_RUN_3)
						pUnit->pGame->bUberQuestFlags.bOpenedIzualPortal = true;

				}
				return false;
			}
#ifdef ENABLE_LEVEL_COMMAND
			if (Warden::getInstance().wcfgEnableLevelCmd)
			if (_stricmp(str, "#level") == 0 || _stricmp(str, "#lvl") == 0)
			{
				if (D2Funcs.D2COMMON_GetStatSigned(pUnit, STAT_LEVEL, NULL) != 1)
					return TRUE;
			
				D2Funcs.D2COMMON_SetStat(pUnit, STAT_LEVEL, 99, 0); // Free skills
				D2Funcs.D2COMMON_SetStat(pUnit, 5, 110, 0);
				D2Funcs.D2COMMON_SetStat(pUnit, STAT_EXP, 3520485254, 0);
				D2Funcs.D2COMMON_SetStat(pUnit, 4, 505, 0); // Stat points
				D2Funcs.D2COMMON_AddStat(pUnit, STAT_MAXHP, 60 << 8, 0); // 
				DWORD ClassId = pUnit->dwClassId;
				CharStatsTxt* pTxt = &(*D2Vars.D2COMMON_sgptDataTables)->pCharStatsTxt[ClassId];
				if (pTxt) {
					D2Funcs.D2COMMON_AddStat(pUnit, STAT_MAXSTAMINA, ((pTxt->bStaminaPerLevel * 98) / 4) << 8, 0);
					D2Funcs.D2COMMON_AddStat(pUnit, STAT_MAXMANA, ((pTxt->bManaPerLevel * 98) / 4) << 8, 0);
					D2Funcs.D2COMMON_AddStat(pUnit, STAT_MAXHP, ((pTxt->bLifePerLevel * 98) / 4) << 8, 0);
				}
				for (int diff = 0; diff < 3; ++diff) {
					for (int lvl = 1; lvl < (*D2Vars.D2COMMON_sgptDataTables)->dwLevelsRecs; ++lvl) {
						LevelsTxt * pLevelTxt = &(*D2Vars.D2COMMON_sgptDataTables)->pLevelsTxt[lvl];
						if (pLevelTxt && pLevelTxt->nWaypoint != 255) {
							D2Funcs.D2COMMON_EnableWaypoint(pUnit->pPlayerData->pWaypoints[diff], pLevelTxt->nWaypoint);
						}
					}
						for (int i = 0; i < 41; ++i) {
							D2Funcs.D2COMMON_SetQuestFlag(pUnit->pPlayerData->QuestsFlags[diff], i, 0);
							D2Funcs.D2COMMON_SetQuestFlag(pUnit->pPlayerData->QuestsFlags[diff], i, 13);
						}
				}
					
				D2Funcs.D2COMMON_SetStat(pUnit, STAT_GOLD, 500 * 1000, 0); //Gold
				D2Funcs.D2COMMON_SetStat(pUnit, STAT_GOLDBANK, 800 * 1000, 0); //Gold
				D2ASMFuncs::D2GAME_UpdateBonuses(pUnit);

				QUESTS_UpdateUnit(pUnit, 2, pUnit);
				D2Funcs.D2COMMON_SetStartFlags(pUnit, 1);

				return false;
			}
#endif
			if (_stricmp(str, "#removepets") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Removing your pets...");
				D2ASMFuncs::D2GAME_RemovePets(pUnit->pGame, pUnit);
				return false;
			}
			if (_stricmp(str, "#interinfo") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "I? %d, Id: %d, Type %d", pUnit->bInteracting, pUnit->dwInteractId, pUnit->dwInteractType);
				return false;
			}
			if (_stricmp(str, "#checkroom") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				
				for (int a = 0; a < D2ACT_V; ++a) {
					if (pUnit->dwAct == a) continue;
					if (!pUnit->pGame->pDrlgAct[a]) {
						SendMsgToClient(pUnit->pPlayerData->pClientData, "Skipping A%d because is not initialized!", a+1);
						continue;
					}
					for (InactiveRoom* room = pUnit->pGame->pDrlgRoomList[a]; room; room = room->pNextRoom) {
						if (room->dwXStart <= pUnit->pPath->pRoom1->pRoom2->dwPosX && room->dwYStart <= pUnit->pPath->pRoom1->pRoom2->dwPosY &&
							(room->dwXStart + 48 * 5) >= pUnit->pPath->pRoom1->pRoom2->dwPosX && (room->dwYStart + 48 * 5) >= pUnit->pPath->pRoom1->pRoom2->dwPosY)
							SendMsgToClient(pUnit->pPlayerData->pClientData, "Possible room found @ A%d", a+1);
						}
					}

				return false;
			}

			if (_stricmp(str, "#setstate") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Usage: #setstate <stateid> <1,0>"); return false; }
				DWORD nState = atoi(str);
				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Usage: #setstate <stateid> <1,0>"); return false; }
				int nHowSet = atoi(str);

				if (nState == 0 || nState > (*D2Vars.D2COMMON_sgptDataTables)->dwStatesRecs)
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, "State '%d' is out of range (%d)...", nState, (*D2Vars.D2COMMON_sgptDataTables)->dwStatesRecs);
					return false;
				}
				if (nHowSet > 1)
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, "Unknown state's state (%d). Only 1 or 0 are allowed!", nHowSet);
					return false;
				}
				D2Funcs.D2COMMON_SetGfxState(pUnit, nState, nHowSet);

				SendMsgToClient(pUnit->pPlayerData->pClientData, "State %d has been set!", nState);
				return false;
			}
		}
	}
	return TRUE;
}

void GAME_EmptyExtendedMemory(Game* pGame)
{
	BEGINDEBUGMSG("Clearing memory of extended Game...");
	memset((BYTE*)pGame + 0x1DF4, 0, sizeof(Game) - 0x1DF4);
	FINISHDEBUGMSG("ok!");
	DEBUGMSG("Initing the PlayerObservers list")
	pGame->pSpectators = new list<PlayerObservation>();
}