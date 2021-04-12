#include "stdafx.h"
#include "base_monster.h"

#include "../../../ai_object_location.h"
#include "../../../game_graph.h"
#include "../../../ai_space.h"
#include "../../../hit.h"
#include "../../../PHDestroyable.h"
#include "../../../CharacterPhysicsSupport.h"
#include "../control_animation_base.h"
#include "net_physics_state.h"

#include "../../../../xrphysics/iPHWorld.h"
#include "../../../PHMovementControl.h"
#include "../../../../xrphysics/PhysicsShell.h"
#include "../xrphysics/phvalide.h"
#include "../../../sound_player.h"
#include "../../../../xrServerEntities/xrserver_objects_alife_monsters.h"

using sync_flags = CSE_ALifeMonsterBase::sync_flags;
using snd_flags = CSE_ALifeMonsterBase::snd_flags;

extern int g_cl_InterpolationType;

void CBaseMonster::net_Save			(NET_Packet& P)
{
	inherited::net_Save(P);
	m_pPhysics_support->in_NetSave(P);
}

BOOL CBaseMonster::net_SaveRelevant	()
{
	return (inherited::net_SaveRelevant() || BOOL(PPhysicsShell()!=NULL));
}

void CBaseMonster::net_Export(NET_Packet& P) 
{
	R_ASSERT				(Local());
	if (IsGameTypeSingle()) //Single net_export
	{
		// export last known packet
		R_ASSERT(!NET.empty());
		net_update& N = NET.back();
		P.w_float(GetfHealth());
		P.w_u32(N.dwTimeStamp);
		P.w_u8(0);
		P.w_vec3(N.p_pos);
		P.w_float /*w_angle8*/(N.o_model);
		P.w_float /*w_angle8*/(N.o_torso.yaw);
		P.w_float /*w_angle8*/(N.o_torso.pitch);
		P.w_float /*w_angle8*/(N.o_torso.roll);
		P.w_u8(u8(g_Team()));
		P.w_u8(u8(g_Squad()));
		P.w_u8(u8(g_Group()));

		GameGraph::_GRAPH_ID		l_game_vertex_id = ai_location().game_vertex_id();
		P.w(&l_game_vertex_id, sizeof(l_game_vertex_id));
		P.w(&l_game_vertex_id, sizeof(l_game_vertex_id));
		//	P.w						(&m_fGoingSpeed,			sizeof(m_fGoingSpeed));
		//	P.w						(&m_fGoingSpeed,			sizeof(m_fGoingSpeed));
		float					f1 = 0;
		if (ai().game_graph().valid_vertex_id(l_game_vertex_id)) {
			f1 = Position().distance_to(ai().game_graph().vertex(l_game_vertex_id)->level_point());
			P.w(&f1, sizeof(f1));
			f1 = Position().distance_to(ai().game_graph().vertex(l_game_vertex_id)->level_point());
			P.w(&f1, sizeof(f1));
		}
		else {
			P.w(&f1, sizeof(f1));
			P.w(&f1, sizeof(f1));
		}
	}
	else // MP net_export
	{
		CPHSynchronize* pPhSync = PHGetSyncItem(0);
		CControl_Com* pCapturer = control().get_capturer(ControlCom::eControlAnimation);

		Flags8 flags;
		flags.zero();
		
		// physics flag
		flags.set(sync_flags::fNeedPhysicSync, !!pPhSync);

		// sounds flags
		switch (m_sv_snd_sync_flag)
		{
		case snd_flags::monster_sound_no:
			flags.set(sync_flags::fSndNoSound, true);
			break;
		case snd_flags::monster_sound_play:
			flags.set(sync_flags::fSndPlayNoDelay, true);
			break;
		case snd_flags::monster_sound_play_with_delay:
			flags.set(sync_flags::fSndPlayWithDelay, true);
			break;
		default:
			break;
		}

		// animation flag
		if (pCapturer && pCapturer->ced() != NULL)
		{
			flags.set(sync_flags::fAnimNoLoop, true);
		}		
		
		flags.set(sync_flags::fHasCustomSyncFlag, HasCustomSyncFlag());

		// write flag
		P.w_u8(flags.get()); // <--

		if (flags.test(sync_flags::fHasCustomSyncFlag))
		{
			P.w_u8(GetCustomSyncFlag()); // <--
		}

		// write health
		P.w_float(GetfHealth()); // <--

		// write physics
		if (flags.test(sync_flags::fNeedPhysicSync))
		{
			SPHNetState state;
			pPhSync->get_State(state);

			net_physics_state physics_state;
			physics_state.fill(state, Level().timeServer());
			physics_state.write(P);
		}
		else
		{
			P.w_vec3(Position()); // <--
		}

		// write angles
		P.w_angle8(movement().m_body.current.pitch); // <--
		//P.w_angle8(movement().m_body.current.roll);
		P.w_angle8(movement().m_body.current.yaw); // <--

		// write sounds
		R_ASSERT(m_sv_snd_sync_sound < 255);
		switch (m_sv_snd_sync_flag)
		{
		case snd_flags::monster_sound_no:
			break;
		case snd_flags::monster_sound_play:
			P.w_u8(static_cast<u8>(m_sv_snd_sync_sound)); // <--
			break;
		case snd_flags::monster_sound_play_with_delay:
			P.w_u8(static_cast<u8>(m_sv_snd_sync_sound)); // <--
			P.w_u16(static_cast<u16>(m_sv_snd_sync_sound_delay / 1000)); // <--
			break;
		default:
			break;
		}  	

		// write animations
		if (flags.test(sync_flags::fAnimNoLoop))
		{
			control().get_capturer(ControlCom::eControlAnimation);
			SControlAnimationData *ctrl_anim = (SControlAnimationData*)control().data(pCapturer, ControlCom::eControlAnimation);
			VERIFY(ctrl_anim);

			P.w_u16(ctrl_anim->global.get_motion().idx);
			//P.w_u8(ctrl_anim->global.get_motion().slot);
			R_ASSERT3(ctrl_anim->global.get_motion().slot == 0, "motion slot is not zero", this->cName().c_str());
		}
		else
		{
			IKinematicsAnimated* ik_anim_obj = smart_cast<IKinematicsAnimated*>(Visual());
			P.w_u16(ik_anim_obj->ID_Cycle_Safe(m_anim_base->cur_anim_info().name).idx);
			//P.w_u8(ik_anim_obj->ID_Cycle_Safe(m_anim_base->cur_anim_info().name).slot);
			R_ASSERT3(ik_anim_obj->ID_Cycle_Safe(m_anim_base->cur_anim_info().name).slot == 0, "motion slot is not zero", this->cName().c_str());
		}

		// reset sounds
		m_sv_snd_sync_flag = snd_flags::monster_sound_no;
		m_sv_snd_sync_sound = 0;
		m_sv_snd_sync_sound_delay = 0;
	}
}

void CBaseMonster::net_Import(NET_Packet& P)
{
	R_ASSERT				(Remote());
	if (IsGameTypeSingle()) 
	{
		net_update				N;

		u8 flags;

		float health;
		P.r_float(health);
		SetfHealth(health);

		P.r_u32(N.dwTimeStamp);
		P.r_u8(flags);
		P.r_vec3(N.p_pos);
		P.r_float /*r_angle8*/(N.o_model);
		P.r_float /*r_angle8*/(N.o_torso.yaw);
		P.r_float /*r_angle8*/(N.o_torso.pitch);
		P.r_float /*r_angle8*/(N.o_torso.roll);
		id_Team = P.r_u8();
		id_Squad = P.r_u8();
		id_Group = P.r_u8();

		GameGraph::_GRAPH_ID		l_game_vertex_id = ai_location().game_vertex_id();
		P.r(&l_game_vertex_id, sizeof(l_game_vertex_id));
		P.r(&l_game_vertex_id, sizeof(l_game_vertex_id));

		if (NET.empty() || (NET.back().dwTimeStamp < N.dwTimeStamp)) {
			NET.push_back(N);
			NET_WasInterpolating = TRUE;
		}

		//	P.r						(&m_fGoingSpeed,			sizeof(m_fGoingSpeed));
		//	P.r						(&m_fGoingSpeed,			sizeof(m_fGoingSpeed));
		float					f1 = 0;
		if (ai().game_graph().valid_vertex_id(l_game_vertex_id)) {
			f1 = Position().distance_to(ai().game_graph().vertex(l_game_vertex_id)->level_point());
			P.r(&f1, sizeof(f1));
			f1 = Position().distance_to(ai().game_graph().vertex(l_game_vertex_id)->level_point());
			P.r(&f1, sizeof(f1));
		}
		else {
			P.r(&f1, sizeof(f1));
			P.r(&f1, sizeof(f1));
		}


		setVisible(TRUE);
		setEnabled(TRUE);
	} 
	else // MP net_import
	{
		Flags8 flags;
		u8 custom_flag;
		float health;
		Fvector fv_position;
		net_physics_state physics_state;
		SRotation fv_direction;

		u8 sound_type = 0;
		u32 sound_delay = 0;

		u16 u_motion_idx;
		// u8 u_motion_slot;

		setVisible(TRUE);
		setEnabled(TRUE);
		
		P.r_u8(flags.flags);

		if (flags.test(sync_flags::fHasCustomSyncFlag))
		{
			P.r_u8(custom_flag);
		}

		P.r_float(health);

		if (flags.test(sync_flags::fNeedPhysicSync))
		{
			physics_state.read(P);
			fv_position.set(physics_state.physics_position);
		}
		else
		{
			P.r_vec3(fv_position);
		}
		
		P.r_angle8(fv_direction.pitch);
		//P.r_angle8(fv_direction.roll);
		P.r_angle8(fv_direction.yaw);

		if (flags.test(sync_flags::fSndPlayNoDelay))
		{
			sound_type = P.r_u8();
		}
		else if (flags.test(sync_flags::fSndPlayWithDelay))
		{
			sound_type = P.r_u8();
			sound_delay = P.r_u16() * 1000;
		}

		P.r_u16(u_motion_idx);
		//P.r_u8(u_motion_slot);
		

		if (flags.test(sync_flags::fHasCustomSyncFlag))
		{
			ProcessCustomSyncFlag_CL(custom_flag);
		}

		// set health
		SetfHealth(health);
		
		// apply physics and position
		if (flags.test(sync_flags::fNeedPhysicSync) && !m_bSkipCorrectionPrediction)
		{
			monster_interpolation::net_update_A N_A;
			N_A.State.enabled = physics_state.physics_state_enabled;
			N_A.State.linear_vel = physics_state.physics_linear_velocity;
			N_A.State.position = physics_state.physics_position;
			N_A.o_torso = fv_direction;
			N_A.dwTimeStamp = physics_state.dwTimeStamp;

			// interpocation
			postprocess_packet(N_A);
		}
		else
		{
			Fmatrix M;
			M.setHPB(0.0f, -fv_direction.pitch, 0.0f);
			XFORM().mulB_43(M);
			XFORM().rotateY(fv_direction.yaw);

			// pavel: should be after rotation
			XFORM().translate_over(fv_position);
			Position().set(fv_position); // we need it?

			// pavel: not tested. We need it?
			if (!g_Alive())
			{
				PHUnFreeze();
			}
			NET_A.clear();
			m_bSkipCorrectionPrediction = false;
		}

		// play sound
		if (flags.test(sync_flags::fSndPlayNoDelay))
		{
			sound().play(sound_type);
		}
		else if (flags.test(sync_flags::fSndPlayWithDelay))
		{
			sound().play(sound_type, 0, 0, sound_delay);
		}
		
		// apply animation
		ApplyAnimation(u_motion_idx, 0, flags.test(sync_flags::fAnimNoLoop));
	}
}

void CBaseMonster::postprocess_packet(monster_interpolation::net_update_A &N_A)
{

	if (!NET_A.empty())
		N_A.dwTimeStamp = NET_A.back().dwTimeStamp;
	else
		N_A.dwTimeStamp = Level().timeServer();

	N_A.State.previous_position = N_A.State.position;
	N_A.State.previous_quaternion = N_A.State.quaternion;

	if (Local() && OnClient() || !g_Alive())
		return;

	if (!NET_A.empty() && N_A.dwTimeStamp < NET_A.back().dwTimeStamp) return;
	if (!NET_A.empty() && N_A.dwTimeStamp == NET_A.back().dwTimeStamp)
	{
		NET_A.back() = N_A;
	}
	else
	{
		VERIFY(valid_pos(N_A.State.position));
		NET_A.push_back(N_A);
		if (NET_A.size() > 5)
		{
			NET_A.pop_front();
		}
	};

	if (!NET_A.empty()) m_bInterpolate = true;

	Level().AddObject_To_Objects4CrPr(this);
	CrPr_SetActivated(false);
	CrPr_SetActivationStep(0);
}


void CBaseMonster::PH_B_CrPr()
{
	if (IsGameTypeSingle())
	{
		inherited::PH_B_CrPr();
		return;
	}

	if (CrPr_IsActivated()) return;
	if (CrPr_GetActivationStep() > physics_world()->StepsNum()) return;


	if (g_Alive())
	{
		CrPr_SetActivated(true);

		monster_interpolation::InterpData* pIStart = &IStart;
		pIStart->Pos = Position();
		pIStart->Vel = m_pPhysics_support->movement()->GetVelocity();

		pIStart->o_torso.yaw = angle_normalize(movement().m_body.current.yaw);
		pIStart->o_torso.pitch = angle_normalize(movement().m_body.current.pitch);
		pIStart->o_torso.roll = angle_normalize(movement().m_body.current.roll);


		CPHSynchronize* pSyncObj = NULL;
		pSyncObj = PHGetSyncItem(0);
		if (!pSyncObj) return;
		pSyncObj->get_State(LastState);

		if (Local() && OnClient())
		{
			PHUnFreeze();
			pSyncObj->set_State(NET_A.back().State);
		}
		else
		{
			auto N_A = NET_A.back();
			NET_A_Last = N_A;

			if (!N_A.State.enabled)
			{
				pSyncObj->set_State(N_A.State);
			}
			else
			{
				PHUnFreeze();
				pSyncObj->set_State(N_A.State);
				Position().set(IStart.Pos);
			};
		};
	}
	else
	{
		CrPr_SetActivated(true);
		PHUnFreeze();
	}
}

void CBaseMonster::PH_I_CrPr()
{
	if (IsGameTypeSingle())
	{
		inherited::PH_I_CrPr();
		return;
	}

	if (!CrPr_IsActivated()) return;

	if (g_Alive())
	{
		CPHSynchronize* pSyncObj = NULL;
		pSyncObj = PHGetSyncItem(0);
		if (!pSyncObj) return;
		pSyncObj->get_State(RecalculatedState);
	};
}

void CBaseMonster::PH_A_CrPr()
{
	if (IsGameTypeSingle())
	{
		inherited::PH_A_CrPr();
		return;
	}

	if (!CrPr_IsActivated()) return;
	if (!g_Alive()) return;

	CPHSynchronize* pSyncObj = NULL;
	pSyncObj = PHGetSyncItem(0);
	if (!pSyncObj) return;

	pSyncObj->get_State(PredictedState);
	pSyncObj->set_State(RecalculatedState);

	if (!m_bInterpolate)
	{
		return;
	}

	CalculateInterpolationParams();
}

void CBaseMonster::CalculateInterpolationParams()
{
	CPHSynchronize* pSyncObj = NULL;
	pSyncObj = PHGetSyncItem(0);

	monster_interpolation::InterpData* pIStart = &IStart;
	monster_interpolation::InterpData* pIEnd = &IEnd;


	pIEnd->Pos = PredictedState.position;
	pIEnd->Vel = PredictedState.linear_vel;
	pIEnd->o_torso = NET_A_Last.o_torso;

	Fvector SP0, SP1, SP2, SP3;
	Fvector HP0, HP1, HP2, HP3;

	SP0 = pIStart->Pos;
	HP0 = pIStart->Pos;

	if (m_bInInterpolation)
	{
		u32 CurTime = Level().timeServer();
		float factor = float(CurTime - m_dwIStartTime) / (m_dwIEndTime - m_dwIStartTime);
		if (factor > 1.0f) factor = 1.0f;

		float c = factor;
		for (u32 k = 0; k < 3; k++)
		{
			SP0[k] = c * (c*(c*SCoeff[k][0] + SCoeff[k][1]) + SCoeff[k][2]) + SCoeff[k][3];
			SP1[k] = (c*c*SCoeff[k][0] * 3 + c * SCoeff[k][1] * 2 + SCoeff[k][2]) / 3; //     3       !!!!

			HP0[k] = c * (c*(c*HCoeff[k][0] + HCoeff[k][1]) + HCoeff[k][2]) + HCoeff[k][3];
			HP1[k] = (c*c*HCoeff[k][0] * 3 + c * HCoeff[k][1] * 2 + HCoeff[k][2]) / 3; //     3       !!!!
		};

		SP1.add(SP0);
	}
	else
	{
		if (LastState.linear_vel.x == 0 && LastState.linear_vel.y == 0 && LastState.linear_vel.z == 0)
		{
			HP1.sub(RecalculatedState.position, RecalculatedState.previous_position);
		}
		else
		{
			HP1.sub(LastState.position, LastState.previous_position);
		};
		HP1.mul(1.0f / fixed_step);
		SP1.add(HP1, SP0);
	}

	HP2.sub(PredictedState.position, PredictedState.previous_position);
	HP2.mul(1.0f / fixed_step);
	SP2.sub(PredictedState.position, HP2);

	SP3.set(PredictedState.position);
	HP3.set(PredictedState.position);

	Fvector TotalPath;
	TotalPath.sub(SP3, SP0);
	float TotalLen = TotalPath.magnitude();

	SPHNetState	State0 = (NET_A.back()).State;
	SPHNetState	State1 = PredictedState;

	float lV0 = State0.linear_vel.magnitude();
	float lV1 = State1.linear_vel.magnitude();

	u32 ConstTime = u32((fixed_step - physics_world()->FrameTime()) * 1000) + Level().GetInterpolationSteps()*u32(fixed_step * 1000);

	m_dwIStartTime = m_dwILastUpdateTime;
	m_dwIEndTime = m_dwIStartTime + ConstTime;

	Fvector V0, V1;
	V0.set(HP1);
	V1.set(HP2);
	lV0 = V0.magnitude();
	lV1 = V1.magnitude();

	if (TotalLen != 0)
	{
		if (V0.x != 0 || V0.y != 0 || V0.z != 0)
		{
			if (lV0 > TotalLen / 3)
			{
				HP1.normalize();
				//				V0.normalize();
				//				V0.mul(TotalLen/3);
				HP1.normalize();
				HP1.mul(TotalLen / 3);
				SP1.add(HP1, SP0);
			}
		}

		if (V1.x != 0 || V1.y != 0 || V1.z != 0)
		{
			if (lV1 > TotalLen / 3)
			{
				//				V1.normalize();
				//				V1.mul(TotalLen/3);
				HP2.normalize();
				HP2.mul(TotalLen / 3);
				SP2.sub(SP3, HP2);
			};
		}
	};
	/////////////////////////////////////////////////////////////////////////////
	for (u32 i = 0; i < 3; i++)
	{
		SCoeff[i][0] = SP3[i] - 3 * SP2[i] + 3 * SP1[i] - SP0[i];
		SCoeff[i][1] = 3 * SP2[i] - 6 * SP1[i] + 3 * SP0[i];
		SCoeff[i][2] = 3 * SP1[i] - 3 * SP0[i];
		SCoeff[i][3] = SP0[i];

		HCoeff[i][0] = 2 * HP0[i] - 2 * HP3[i] + HP1[i] + HP2[i];
		HCoeff[i][1] = -3 * HP0[i] + 3 * HP3[i] - 2 * HP1[i] - HP2[i];
		HCoeff[i][2] = HP1[i];
		HCoeff[i][3] = HP0[i];
	};
	/////////////////////////////////////////////////////////////////////////////
	m_bInInterpolation = true;

	if (m_pPhysicsShell) m_pPhysicsShell->NetInterpolationModeON();
}

void CBaseMonster::ApplyAnimation(u16 motion_idx, u8 motion_slot, bool noLoop)
{
	MotionID motion;
	IKinematicsAnimated* ik_anim_obj = smart_cast<IKinematicsAnimated*>(Visual());
	if (u_last_motion_idx != motion_idx || u_last_motion_slot != motion_slot)
	{
		u_last_motion_idx = motion_idx;
		u_last_motion_slot = motion_slot;
		u_last_motion_no_loop = noLoop;
		motion.idx = motion_idx;
		motion.slot = motion_slot;
		if (motion.valid())
		{
			u16 bone_or_part = ik_anim_obj->LL_GetMotionDef(motion)->bone_or_part;
			if (bone_or_part == u16(-1)) bone_or_part = ik_anim_obj->LL_PartID("default");

			CStepManager::on_animation_start(motion, ik_anim_obj->LL_PlayCycle(bone_or_part, motion, TRUE,
				ik_anim_obj->LL_GetMotionDef(motion)->Accrue(), ik_anim_obj->LL_GetMotionDef(motion)->Falloff(),
				ik_anim_obj->LL_GetMotionDef(motion)->Speed(), noLoop, 0, 0, 0));
		}
	}
}

void CBaseMonster::make_Interpolation()
{
	m_dwILastUpdateTime = Level().timeServer();

	if (g_Alive() && m_bInInterpolation)
	{
		u32 CurTime = m_dwILastUpdateTime;

		if (CurTime >= m_dwIEndTime)
		{
			m_bInInterpolation = false;

			CPHSynchronize* pSyncObj = NULL;
			pSyncObj = PHGetSyncItem(0);
			if (!pSyncObj) return;
			pSyncObj->set_State(PredictedState);
			VERIFY2(_valid(renderable.xform), *cName());
		}
		else
		{
			float factor = 0.0f;

			if (m_dwIEndTime != m_dwIStartTime)
				factor = float(CurTime - m_dwIStartTime) / (m_dwIEndTime - m_dwIStartTime);

			clamp(factor, 0.f, 1.0f);

			Fvector NewPos;
			NewPos.lerp(IStart.Pos, IEnd.Pos, factor);

			VERIFY2(_valid(renderable.xform), *cName());

			float& yaw = movement().m_body.current.yaw;
			float& pitch = movement().m_body.current.pitch;

			float f_yaw = angle_lerp(IStart.o_torso.yaw, IEnd.o_torso.yaw, factor);
			float f_pitch = angle_lerp(IStart.o_torso.pitch, IEnd.o_torso.pitch, factor);

			XFORM().rotateY(f_yaw);

			Fmatrix M;
			M.setHPB(0.0f, -f_pitch, 0.0f);
			XFORM().mulB_43(M);

			movement().m_body.current.pitch = f_pitch;
			movement().m_body.current.yaw = f_yaw;

			for (u32 k = 0; k < 3; k++)
			{
				IPosL[k] = NewPos[k];
				IPosS[k] = factor * (factor*(factor*SCoeff[k][0] + SCoeff[k][1]) + SCoeff[k][2]) + SCoeff[k][3];
				IPosH[k] = factor * (factor*(factor*HCoeff[k][0] + HCoeff[k][1]) + HCoeff[k][2]) + HCoeff[k][3];
			};

			Fvector SpeedVector, ResPosition;
			switch (g_cl_InterpolationType)
			{
			case 0:
			{
				ResPosition.set(IPosL);
				SpeedVector.sub(IEnd.Pos, IStart.Pos);
				SpeedVector.div(float(m_dwIEndTime - m_dwIStartTime) / 1000.0f);
			}break;
			case 1:
			{
				for (int k = 0; k < 3; k++)
					SpeedVector[k] = (factor*factor*SCoeff[k][0] * 3 + factor * SCoeff[k][1] * 2 + SCoeff[k][2]) / 3; //     3       !!!!

				ResPosition.set(IPosS);
			}break;
			case 2:
			{
				for (int k = 0; k < 3; k++)
					SpeedVector[k] = (factor*factor*HCoeff[k][0] * 3 + factor * HCoeff[k][1] * 2 + HCoeff[k][2]);

				ResPosition.set(IPosH);
			}break;
			default:
				R_ASSERT2(0, "Unknown interpolation curve type!");
				break;
			}
			XFORM().translate_over(ResPosition);
			Position().set(ResPosition); // we need it?
			character_physics_support()->movement()->SetPosition(ResPosition); // we need it?
			character_physics_support()->movement()->SetVelocity(SpeedVector);
		};
	}
	else
	{
		m_bInInterpolation = false;
	};
};
