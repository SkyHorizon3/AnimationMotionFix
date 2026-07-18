#include "Hooks.h"
#include "Settings.h"

namespace AMF
{
	inline bool IsMovementAnimationDriven_1405E3250(RE::Actor* a_actor)
	{
		using func_t = decltype(&IsMovementAnimationDriven_1405E3250);
		static REL::Relocation<func_t> func{ REL::VariantID(36487, 37486, 0x5EB8F0) };
		return func(a_actor);
	}

	inline RE::hkContactPoint* GetContactPoint_140A9ED70(RE::hkpSimpleConstraintContactMgr* a_mgr, uint16_t a_contactPointIds)
	{
		using func_t = decltype(&GetContactPoint_140A9ED70);
		static REL::Relocation<func_t> func{ REL::VariantID(61252, 62142, 0xAD9770) };
		return func(a_mgr, a_contactPointIds);
	}

	inline void SetInvMassScalingForContact_140AA8740(RE::hkpSimpleConstraintContactMgr* a_mgr, RE::hkpRigidBody* a_body, RE::hkpConstraintOwner& a_constraintOwner, const RE::hkVector4& a_factor)
	{
		using func_t = decltype(&SetInvMassScalingForContact_140AA8740);
		static REL::Relocation<func_t> func{ REL::VariantID(61388, 62282, 0xAE3140) };
		func(a_mgr, a_body, a_constraintOwner, a_factor);
	}

	// implementation of hkpAddModifierUtil::setInvMassScalingForContact not included in Skyrim exe file
	inline void SetInvMassScalingForContact_Impl(const RE::hkpContactPointEvent& a_event, RE::hkpRigidBody* a_rigidBody, const RE::hkVector4& a_factor)
	{
		auto island = a_event.bodies[0]->simulationIsland;
		if (island->storageIndex == 0xFFFF) {  // hkpSimulationIsland::isFixed
			island = a_event.bodies[1]->simulationIsland;
		}

		if (a_event.type == RE::hkpContactPointEvent::Type::kManifold) {
			const auto old_threadID = island->multiThreadCheck.threadId;
			const auto old_markCount = island->multiThreadCheck.markCount;
			const auto old_stackTrace = island->multiThreadCheck.stackTraceId;

			island->multiThreadCheck.markCount |= 0x8000;  //hkMultiThreadCheck::disableChecks
			SetInvMassScalingForContact_140AA8740(a_event.contactMgr, a_rigidBody, *island, a_factor);

			island->multiThreadCheck.threadId = old_threadID;
			island->multiThreadCheck.stackTraceId = old_stackTrace;
			island->multiThreadCheck.markCount = old_markCount;
		} else {
			SetInvMassScalingForContact_140AA8740(a_event.contactMgr, a_rigidBody, *island, a_factor);
		}
	}

	bool FixPitchTransHandler::RevertPitchRotation(RE::Actor* a_actor, RE::NiPoint3& a_translation)
	{
		if (!a_actor)
			return false;

		const auto actorState = a_actor->AsActorState();
		if (!actorState || actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal || actorState->IsFlying())
			return false;

		bool bIsSynced = false;
		if (a_actor->GetGraphVariableBool(RE::FixedStrings::GetSingleton()->bIsSynced, bIsSynced) && bIsSynced) {
			return false;
		}

		if (IsMovementAnimationDriven_1405E3250(a_actor) && (a_actor->IsAnimationDriven() || a_actor->IsRotationAllowed())) {
			auto pitchAngle = a_actor->data.angle.x;
			if (std::abs(pitchAngle) > 1.57079638f) {
				return false;  //Gimbal Lock Occured
			}

			auto nonPitchTranslationY = a_translation.y / std::cos(pitchAngle);
			auto nonPitchTranslationZ = a_translation.z - nonPitchTranslationY * std::sin(pitchAngle);
			a_translation.y = nonPitchTranslationY;
			a_translation.z = nonPitchTranslationZ;

			return true;
		}

		return false;
	}

	void FixPitchTransHandler::Hook_ConvertMoveDirToTranslation(const RE::NiPoint3& a_angle, RE::NiPoint3& a_outDirection, RE::Actor* a_actor)
	{
		ConvertMoveDirToTranslation(a_angle, a_outDirection);
		if (a_actor && !a_actor->IsPlayerRef() && AMFSettings::GetSingleton()->enablePitchTranslationFix) {
			RevertPitchRotation(a_actor, a_outDirection);
		}
	}

	bool AttackMagnetismHandler::ShouldDisableMovementMagnetism(RE::Actor* a_actor)
	{
		if (!a_actor)
			return false;

		auto settings = AMFSettings::GetSingleton();
		bool isDisableInSetting = a_actor->IsPlayerRef() ? settings->disablePlayerMovementMagnetism : settings->disableNpcMovementMagnetism;
		if (isDisableInSetting) {
			bool bForceMoveMagnetism = a_actor->GetGraphVariableBool("AMF_bForceMoveMagnetism", bForceMoveMagnetism) && bForceMoveMagnetism;
			return !bForceMoveMagnetism;
		} else {
			bool bForbidMoveMagnetism = a_actor->GetGraphVariableBool("AMF_bForbidMoveMagnetism", bForbidMoveMagnetism) && bForbidMoveMagnetism;
			return bForbidMoveMagnetism;
		}
	}

	void AttackMagnetismHandler::PlayerRotateMagnetismHook::UpdateMagnetism(RE::PlayerCharacter* a_player, float a_delta, RE::NiPoint3& a_translation, float& a_rotationZ)
	{
		if (!AMFSettings::GetSingleton()->disablePlayerRotationMagnetism) {
			return func(a_player, a_delta, a_translation, a_rotationZ);
		}
	}

	bool AttackMagnetismHandler::MovementMagnetismHook::Hook_IsStartingMeleeAttack(RE::Actor* a_actor)
	{
		if (ShouldDisableMovementMagnetism(a_actor)) {
			return false;
		}

		return func(a_actor);
	}

	bool PushCharacterHandler::ShouldPreventAttackPushing(RE::Actor* a_pusher, RE::Actor* a_target)
	{
		if (!a_pusher || !a_target)
			return false;

		const auto pusherHandle = a_pusher->GetActorRuntimeData().currentCombatTarget;
		auto combatTarg = pusherHandle ? pusherHandle.get() : nullptr;

		if (combatTarg && AttackMagnetismHandler::ShouldDisableMovementMagnetism(a_pusher) && a_pusher->IsAttacking() &&
			IsMovementAnimationDriven_1405E3250(a_pusher)) {
			if (a_target == combatTarg.get() || a_target->GetMountedBy(combatTarg)) {
				return true;
			}
		}

		return false;
	}

	bool PushCharacterHandler::ShouldPreventAttackPushing(RE::bhkCharacterController* a_pusher, RE::bhkCharacterController* a_target)
	{
		auto pusherActor = GetActor(a_pusher);
		if (pusherActor) {
			auto targActor = GetActor(a_target);
			if (targActor)
				return ShouldPreventAttackPushing(pusherActor, targActor);
		}

		return false;
	}

	RE::Actor* PushCharacterHandler::GetActor(RE::bhkCharacterController* a_charCtrl)
	{
		return a_charCtrl ? GetActor(a_charCtrl->GetRigidBody()) : nullptr;
	}

	RE::Actor* PushCharacterHandler::GetActor(RE::hkpWorldObject* a_rigidBody)
	{
		if (!a_rigidBody)
			return nullptr;

		const auto charCollisionFilterInfo = a_rigidBody->collidable.GetCollisionLayer();
		if (charCollisionFilterInfo != RE::COL_LAYER::kCharController)
			return nullptr;

		auto objRef = a_rigidBody ? a_rigidBody->GetUserData() : nullptr;
		return objRef ? objRef->As<RE::Actor>() : nullptr;
	}

	void PushCharacterHandler::ProxyPushProxyHandler::Hook_PushTargetCharacter(RE::bhkCharacterController* a_pusher, RE::bhkCharacterController* a_target, RE::hkContactPoint* a_contactPoint)
	{
		if (ShouldPreventAttackPushing(a_pusher, a_target))
			return;

		func(a_pusher, a_target, a_contactPoint);
	}

	void PushCharacterHandler::ProxyPushRigidBodyHandler::Hook_PushTargetCharacter(RE::bhkCharacterController* a_pusher, RE::bhkCharacterController* a_target, RE::hkContactPoint* a_contactPoint)
	{
		if (ShouldPreventAttackPushing(a_pusher, a_target))
			return;

		func(a_pusher, a_target, a_contactPoint);
	}

	void PushCharacterHandler::RigidBodyPushProxyHandler::Hook_ProcessConstraintsCallback(RE::bhkCharProxyController* a_proxyCtrl, const RE::hkpCharacterProxy* a_proxy, const RE::hkArray<RE::hkpRootCdPoint>& a_manifold, RE::hkpSimplexSolverInput& a_input)
	{
		ProcessConstraintsCallback(a_proxyCtrl, a_proxy, a_manifold, a_input);

		for (std::int32_t i = 0; i < a_manifold.size(); i++) {
			const RE::hkpRootCdPoint& rootPoint = a_manifold[i];
			const RE::hkpCollidable* rootCollidableB = rootPoint.rootCollidableB;

			if (!rootCollidableB || rootCollidableB->GetCollisionLayer() != RE::COL_LAYER::kCharController)
				continue;

			if (static_cast<RE::hkpWorldObject::BroadPhaseType>(rootCollidableB->broadPhaseHandle.type) != RE::hkpWorldObject::BroadPhaseType::kEntity)
				continue;

			const auto attackerRef = RE::TESHavokUtilities::FindCollidableRef(*rootCollidableB);
			const auto attacker = attackerRef ? attackerRef->As<RE::Actor>() : nullptr;

			if (ShouldPreventAttackPushing(attacker, GetActor(a_proxyCtrl))) {
				auto attackerCharCtrl = attacker->GetCharController();
				auto rigidBodyChar = attackerCharCtrl ? skyrim_cast<RE::bhkCharRigidBodyController*>(attackerCharCtrl) : nullptr;

				if (rigidBodyChar) {
					a_input.constraints[i].velocity = { 0 };

					{
						WriteLocker lock(charCtrlPlaneLock);
						charCtrlPlaneMap.emplace(rigidBodyChar, a_input.constraints[i].plane);
					}
				}
			}
		}
	}

	void PushCharacterHandler::RigidBodyPushProxyHandler::Hook_UpdateForAnimationAttack(RE::bhkCharacterController* a_charCtrl)
	{
		UpdateForAnimationAttack(a_charCtrl);

		auto rigidCharCtrl = a_charCtrl ? skyrim_cast<RE::bhkCharRigidBodyController*>(a_charCtrl) : nullptr;
		if (!rigidCharCtrl)
			return;

		{
			WriteLocker lock(charCtrlPlaneLock);
			auto it = charCtrlPlaneMap.find(rigidCharCtrl);
			if (it != charCtrlPlaneMap.end()) {
				const auto& normal = it->second;
				RE::hkVector4 currentVelocity;
				rigidCharCtrl->GetLinearVelocityImpl(currentVelocity);

				const auto velDotNormal = currentVelocity.Dot3(normal);
				if (velDotNormal > 0.0f) {
					const auto counterVel = normal * (-velDotNormal);
					currentVelocity = currentVelocity + counterVel;
					rigidCharCtrl->SetLinearVelocityImpl(currentVelocity);
				}

				charCtrlPlaneMap.erase(it);
			}
		}
	}

	void PushCharacterHandler::RigidBodyPushProxyHandler::Hook_DeleteThis(RE::bhkCharRigidBodyController* a_charCtrl)
	{
		{
			WriteLocker lock(charCtrlPlaneLock);
			auto it = charCtrlPlaneMap.find(a_charCtrl);
			if (it != charCtrlPlaneMap.end()) {
				charCtrlPlaneMap.erase(it);
			}
		}

		DeleteThis(a_charCtrl);
	}

	void PushCharacterHandler::RigidBodyPushRigidBodyHandler::Hook_PushTargetCharacter(RE::bhkCharacterController* a_pusher, RE::bhkCharacterController* a_target, RE::hkContactPoint* a_contactPoint)
	{
		if (ShouldPreventAttackPushing(a_pusher, a_target))
			return;

		PushTargetCharacter(a_pusher, a_target, a_contactPoint);
	}

	void PushCharacterHandler::RigidBodyPushRigidBodyHandler::Hook_AddContactListener(RE::bhkWorld* a_world, RE::hkpContactListener* a_listener)
	{
		_AddContactListener(a_world, AMFContactListener::GetSingleton());
		_AddContactListener(a_world, a_listener);
	}

	void PushCharacterHandler::RigidBodyPushRigidBodyHandler::AMFContactListener::ContactPointCallback(const RE::hkpContactPointEvent& a_event)
	{
		const auto prop = a_event.contactPointProperties;
		if (!prop || prop->flags.any(RE::hkContactPointMaterial::Flag::kIsDisabled) || !a_event.contactMgr) {
			return;
		}

		auto rigidBodyA = a_event.bodies[0];
		auto rigidBodyB = a_event.bodies[1];

		auto attacker = GetActor(rigidBodyA);
		auto target = GetActor(rigidBodyB);

		if (ShouldPreventAttackPushing(attacker, target) && rigidBodyA->simulationIsland && rigidBodyB->simulationIsland) {
			rigidBodyB->responseModifierFlags |= 1;  //MASS_SCALING = 1
			SetInvMassScalingForContact_Impl(a_event, rigidBodyB, { 0 });

			if (ShouldPreventAttackPushing(target, attacker)) {
				rigidBodyA->responseModifierFlags |= 1;
				SetInvMassScalingForContact_Impl(a_event, rigidBodyA, { 0 });
			}
		}
	}
}
