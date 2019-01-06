/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "pickup.h"

#include <game/server/teams.h>

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Layer, int Number)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Type = Type;
	m_Subtype = SubType;
	m_ProximityRadius = PickupPhysSize;

	m_Layer = Layer;
	m_Number = Number;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_SpawnTick[i] = -1;
}

void CPickup::Tick()
{
	//Move();

	// wait for respawn
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameServer()->m_apPlayers[i] || !GameServer()->m_apPlayers[i]->GetCharacter())
		{
			m_SpawnTick[i] = -1;
		}
		else if(m_SpawnTick[i] > 0 && Server()->Tick() > m_SpawnTick[i])
		{
			// respawn
			m_SpawnTick[i] = -1;

			if(m_Type == POWERUP_WEAPON)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, GameServer()->m_apPlayers[i]->GetCharacter()->Teams()->TeamMask(GameServer()->m_apPlayers[i]->GetCharacter()->Team()));
		}
	}

	// Check if a player intersected us
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i) {
		CCharacter * pChr = apEnts[i];
		if(pChr && pChr->IsAlive())
		{
			//if(m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]) continue;

			if(m_SpawnTick[pChr->GetPlayer()->GetCID()] != -1)
				continue;

			// player picked us up, is someone was hooking us, let them go
			int RespawnTime = -1;
			bool Sound = false;
			switch (m_Type)
			{
				case POWERUP_HEALTH:
					if(pChr->IncreaseHealth(1))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->Teams()->TeamMask(pChr->Team()));
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					}
					break;

				case POWERUP_ARMOR:
					if(g_Config.m_SvHealthAndAmmo)
					{
						if(pChr->IncreaseArmor(1))
						{
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
							RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
						}
					}
					else
					{
						if(pChr->Team() == TEAM_SUPER) continue;
						for(int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
						{
							if(pChr->GetWeaponGot(i))
							{
								if(!(pChr->m_FreezeTime && i == WEAPON_NINJA))
								{
									pChr->SetWeaponGot(i, false);
									pChr->SetWeaponAmmo(i, 0);
									Sound = true;
								}
							}
						}
						pChr->SetNinjaActivationDir(vec2(0,0));
						pChr->SetNinjaActivationTick(-500);
						pChr->SetNinjaCurrentMoveTime(0);
						if (Sound)
						{
							pChr->SetLastWeapon(WEAPON_GUN);
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
						}
						if(!pChr->m_FreezeTime && pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
							pChr->SetActiveWeapon(WEAPON_HAMMER);
					}
					break;

				case POWERUP_WEAPON:

					if (m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || (pChr->GetWeaponAmmo(m_Subtype) != -1 && pChr->GetWeaponAmmo(m_Subtype) != g_pData->m_Weapons.m_aId[m_Subtype].m_Maxammo && !pChr->m_FreezeTime)))
					{
						pChr->GiveWeapon(m_Subtype);

						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if (m_Subtype == WEAPON_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->Teams()->TeamMask(pChr->Team()));
						else if (m_Subtype == WEAPON_SHOTGUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team()));
						else if (m_Subtype == WEAPON_RIFLE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team()));

						if (pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);

					}
					break;

			case POWERUP_NINJA:
				{
					// activate ninja on target player
					pChr->GiveNinja();
					//RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

					/*// loop through all players, setting their emotes
					CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
					for(; pC; pC = (CCharacter *)pC->TypeNext())
					{
						if (pC != pChr)
							pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
					}*/
					break;
				}
				default:
					break;
			};

			if(RespawnTime >= 0 && g_Config.m_SvHealthAndAmmo)
				m_SpawnTick[pChr->GetPlayer()->GetCID()] = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}
}

void CPickup::TickPaused()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_SpawnTick[i] != -1)
			++m_SpawnTick[i];
}

void CPickup::Snap(int SnappingClient)
{
	int ClientID = SnappingClient;
	if(SnappingClient > -1 && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1
				|| GameServer()->m_apPlayers[SnappingClient]->IsPaused())
			&& GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID != SPEC_FREEVIEW)
		ClientID = GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID;
	if(m_SpawnTick[ClientID] != -1)
		return;

	/*int Tick = (Server()->Tick()%Server()->TickSpeed())%11;
	if (Char && Char->IsAlive() &&
			(m_Layer == LAYER_SWITCH &&
					!GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Char->Team()])
					&& (!Tick))
		return;*/

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	pP->m_Subtype = m_Subtype;
}

void CPickup::Move()
{
	if (Server()->Tick()%int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = GameServer()->Collision()->IsMover(m_Pos.x,m_Pos.y, &Flags);
		if (index)
		{
			m_Core=GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}
