/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/sorted_array.h>

#include <engine/demo.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/gamecore.h> // get_angle
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/render.h>

#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/effects.h>
#include <game/client/components/sounds.h>
#include <game/client/components/controls.h>

#include <engine/textrender.h>

#include "players.h"
#include <stdio.h>

void CPlayers::RenderHand(CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset)
{
	vec2 HandPos = CenterPos + Dir;
	float Angle = GetAngle(Dir);
	if(Dir.x < 0)
		Angle -= AngleOffset;
	else
		Angle += AngleOffset;

	vec2 DirX = Dir;
	vec2 DirY(-Dir.y,Dir.x);

	if(Dir.x < 0)
		DirY = -DirY;

	HandPos += DirX * PostRotOffset.x;
	HandPos += DirY * PostRotOffset.y;

	//Graphics()->TextureSet(data->m_aImages[IMAGE_CHAR_DEFAULT].id);
	Graphics()->TextureSet(pInfo->m_Texture);
	Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, pInfo->m_ColorBody.a);

	// two passes
	for(int i = 0; i < 2; i++)
	{
		int QuadOffset = NUM_WEAPONS * 2 + i;
		Graphics()->QuadsSetRotation(Angle);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HandPos.x, HandPos.y);
	}

}

inline float NormalizeAngular(float f)
{
	return fmod(f+pi*2, pi*2);
}

inline float AngularMixDirection (float Src, float Dst) { return sinf(Dst-Src) >0?1:-1; }
inline float AngularDistance(float Src, float Dst) { return asinf(sinf(Dst-Src)); }

inline float AngularApproach(float Src, float Dst, float Amount)
{
	float d = AngularMixDirection (Src, Dst);
	float n = Src + Amount*d;
	if(AngularMixDirection (n, Dst) != d)
		return Dst;
	return n;
}

void CPlayers::Predict(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPrevInfo,
	const CNetObj_PlayerInfo *pPlayerInfo,
	vec2 &PrevPredPos,
	vec2 &SmoothPos,
	int &MoveCnt,
	vec2 &Position
	)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CNetObj_PlayerInfo pInfo = *pPlayerInfo;


	// set size

	float IntraTick = Client()->IntraGameTick();


	//float angle = 0;

	if(pInfo.m_Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's local player we are rendering
	}
	else
	{
		/*
		float mixspeed = Client()->FrameTime()*2.5f;
		if(player.attacktick != prev.attacktick) // shooting boosts the mixing speed
			mixspeed *= 15.0f;

		// move the delta on a constant speed on a x^2 curve
		float current = g_GameClient.m_aClients[info.cid].angle;
		float target = player.angle/256.0f;
		float delta = angular_distance(current, target);
		float sign = delta < 0 ? -1 : 1;
		float new_delta = delta - 2*mixspeed*sqrt(delta*sign)*sign + mixspeed*mixspeed;

		// make sure that it doesn't vibrate when it's still
		if(fabs(delta) < 2/256.0f)
			angle = target;
		else
			angle = angular_approach(current, target, fabs(delta-new_delta));

		g_GameClient.m_aClients[info.cid].angle = angle;*/
	}

	vec2 NonPredPos = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	// use preditect players if needed
	if(g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
		{
			// apply predicted results
			m_pClient->m_aClients[pInfo.m_ClientID].m_Predicted.Write(&Player);
			m_pClient->m_aClients[pInfo.m_ClientID].m_PrevPredicted.Write(&Prev);

			IntraTick = Client()->PredIntraGameTick();
		}
	}

	Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);


	static double s_Ping = 0;

	if(pInfo.m_Local) {
		s_Ping = mix(s_Ping, (double)pInfo.m_Latency, 0.1);
	}

	if(!pInfo.m_Local)
	{
		/*
		for ping = 260, usual missprediction distances:

		move = 120-140
		jump = 130
		dj = 250

		normalized:
		move = 0.461 - 0.538
		jump = 0.5
		dj = .961

		*/
		//printf("%d\n", m_pClient->m_Snap.m_pLocalInfo->m_Latency);


		if(m_pClient->m_Snap.m_pLocalInfo)
			s_Ping = mix(s_Ping, (double)m_pClient->m_Snap.m_pLocalInfo->m_Latency, 0.1);

		double d = length(PrevPredPos - Position)/s_Ping;

		if((d > 0.4) && (d < 5.))
		{
//			if(MoveCnt == 0)
//				printf("[\n");
			if(MoveCnt == 0)
				SmoothPos = NonPredPos;

			MoveCnt = 10;
//		SmoothPos = PrevPredPos;
//		SmoothPos = mix(NonPredPos, Position, 0.6);
		}

		PrevPredPos = Position;

		if(MoveCnt > 0)
		{
//		Position = mix(mix(NonPredPos, Position, 0.5), SmoothPos, (((float)MoveCnt))/15);
//		Position = mix(mix(NonPredPos, Position, 0.5), SmoothPos, 0.5);
			Position = mix(NonPredPos, Position, 0.5);

			SmoothPos = Position;
			MoveCnt--;
//		if(MoveCnt == 0)
//			printf("]\n\n");
		}
	}
}

void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID,
	const vec2 &parPosition,
	const vec2 &PositionTo,
	float Intra
	)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	bool AntiPingPlayers = m_pClient->AntiPingPlayers();
	bool Local = m_pClient->m_Snap.m_LocalClientID == ClientID;

	// don't render hooks to not active character cores
	if(pPlayerChar->m_HookedPlayer != -1 && !m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Active)
		return;

	float IntraTick = Client()->IntraGameTick();
	if(ClientID < 0)
	{
		IntraTick = Intra;
		AntiPingPlayers = false;
	}

	bool OtherTeam;

	if((m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team == TEAM_SPECTATORS && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW) || ClientID < 0)
		OtherTeam = false;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		OtherTeam = m_pClient->m_Teams.Team(ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);
	else
		OtherTeam = m_pClient->m_Teams.Team(ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientID);

	if(OtherTeam)
	{
		RenderInfo.m_ColorBody.a = g_Config.m_ClShowOthersAlpha / 100.0f;
		RenderInfo.m_ColorFeet.a = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	RenderInfo.m_Size = 64.0f;

	if(!AntiPingPlayers)
	{
		// use predicted players if needed
		if(Local && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(!m_pClient->m_Snap.m_pLocalCharacter || (m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
			}
			else
			{
				// apply predicted results
				m_pClient->m_PredictedChar.Write(&Player);
				m_pClient->m_PredictedPrevChar.Write(&Prev);
				IntraTick = Client()->PredIntraGameTick();
			}
		}
	}
	else
	{
		if(g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
				// apply predicted results
				m_pClient->m_aClients[ClientID].m_Predicted.Write(&Player);
				m_pClient->m_aClients[ClientID].m_PrevPredicted.Write(&Prev);

				IntraTick = Client()->PredIntraGameTick();
			}
		}
	}

	vec2 Position;
	if(!AntiPingPlayers)
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	else
		Position = parPosition;

	// draw hook
	if(Prev.m_HookState>0 && Player.m_HookState>0)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		if(ClientID < 0)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		vec2 Pos = Position;
		vec2 HookPos;

		if(!AntiPingPlayers)
		{
			if(pPlayerChar->m_HookedPlayer != -1)
			{
				if(m_pClient->m_Snap.m_LocalClientID != -1 && pPlayerChar->m_HookedPlayer == m_pClient->m_Snap.m_LocalClientID && !m_pClient->m_Snap.m_SpecInfo.m_Active)
				{
					if(Client()->State() == IClient::STATE_DEMOPLAYBACK) // only use prediction if needed
						HookPos = vec2(m_pClient->m_LocalCharacterPos.x, m_pClient->m_LocalCharacterPos.y);
					else
						HookPos = mix(vec2(m_pClient->m_PredictedPrevChar.m_Pos.x, m_pClient->m_PredictedPrevChar.m_Pos.y),
							vec2(m_pClient->m_PredictedChar.m_Pos.x, m_pClient->m_PredictedChar.m_Pos.y), Client()->PredIntraGameTick());
				}
				else if(Local)
				{
					HookPos = mix(vec2(m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Prev.m_X,
						m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Prev.m_Y),
						vec2(m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Cur.m_X,
						m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Cur.m_Y),
						Client()->IntraGameTick());
				}
				else
					HookPos = mix(vec2(pPrevChar->m_HookX, pPrevChar->m_HookY), vec2(pPlayerChar->m_HookX, pPlayerChar->m_HookY), Client()->IntraGameTick());
			}
			else
				HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);
		}
		else
		{
			if(pPrevChar->m_HookedPlayer != -1)
				HookPos = PositionTo;
			else
				HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);
		}

		float d = distance(Pos, HookPos);
		vec2 Dir = normalize(Pos-HookPos);

		Graphics()->QuadsSetRotation(GetAngle(Dir)+pi);
		// render head
		int QuadOffset = NUM_WEAPONS * 2 + 2;
		if(OtherTeam)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookPos.x, HookPos.y);

		// render chain
		++QuadOffset;
		static IGraphics::SRenderSpriteInfo s_HookChainRenderInfo[1024];
		int HookChainCount = 0;
		for(float f = 24; f < d && HookChainCount < 1024; f += 24, ++HookChainCount)
		{
			vec2 p = HookPos + Dir*f;
			s_HookChainRenderInfo[HookChainCount].m_Pos[0] = p.x;
			s_HookChainRenderInfo[HookChainCount].m_Pos[1] = p.y;

			s_HookChainRenderInfo[HookChainCount].m_Scale = 1;
			s_HookChainRenderInfo[HookChainCount].m_Rotation = GetAngle(Dir) + pi;
			++HookChainCount;
		}
		Graphics()->RenderQuadContainerAsSpriteMultiple(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookChainCount, s_HookChainRenderInfo);

		Graphics()->QuadsSetRotation(0);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		RenderHand(&RenderInfo, Position, normalize(HookPos-Pos), -pi/2, vec2(20, 0));
	}
}

void CPlayers::RenderPlayer(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID,
	const vec2 &parPosition,
	float Intra
/*	vec2 &PrevPos,
	vec2 &SmoothPos,
	int &MoveCnt
*/	)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	bool AntiPingPlayers = m_pClient->AntiPingPlayers();
	bool Local = m_pClient->m_Snap.m_LocalClientID == ClientID;

	bool NewTick = m_pClient->m_NewTick;

	bool OtherTeam;

	if((m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team == TEAM_SPECTATORS && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW) || ClientID < 0)
		OtherTeam = false;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		OtherTeam = m_pClient->m_Teams.Team(ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);
	else
		OtherTeam = m_pClient->m_Teams.Team(ClientID) != m_pClient->m_Teams.Team(m_pClient->m_Snap.m_LocalClientID);

	// set size
	RenderInfo.m_Size = 64.0f;

	float IntraTick = Client()->IntraGameTick();
	if(ClientID < 0)
	{
		IntraTick = Intra;
		AntiPingPlayers = false;
	}

	float Angle;
	if(Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's the local player we are rendering
		Angle = GetAngle(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy]);
	}
	else
	{
		// If the player moves their weapon through top, then change
		// the end angle by 2*Pi, so that the mix function will use the
		// short path and not the long one.
		if(Player.m_Angle > (256.0f * pi) && Prev.m_Angle < 0)
		{
			Player.m_Angle -= 256.0f * 2 * pi;
			Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick) / 256.0f;
		}
		else if(Player.m_Angle < 0 && Prev.m_Angle > (256.0f * pi))
		{
			Player.m_Angle += 256.0f * 2 * pi;
			Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick) / 256.0f;
		}
		else
		{
			// No special cases? Just use mix():
			Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick)/256.0f;
		}
	}


	// use preditect players if needed
	if(!AntiPingPlayers)
	{
		if(Local && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(!m_pClient->m_Snap.m_pLocalCharacter || (m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
			}
			else
			{
				// apply predicted results
				m_pClient->m_PredictedChar.Write(&Player);
				m_pClient->m_PredictedPrevChar.Write(&Prev);
				IntraTick = Client()->PredIntraGameTick();
				NewTick = m_pClient->m_NewPredictedTick;
			}
		}
	}
	else
	{
		if(g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
				// apply predicted results
				m_pClient->m_aClients[ClientID].m_Predicted.Write(&Player);
				m_pClient->m_aClients[ClientID].m_PrevPredicted.Write(&Prev);

				IntraTick = Client()->PredIntraGameTick();
				NewTick = m_pClient->m_NewPredictedTick;
			}
		}
	}

	vec2 Direction = GetDirection((int)(Angle*256.0f));
	vec2 Position;
	if(!AntiPingPlayers)
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	else
		Position = parPosition;
	vec2 Vel = mix(vec2(Prev.m_VelX/256.0f, Prev.m_VelY/256.0f), vec2(Player.m_VelX/256.0f, Player.m_VelY/256.0f), IntraTick);

	m_pClient->m_pFlow->Add(Position, Vel*100.0f, 10.0f);

	RenderInfo.m_GotAirJump = Player.m_Jumped&2?0:1;


	// detect events
	if(NewTick)
	{
		// detect air jump
		if(!RenderInfo.m_GotAirJump && !(Prev.m_Jumped&2))
			m_pClient->m_pEffects->AirJump(Position);
	}

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y+16);
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);

	// evaluate animation
	float WalkTime = fmod(absolute(Position.x), 100.0f)/100.0f;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f); // TODO: some sort of time here
	else if(Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f); // TODO: some sort of time here
	else if(!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	static float s_LastGameTickTime = Client()->GameTickTime();
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime();
	if(Player.m_Weapon == WEAPON_HAMMER)
	{
		float ct = (Client()->PrevGameTick()-Player.m_AttackTick)/(float)SERVER_TICK_SPEED + s_LastGameTickTime;
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], clamp(ct*5.0f,0.0f,1.0f), 1.0f);
	}
	if(Player.m_Weapon == WEAPON_NINJA)
	{
		float ct = (Client()->PrevGameTick()-Player.m_AttackTick)/(float)SERVER_TICK_SPEED + s_LastGameTickTime;
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], clamp(ct*2.0f,0.0f,1.0f), 1.0f);
	}

	// do skidding
	if(!InAir && WantOtherDir && length(Vel*50) > 500.0f)
	{
		static int64 SkidSoundTime = 0;
		if(time_get()-SkidSoundTime > time_freq()/10)
		{
			if(g_Config.m_SndGame)
				m_pClient->m_pSounds->PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, 0.25f, Position);
			SkidSoundTime = time_get();
		}

		m_pClient->m_pEffects->SkidTrail(
			Position+vec2(-Player.m_Direction*6,12),
			vec2(-Player.m_Direction*100*length(Vel),-50)
		);
	}

	// draw gun
	{
		CServerInfo Info;
		Client()->GetServerInfo(&Info);
		if(ClientID >= 0 && (((IsDDRace(&Info) || IsDDNet(&Info)) && g_Config.m_ClShowHookCollAlways) || (Player.m_PlayerFlags&PLAYERFLAG_AIM && ((!Local && g_Config.m_ClShowHookCollOther) || (Local && g_Config.m_ClShowHookCollOwn)))))
		{
			float Alpha = 1.0f;
			if(OtherTeam)
				Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;

			vec2 ExDirection = Direction;

			if(Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
				ExDirection = normalize(vec2(m_pClient->m_pControls->m_InputData[g_Config.m_ClDummy].m_TargetX, m_pClient->m_pControls->m_InputData[g_Config.m_ClDummy].m_TargetY));

			Graphics()->TextureSet(-1);
			vec2 InitPos = Position;
			vec2 FinishPos = InitPos + ExDirection * (m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength-42.0f);

			Graphics()->LinesBegin();
			Graphics()->SetColor(1.00f, 0.0f, 0.0f, Alpha);

			float PhysSize = 28.0f;

			vec2 OldPos = InitPos + ExDirection * PhysSize * 1.5f;
			vec2 NewPos = OldPos;

			bool DoBreak = false;
			int Hit = 0;

			do {
				OldPos = NewPos;
				NewPos = OldPos + ExDirection * m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookFireSpeed;

				if(distance(InitPos, NewPos) > m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength)
				{
					NewPos = InitPos + normalize(NewPos-InitPos) * m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength;
					DoBreak = true;
				}

				int TeleNr = 0;
				Hit = Collision()->IntersectLineTeleHook(OldPos, NewPos, &FinishPos, 0x0, &TeleNr);

				if(!DoBreak && Hit) {
					if(Hit != TILE_NOHOOK)
						Graphics()->SetColor(130.0f/255.0f, 232.0f/255.0f, 160.0f/255.0f, Alpha);
				}

				if(m_pClient->m_Tuning[g_Config.m_ClDummy].m_PlayerHooking && m_pClient->IntersectCharacter(OldPos, FinishPos, FinishPos, ClientID) != -1)
				{
					Graphics()->SetColor(1.0f, 1.0f, 0.0f, Alpha);
					break;
				}

				if(Hit)
					break;

				NewPos.x = round_to_int(NewPos.x);
				NewPos.y = round_to_int(NewPos.y);

				if(OldPos == NewPos)
					break;

				ExDirection.x = round_to_int(ExDirection.x*256.0f) / 256.0f;
				ExDirection.y = round_to_int(ExDirection.y*256.0f) / 256.0f;
			} while (!DoBreak);

			IGraphics::CLineItem LineItem(InitPos.x, InitPos.y, FinishPos.x, FinishPos.y);
			Graphics()->LinesDraw(&LineItem, 1);
			Graphics()->LinesEnd();
		}

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle*pi*2+Angle);

		if(ClientID < 0)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		// normal weapons
		int iw = clamp(Player.m_Weapon, 0, NUM_WEAPONS-1);
		int QuadOffset = iw * 2 + (Direction.x < 0 ? 1 : 0);

		if(OtherTeam)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);

		vec2 Dir = Direction;
		float Recoil = 0.0f;
		vec2 p;
		if(Player.m_Weapon == WEAPON_HAMMER)
		{
			// Static position for hammer
			p = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
			// if attack is under way, bash stuffs
			if(Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi/2-State.GetAttach()->m_Angle*pi*2);
				p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi/2+State.GetAttach()->m_Angle*pi*2);
			}
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);
		}
		else if(Player.m_Weapon == WEAPON_NINJA)
		{
			p = Position;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;

			if(Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi/2-State.GetAttach()->m_Angle*pi*2);
				p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
				m_pClient->m_pEffects->PowerupShine(p+vec2(32,0), vec2(32,12));
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi/2+State.GetAttach()->m_Angle*pi*2);
				m_pClient->m_pEffects->PowerupShine(p-vec2(32,0), vec2(32,12));
			}
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);

			// HADOKEN
			if((Client()->GameTick()-Player.m_AttackTick) <= (SERVER_TICK_SPEED / 6) && g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles)
			{
				int IteX = rand() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
				{
					const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
					if(pInfo->m_Paused)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				else
				{
					if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				if(g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
				{
					vec2 Dir = vec2(pPlayerChar->m_X,pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y);
					Dir = normalize(Dir);
					float HadOkenAngle = GetAngle(Dir);
					Graphics()->QuadsSetRotation(HadOkenAngle);
					int QuadOffset = IteX * 2;
					vec2 DirY(-Dir.y,Dir.x);
					p = Position;
					float OffsetX = g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx;
					p -= Dir * OffsetX;
					Graphics()->RenderQuadContainerAsSprite(m_WeaponSpriteMuzzleQuadContainerIndex[iw], QuadOffset, p.x, p.y);
				}
			}
		}
		else
		{
			// TODO: should be an animation
			Recoil = 0;
			static float s_LastIntraTick = IntraTick;
			if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
				s_LastIntraTick = IntraTick;

			float a = (Client()->GameTick()-Player.m_AttackTick+s_LastIntraTick)/5.0f;
			if(a < 1)
				Recoil = sinf(a*pi);
			p = Position + Dir * g_pData->m_Weapons.m_aId[iw].m_Offsetx - Dir*Recoil*10.0f;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
			if(Player.m_Weapon == WEAPON_GUN && g_Config.m_ClOldGunPosition)
				p.y -= 8;
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);
		}

		if(Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
		{
			// check if we're firing stuff
			if(g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles)//prev.attackticks)
			{
				float Alpha = 0.0f;
				int Phase1Tick = (Client()->GameTick() - Player.m_AttackTick);
				if(Phase1Tick < (g_pData->m_Weapons.m_aId[iw].m_Muzzleduration + 3))
				{
					float t = ((((float)Phase1Tick) + IntraTick)/(float)g_pData->m_Weapons.m_aId[iw].m_Muzzleduration);
					Alpha = mix(2.0f, 0.0f, min(1.0f,max(0.0f,t)));
				}

				int IteX = rand() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
				{
					const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
					if(pInfo->m_Paused)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				else
				{
					if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
						IteX = s_LastIteX;
					else
						s_LastIteX = IteX;
				}
				if(Alpha > 0.0f && g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
				{
					float OffsetY = -g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsety;
					int QuadOffset = IteX * 2 + (Direction.x < 0 ? 1 : 0);
					if(Direction.x < 0)
						OffsetY = -OffsetY;

					vec2 DirY(-Dir.y,Dir.x);
					vec2 MuzzlePos = p + Dir * g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx + DirY * OffsetY;
					Graphics()->RenderQuadContainerAsSprite(m_WeaponSpriteMuzzleQuadContainerIndex[iw], QuadOffset, MuzzlePos.x, MuzzlePos.y);
				}
			}
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);

		if(OtherTeam)
		{
			RenderInfo.m_ColorBody.a = g_Config.m_ClShowOthersAlpha / 100.0f;
			RenderInfo.m_ColorFeet.a = g_Config.m_ClShowOthersAlpha / 100.0f;
		}

		switch (Player.m_Weapon)
		{
			case WEAPON_GUN: RenderHand(&RenderInfo, p, Direction, -3*pi/4, vec2(-15, 4)); break;
			case WEAPON_SHOTGUN: RenderHand(&RenderInfo, p, Direction, -pi/2, vec2(-5, 4)); break;
			case WEAPON_GRENADE: RenderHand(&RenderInfo, p, Direction, -pi/2, vec2(-4, 7)); break;
		}

	}

	// render the "shadow" tee
	if(Local && (g_Config.m_Debug || g_Config.m_ClUnpredictedShadow))
	{
		vec2 GhostPosition = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), Client()->IntraGameTick());
		CTeeRenderInfo Ghost = RenderInfo;
		Ghost.m_ColorBody.a = 0.5f;
		Ghost.m_ColorFeet.a = 0.5f;
		RenderTools()->RenderTee(&State, &Ghost, Player.m_Emote, Direction, GhostPosition, true); // render ghost
	}

	RenderInfo.m_Size = 64.0f; // force some settings

	if(OtherTeam)
	{
		RenderInfo.m_ColorBody.a = g_Config.m_ClShowOthersAlpha / 100.0f;
		RenderInfo.m_ColorFeet.a = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetRotation(0);
	if(g_Config.m_ClShowDirection && ClientID >= 0 && (!Local || DemoPlayer()->IsPlaying()))
	{
		if(Player.m_Direction == -1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			if(OtherTeam)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
			Graphics()->QuadsSetRotation(pi);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x - 30.f, Position.y - 70.f);
		}
		else if(Player.m_Direction == 1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			if(OtherTeam)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x + 30.f, Position.y - 70.f);
		}
		if(Player.m_Jumped&1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			if(OtherTeam)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
			Graphics()->QuadsSetRotation(pi * 3 / 2);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x, Position.y - 70.f);
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position, OtherTeam || ClientID < 0);
	
	int QuadOffsetToEmoticon = NUM_WEAPONS * 2 + 2 + 2;
	if(Player.m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
		int QuadOffset = QuadOffsetToEmoticon + (SPRITE_DOTDOT - SPRITE_OOP);
		if(OtherTeam)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(ClientID < 0)
		return;

	if(g_Config.m_ClShowEmotes && m_pClient->m_aClients[ClientID].m_EmoticonStart != -1 && m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() > Client()->GameTick())
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);

		int SinceStart = Client()->GameTick() - m_pClient->m_aClients[ClientID].m_EmoticonStart;
		int FromEnd = m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() - Client()->GameTick();

		float a = 1;

		if(FromEnd < Client()->GameTickSpeed() / 5)
			a = FromEnd / (Client()->GameTickSpeed() / 5.0);

		float h = 1;
		if(SinceStart < Client()->GameTickSpeed() / 10)
			h = SinceStart / (Client()->GameTickSpeed() / 10.0);

		float Wiggle = 0;
		if(SinceStart < Client()->GameTickSpeed() / 5)
			Wiggle = SinceStart / (Client()->GameTickSpeed() / 5.0);

		float WiggleAngle = sinf(5*Wiggle);

		Graphics()->QuadsSetRotation(pi/6*WiggleAngle);

		Graphics()->SetColor(1.0f,1.0f,1.0f,a);
		if(OtherTeam)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * (float)g_Config.m_ClShowOthersAlpha / 100.0f);
		// client_datas::emoticon is an offset from the first emoticon
		int QuadOffset = QuadOffsetToEmoticon + m_pClient->m_aClients[ClientID].m_Emoticon;
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f*h) / 64.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}
}

void CPlayers::OnRender()
{
	// update RenderInfo for ninja
	bool IsTeamplay = false;
	if(m_pClient->m_Snap.m_pGameInfoObj)
		IsTeamplay = (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS) != 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aRenderInfo[i] = m_pClient->m_aClients[i].m_RenderInfo;
		if(m_pClient->m_Snap.m_aCharacters[i].m_Cur.m_Weapon == WEAPON_NINJA && g_Config.m_ClShowNinja)
		{
			// change the skin for the player to the ninja
			int Skin = m_pClient->m_pSkins->Find("x_ninja");
			if(Skin != -1)
			{
				if(IsTeamplay)
					m_aRenderInfo[i].m_Texture = m_pClient->m_pSkins->Get(Skin)->m_ColorTexture;
				else
				{
					m_aRenderInfo[i].m_Texture = m_pClient->m_pSkins->Get(Skin)->m_OrgTexture;
					m_aRenderInfo[i].m_ColorBody = vec4(1,1,1,1);
					m_aRenderInfo[i].m_ColorFeet = vec4(1,1,1,1);
				}
			}
		}
	}

	static vec2 PrevPos[MAX_CLIENTS];
	static vec2 SmoothPos[MAX_CLIENTS];
	static int MoveCnt[MAX_CLIENTS] = {0};

	static int predcnt = 0;

	if(m_pClient->AntiPingPlayers())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
				continue;
			const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, i);
			const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

			if(pPrevInfo && pInfo)
			{
				CNetObj_Character PrevChar = m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				CNetObj_Character CurChar = m_pClient->m_Snap.m_aCharacters[i].m_Cur;

				Predict(
						&PrevChar,
						&CurChar,
						(const CNetObj_PlayerInfo *)pPrevInfo,
						(const CNetObj_PlayerInfo *)pInfo,
						PrevPos[i],
						SmoothPos[i],
						MoveCnt[i],
						m_CurPredictedPos[i]
					);
			}
		}

		if(m_pClient->AntiPingPlayers() && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
			if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			{
	//			double ping = m_pClient->m_Snap.m_pLocalInfo->m_Latency;
	//			static double fps;
	//			fps = mix(fps, (1. / Client()->RenderFrameTime()), 0.1);

	//			int predmax = (fps * ping / 1000.);

				int predmax = 19;
	//			if( 0 <= predmax && predmax <= 100)
						predcnt = (predcnt + 1) % predmax;
	//			else
	//			    predcnt = (predcnt + 1) % 2;
			}
	}

	// render other players in two passes, first pass we render the other, second pass we render our self
	for(int p = 0; p < 4; p++)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			// only render active characters
			if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
				continue;

			const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, i);
			const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

			if(pPrevInfo && pInfo)
			{
				//
				bool Local = m_pClient->m_Snap.m_LocalClientID == i;
				if((p % 2) == 0 && Local) continue;
				if((p % 2) == 1 && !Local) continue;

				CNetObj_Character PrevChar = m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				CNetObj_Character CurChar = m_pClient->m_Snap.m_aCharacters[i].m_Cur;

				if(p<2)
				{
					if(PrevChar.m_HookedPlayer != -1)
						RenderHook(
								&PrevChar,
								&CurChar,
								&m_aRenderInfo[i],
								i,
								m_CurPredictedPos[i],
								m_CurPredictedPos[PrevChar.m_HookedPlayer]
							);
					else
						RenderHook(
								&PrevChar,
								&CurChar,
								&m_aRenderInfo[i],
								i,
								m_CurPredictedPos[i],
								m_CurPredictedPos[i]
							);
				}
				else
				{
					RenderPlayer(
							&PrevChar,
							&CurChar,
							&m_aRenderInfo[i],
							i,
							m_CurPredictedPos[i]
						);
				}
			}
		}
	}
}

void CPlayers::OnInit()
{
	m_WeaponEmoteQuadContainerIndex = Graphics()->CreateQuadContainer();

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, 0);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize);
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, SPRITE_FLAG_FLIP_Y);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize);
	}

	// at the end the hand
	RenderTools()->SelectSprite(SPRITE_TEE_HAND_OUTLINE, 0, 0, 0);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f, false);
	RenderTools()->SelectSprite(SPRITE_TEE_HAND, 0, 0, 0);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f, false);

	RenderTools()->SelectSprite(SPRITE_HOOK_HEAD);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);
	RenderTools()->SelectSprite(SPRITE_HOOK_CHAIN);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);

	for(int i = 0; i < NUM_EMOTICONS; ++i)
	{
		RenderTools()->SelectSprite(SPRITE_OOP + i);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 64.f, false);
	}
	
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		m_WeaponSpriteMuzzleQuadContainerIndex[i] = Graphics()->CreateQuadContainer();
		for(int n = 0; n < g_pData->m_Weapons.m_aId[i].m_NumSpriteMuzzles; ++n)
		{
			if(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n])
			{
				RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n], 0);
			}
			if(WEAPON_NINJA == i)
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], 160.f);
			else
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], g_pData->m_Weapons.m_aId[i].m_VisualSize);

			if(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n])
			{
				RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n], SPRITE_FLAG_FLIP_Y);
			}
			if(WEAPON_NINJA == i)
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], 160.f);
			else
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], g_pData->m_Weapons.m_aId[i].m_VisualSize);
		}
	}

	Graphics()->QuadsSetSubset(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0.f);
	// the direction
	m_DirectionQuadContainerIndex = Graphics()->CreateQuadContainer();

	IGraphics::CQuadItem QuadItem(0.f, 0.f, 22.f, 22.f);
	Graphics()->QuadContainerAddQuads(m_DirectionQuadContainerIndex, &QuadItem, 1);

}
