#include "stdafx.h"
#include "pch_script.h"
#include "ai_stalker.h"

#include "../../ai_object_location.h"
#include "../../game_graph.h"
#include "../../ai_space.h"
#include "../../CharacterPhysicsSupport.h"
#include "../../stalker_movement_manager_smart_cover.h"
#include "../../inventory.h"
#include "../../stalker_animation_manager.h"
#include "../../Weapon.h"

void CAI_Stalker::net_Save(NET_Packet& P)
{
	inherited::net_Save(P);
	m_pPhysics_support->in_NetSave(P);
}

BOOL CAI_Stalker::net_SaveRelevant()
{
	return (inherited::net_SaveRelevant() || BOOL(PPhysicsShell() != NULL));
}

void CAI_Stalker::net_Export(NET_Packet& P)
{
	R_ASSERT(Local());
	if (IsGameTypeSingle()) {
		// export last known packet
		R_ASSERT(!NET.empty());
		net_update& N = NET.back();
		//	P.w_float						(inventory().TotalWeight());
		//	P.w_u32							(m_dwMoney);

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


		float					f1 = 0;
		GameGraph::_GRAPH_ID		l_game_vertex_id = ai_location().game_vertex_id();
		P.w(&l_game_vertex_id, sizeof(l_game_vertex_id));
		P.w(&l_game_vertex_id, sizeof(l_game_vertex_id));
		//	P.w						(&f1,						sizeof(f1));
		//	P.w						(&f1,						sizeof(f1));
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

		P.w_stringZ(m_sStartDialog);
	}
	else
	{
		P.w_float(GetfHealth());
		P.w_vec3(Position());
		P.w_angle8(movement().m_body.current.pitch);
		P.w_angle8(movement().m_body.current.roll);
		P.w_angle8(movement().m_body.current.yaw);
		P.w_angle8(movement().m_head.current.pitch);
		P.w_angle8(movement().m_head.current.yaw);

		if (inventory().ActiveItem())
		{
			CWeapon	*weapon = smart_cast<CWeapon*>(inventory().ActiveItem());
			if (weapon && !weapon->strapped_mode())
			{
				P.w_u16(inventory().ActiveItem()->CurrSlot());
			}
			else
			{
				P.w_u16(NO_ACTIVE_SLOT);
			}
		}
		else
			P.w_u16(NO_ACTIVE_SLOT);

		P.w_u16(m_animation_manager->torso().animation().idx);
		P.w_u8(m_animation_manager->torso().animation().slot);
		P.w_u16(m_animation_manager->legs().animation().idx);
		P.w_u8(m_animation_manager->legs().animation().slot);
		P.w_u16(m_animation_manager->head().animation().idx);
		P.w_u8(m_animation_manager->head().animation().slot);

		P.w_u16(m_animation_manager->script().animation().idx);
		P.w_u8(m_animation_manager->script().animation().slot);
	}
}

void CAI_Stalker::net_Import(NET_Packet& P)
{
	R_ASSERT(Remote());
	if (IsGameTypeSingle())
	{
		net_update						N;

		u8 flags;

		P.r_float();
		set_money(P.r_u32(), false);

		float health;
		P.r_float(health);
		SetfHealth(health);
		//	fEntityHealth = health;

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


		GameGraph::_GRAPH_ID				graph_vertex_id = movement().game_dest_vertex_id();
		P.r(&graph_vertex_id, sizeof(GameGraph::_GRAPH_ID));
		graph_vertex_id = ai_location().game_vertex_id();
		P.r(&graph_vertex_id, sizeof(GameGraph::_GRAPH_ID));

		if (NET.empty() || (NET.back().dwTimeStamp < N.dwTimeStamp)) {
			NET.push_back(N);
			NET_WasInterpolating = TRUE;
		}

		P.r_float();
		P.r_float();

		P.r_stringZ(m_sStartDialog);

		setVisible(TRUE);
		setEnabled(TRUE);
	}
	else
	{
		float f_health;
		Fvector fv_position;
		Fvector fv_direction;
		Fvector fv_head_orientation;

		u16	u_active_weapon_slot;

		u16 u_torso_motion_idx;
		u8 u_torso_motion_slot;
		u16 u_legs_motion_idx;
		u8 u_legs_motion_slot;
		u16 u_head_motion_idx;
		u8 u_head_motion_slot;
		u16 u_script_motion_idx;
		u8 u_script_motion_slot;

		P.r_float(f_health);
		P.r_vec3(fv_position);
		P.r_angle8(fv_direction.x);
		P.r_angle8(fv_direction.y);
		P.r_angle8(fv_direction.z);
		P.r_angle8(fv_head_orientation.x);
		P.r_angle8(fv_head_orientation.z);

		P.r_u16(u_active_weapon_slot);

		P.r_u16(u_torso_motion_idx);
		P.r_u8(u_torso_motion_slot);
		P.r_u16(u_legs_motion_idx);
		P.r_u8(u_legs_motion_slot);
		P.r_u16(u_head_motion_idx);
		P.r_u8(u_head_motion_slot);

		P.r_u16(u_script_motion_idx);
		P.r_u8(u_script_motion_slot);

		SetfHealth(f_health);
		Position().set(fv_position);
		movement().m_body.current.pitch = fv_direction.x;
		movement().m_body.current.roll = fv_direction.y;
		movement().m_body.current.yaw = fv_direction.z;
		movement().m_head.current.pitch = fv_head_orientation.x;
		movement().m_head.current.yaw = fv_head_orientation.z;

		SPHNetState	State;
		State.position = fv_position;
		State.previous_position = fv_position;
		CPHSynchronize* pSyncObj = NULL;
		pSyncObj = PHGetSyncItem(0);
		if (pSyncObj)
		{
			pSyncObj->set_State(State);
		}

		inventory().SetActiveSlot(u_active_weapon_slot);

		MotionID motion;
		IKinematicsAnimated* ik_anim_obj = smart_cast<IKinematicsAnimated*>(Visual());
		if (u_last_torso_motion_idx != u_torso_motion_idx)
		{
			u_last_torso_motion_idx = u_torso_motion_idx;
			motion.idx = u_torso_motion_idx;
			motion.slot = u_torso_motion_slot;
			if (motion.valid())
			{
				ik_anim_obj->LL_PlayCycle(ik_anim_obj->LL_PartID("torso"), motion, TRUE, 
					ik_anim_obj->LL_GetMotionDef(motion)->Accrue(), ik_anim_obj->LL_GetMotionDef(motion)->Falloff(), 
					ik_anim_obj->LL_GetMotionDef(motion)->Speed(), FALSE, 0, 0, 0);
			}
		}
		if (u_last_legs_motion_idx != u_legs_motion_idx)
		{
			u_last_legs_motion_idx = u_legs_motion_idx;
			motion.idx = u_legs_motion_idx;
			motion.slot = u_legs_motion_slot;
			if (motion.valid())
			{
				CStepManager::on_animation_start(motion, ik_anim_obj->LL_PlayCycle(ik_anim_obj->LL_PartID("legs"), motion, 
					TRUE, ik_anim_obj->LL_GetMotionDef(motion)->Accrue(), 
					ik_anim_obj->LL_GetMotionDef(motion)->Falloff(), ik_anim_obj->LL_GetMotionDef(motion)->Speed(), FALSE, 0, 0, 0));
			}
		}
		if (u_last_head_motion_idx != u_head_motion_idx)
		{
			u_last_head_motion_idx = u_head_motion_idx;
			motion.idx = u_head_motion_idx;
			motion.slot = u_head_motion_slot;
			if (motion.valid())
			{
				ik_anim_obj->LL_PlayCycle(ik_anim_obj->LL_PartID("head"), motion, TRUE, 
					ik_anim_obj->LL_GetMotionDef(motion)->Accrue(), ik_anim_obj->LL_GetMotionDef(motion)->Falloff(), 
					ik_anim_obj->LL_GetMotionDef(motion)->Speed(), FALSE, 0, 0, 0);
			}
		}

		if (u_last_script_motion_idx != u_script_motion_idx) {
			motion.idx = u_script_motion_idx;
			motion.slot = u_script_motion_slot;
			u_last_script_motion_idx = u_script_motion_idx;
			if (motion.valid())
			{
				ik_anim_obj->LL_PlayCycle(ik_anim_obj->LL_GetMotionDef(motion)->bone_or_part, motion, TRUE,
					ik_anim_obj->LL_GetMotionDef(motion)->Accrue(), ik_anim_obj->LL_GetMotionDef(motion)->Falloff(), 
					ik_anim_obj->LL_GetMotionDef(motion)->Speed(), ik_anim_obj->LL_GetMotionDef(motion)->StopAtEnd(), 0, 0, 0);
			}
		}

		setVisible(TRUE);
		setEnabled(TRUE);
	}
}