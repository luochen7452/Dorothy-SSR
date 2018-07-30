/* Copyright (c) 2018 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "Const/Header.h"
#include "Platformer/Define.h"
#include "Support/Geometry.h"
#include "Platformer/UnitAction.h"
#include "Platformer/Unit.h"
#include "Platformer/UnitDef.h"
#include "Platformer/Data.h"
#include "Platformer/BulletDef.h"
#include "Platformer/Bullet.h"
#include "Platformer/AI.h"
#include "Platformer/Property.h"
#include "Animation/ModelDef.h"
#include "Node/Model.h"
#include "Physics/Sensor.h"
#include "Physics/World.h"
#include "Platformer/VisualCache.h"
#include "Lua/LuaHandler.h"
#include "Audio/Sound.h"

NS_DOROTHY_PLATFORMER_BEGIN

UnitActionDef::UnitActionDef(
	LuaFunctionBool available,
	LuaFunctionFuncBool create,
	LuaFunction stop):
available(available),
create(create),
stop(stop)
{ }

Own<UnitAction> UnitActionDef::toAction(Unit* unit)
{
	ScriptUnitAction* action = new ScriptUnitAction(name, priority, unit);
	action->reaction = reaction;
	action->recovery = recovery;
	action->_available = available;
	action->_create = create;
	action->_stop = stop;
	return MakeOwn(s_cast<UnitAction*>(action));
}

// UnitAction

unordered_map<string, Own<UnitActionDef>> UnitAction::_actionDefs;

UnitAction::UnitAction(String name, int priority, Unit* owner):
_name(name),
_priority(priority),
_isDoing(false),
_owner(owner),
reaction(-1.0f)
{ }

UnitAction::~UnitAction()
{
	_owner = nullptr;
}

const string& UnitAction::getName() const
{
	return _name;
}

int UnitAction::getPriority() const
{
	return _priority;
}

bool UnitAction::isDoing() const
{
	return _isDoing;
}

Unit* UnitAction::getOwner() const
{
	return _owner;
}

bool UnitAction::isAvailable()
{
	return true;
}

void UnitAction::run()
{
	_isDoing = true;
	_reflexDelta = 0.0f;
	if (actionStart)
	{
		actionStart(this);
	}
}

void UnitAction::update(float dt)
{
	float reactionTime = _owner->sensity * UnitAction::reaction;
	if (reactionTime >= 0)
	{
		_reflexDelta += dt;
		if (_reflexDelta >= reactionTime)
		{
			_reflexDelta = 0.0f;
			// Check AI here
			SharedAI.conditionedReflex(_owner);
		}
	}
}

void UnitAction::stop()
{
	_isDoing = false;
	if (actionEnd)
	{
		actionEnd(this);
	}
}

void UnitAction::add(
	String name, int priority, float reaction, float recovery,
	LuaFunctionBool available,
	LuaFunctionFuncBool create,
	LuaFunction stop)
{
	UnitActionDef* actionDef = new UnitActionDef(available, create, stop);
	actionDef->name = name;
	actionDef->priority = priority;
	actionDef->reaction = reaction;
	actionDef->recovery = recovery;
	_actionDefs[name] = MakeOwn(actionDef);
}

void UnitAction::clear()
{
	_actionDefs.clear();
}

ScriptUnitAction::ScriptUnitAction(String name, int priority, Unit* owner):
UnitAction(name, priority, owner),
_available(0),
_create(0),
_update(0),
_stop(0)
{ }

bool ScriptUnitAction::isAvailable()
{
	return _available(this);
}

void ScriptUnitAction::run()
{
	UnitAction::run();
	_update = _create();
	if (_update(this, 0.0f))
	{
		ScriptUnitAction::stop();
	}
}

void ScriptUnitAction::update(float dt)
{
	if (_update(this, dt))
	{
		ScriptUnitAction::stop();
	}
	UnitAction::update(dt);
}

void ScriptUnitAction::stop()
{
	_update = LuaFunctionBool(0);
	_stop(this);
	UnitAction::stop();
}

// Walk

Walk::Walk(Unit* unit):
UnitAction(ActionSetting::UnitActionWalk, ActionSetting::PriorityWalk, unit)
{
	UnitAction::reaction = ActionSetting::ReactionWalk;
	UnitAction::recovery = ActionSetting::RecoveryWalk;
}

bool Walk::isAvailable()
{
	return _owner->isOnSurface();
}

void Walk::run()
{
	Model* model = _owner->getModel();
	model->setSpeed(_owner->moveSpeed);
	model->setLoop(true);
	model->setLook(ActionSetting::LookNormal);
	model->setRecovery(UnitAction::recovery);
	model->resume(ActionSetting::AnimationWalk);
	_eclapsed = 0.0f;
	UnitAction::run();
}

void Walk::update(float dt)
{
	if (_owner->isOnSurface())
	{
		float move = _owner->move * _owner->moveSpeed;
		if (_eclapsed < UnitAction::recovery)
		{
			_eclapsed += dt;
			move *= std::min(_eclapsed / UnitAction::recovery, 1.0f);
		}
		_owner->setVelocityX(_owner->isFaceRight() ? move : -move);
	}
	else
	{
		Walk::stop();
	}
	UnitAction::update(dt);
}

void Walk::stop()
{
	UnitAction::stop();
	_owner->getModel()->pause();
}

Own<UnitAction> Walk::alloc(Unit* unit)
{
	UnitAction* action = new Walk(unit);
	return MakeOwn(action);
}

// Turn

Turn::Turn(Unit* unit):
UnitAction(ActionSetting::UnitActionTurn, ActionSetting::PriorityTurn, unit)
{ }

void Turn::run()
{
	_owner->setFaceRight(!_owner->isFaceRight());
	if (actionStart)
	{
		actionStart(this);
	}
	if (actionEnd)
	{
		actionEnd(this);
	}
}

Own<UnitAction> Turn::alloc(Unit* unit)
{
	UnitAction* action = new Turn(unit);
	return MakeOwn(action);
}

Idle::Idle(Unit* unit):
UnitAction(ActionSetting::UnitActionIdle, ActionSetting::PriorityIdle, unit)
{
	UnitAction::reaction = ActionSetting::ReactionIdle;
	UnitAction::recovery = ActionSetting::RecoveryIdle;
}

void Idle::run()
{
	Model* model = _owner->getModel();
	model->setSpeed(1.0f);
	model->setLoop(true);
	model->setLook(ActionSetting::LookNormal);
	model->setRecovery(UnitAction::recovery);
	if (!_owner->isOnSurface())
	{
		Model* model = _owner->getModel();
		model->resume(ActionSetting::AnimationJump);
	}
	else
	{
		model->resume(ActionSetting::AnimationIdle);
	}
	UnitAction::run();
}

void Idle::update(float dt)
{
	Model* model = _owner->getModel();
	if (!_owner->isOnSurface())
	{
		model->resume(ActionSetting::AnimationJump);
	}
	else if (_owner->getModel()->getCurrentAnimationName() != ActionSetting::AnimationIdle)
	{
		model->resume(ActionSetting::AnimationIdle);
	}
	UnitAction::update(dt);
}

void Idle::stop()
{
	UnitAction::stop();
	_owner->getModel()->pause();
}

Own<UnitAction> Idle::alloc(Unit* unit)
{
	UnitAction* action = new Idle(unit);
	return MakeOwn(action);
}

Jump::Jump(Unit* unit):
UnitAction(ActionSetting::UnitActionJump, ActionSetting::PriorityJump, unit)
{
	UnitAction::reaction = ActionSetting::ReactionJump;
	UnitAction::recovery = ActionSetting::RecoveryJump;
}

bool Jump::isAvailable()
{
	return _owner->isOnSurface();
}

void Jump::run()
{
	Model* model = _owner->getModel();
	model->setSpeed(1.0f);
	model->setLoop(true);
	model->setLook(ActionSetting::LookNormal);
	model->setRecovery(UnitAction::recovery);
	model->resume(ActionSetting::AnimationJump);
	_current = 0.0f;
	_owner->setVelocityY(_owner->jump);
	Sensor* sensor = _owner->getGroundSensor();
	b2Body* self = _owner->getB2Body();
	b2Body* target = sensor->getSensedBodies()->get(0).to<Body>()->getB2Body();
	b2DistanceInput input =
	{
		b2DistanceProxy(self->GetFixtureList()->GetShape(), 0),
		b2DistanceProxy(target->GetFixtureList()->GetShape(), 0),
		self->GetTransform(),
		target->GetTransform()
	};
	b2DistanceOutput output;
	b2Distance(&output, &input);
	target->ApplyLinearImpulse(
		b2Vec2(-self->GetMass() * self->GetLinearVelocityX(),
			-self->GetMass() * self->GetLinearVelocityY()),
			output.pointB, true);
	UnitAction::run();
}

void Jump::update(float dt)
{
	if (_current < 0.2f) // don`t check for ground for a while, for actor won`t lift immediately.
	{
		_current += dt;
	}
	else
	{
		if (_owner->isOnSurface())
		{
			Model* model = _owner->getModel();
			model->setRecovery(UnitAction::recovery * 0.5f);
			model->resume(ActionSetting::AnimationIdle);
			if (_current < 0.2f + UnitAction::recovery * 0.5f)
			{
				_current += dt;
			}
			else Jump::stop();
		}
		UnitAction::update(dt);
	}
}

void Jump::stop()
{
	UnitAction::stop();
	_owner->getModel()->pause();
}

Own<UnitAction> Jump::alloc(Unit* unit)
{
	UnitAction* action = new Jump(unit);
	return MakeOwn(action);
}

Cancel::Cancel(Unit* unit):
UnitAction(ActionSetting::UnitActionCancel, ActionSetting::PriorityCancel, unit)
{ }

void Cancel::run()
{ }

Own<UnitAction> Cancel::alloc(Unit* unit)
{
	UnitAction* action = new Cancel(unit);
	return MakeOwn(action);
}

// Attack

Attack::Attack(String name, Unit* unit ):
UnitAction(name, ActionSetting::PriorityAttack, unit)
{
	UnitAction::recovery = ActionSetting::RecoveryAttack;
	Model* model = unit->getModel();
	model->handlers[ActionSetting::AnimationAttack] += std::make_pair(this, &Attack::onAnimationEnd);
}

Attack::~Attack()
{
	Model* model = _owner->getModel();
	model->handlers[ActionSetting::AnimationAttack] -= std::make_pair(this, &Attack::onAnimationEnd);
}

void Attack::run()
{
	_current = 0;
	_attackDelay = _owner->getUnitDef()->attackDelay / _owner->attackSpeed;
	_attackEffectDelay = _owner->getUnitDef()->attackEffectDelay / _owner->attackSpeed;
	Model* model = _owner->getModel();
	model->setLoop(false);
	model->setLook(ActionSetting::LookFight);
	model->setRecovery(UnitAction::recovery);
	model->setSpeed(_owner->attackSpeed);
	model->play(ActionSetting::AnimationAttack);
	UnitAction::run();
}

void Attack::update(float dt)
{
	_current += dt;
	if (_attackDelay >= 0 && _current >= _attackDelay)
	{
		_attackDelay = -1;
		if (!_owner->getUnitDef()->sndAttack.empty())
		{
			SharedAudio.play(_owner->getUnitDef()->sndAttack);
		}
		this->onAttack();
	}
	if (_attackEffectDelay >= 0 && _current >= _attackEffectDelay)
	{
		_attackEffectDelay = -1;
		const string& attackEffect = _owner->getUnitDef()->attackEffect;
		if (!attackEffect.empty())
		{
			Vec2 key = _owner->getModel()->getModelDef()->getKeyPoint(UnitDef::AttackKey);
			if (_owner->getModel()->getModelDef()->isFaceRight() != _owner->isFaceRight())
			{
				key.x = -key.x;
			}
			Visual* effect = Visual::create(attackEffect);
			effect->setPosition(key);
			effect->addTo(_owner);
			effect->autoRemove();
			effect->start();
		}
	}
}

void Attack::stop()
{
	UnitAction::stop();
	_owner->getModel()->stop();
}

void Attack::onAnimationEnd(Model* model)
{
	if (UnitAction::isDoing())
	{
		this->stop();
	}
}

float Attack::getDamage(Unit* target)
{
	float factor = SharedData.getDamageFactor(_owner->damageType, target->defenceType);
	float damage = (_owner->attackBase + _owner->attackBonus) * (_owner->attackFactor + factor);
	return damage;
}

Vec2 Attack::getHitPoint(Body* self, Body* target, b2Shape* selfShape)
{
	Vec2 hitPoint;
	float distance = -1;
	for (b2Fixture* f = target->getB2Body()->GetFixtureList();f;f = f->GetNext())
	{
		if (!f->IsSensor())
		{
			b2DistanceInput input =
			{
				b2DistanceProxy(selfShape, 0),
				b2DistanceProxy(f->GetShape(), 0),
				self->getB2Body()->GetTransform(),
				target->getB2Body()->GetTransform()
			};
			b2DistanceOutput output;
			b2Distance(&output, &input);
			if (distance == -1 || distance > output.distance)
			{
				distance = output.distance;
				hitPoint = World::oVal(output.pointB);
			}
		}
	}
	return hitPoint;
}

// MeleeAttack

MeleeAttack::MeleeAttack(Unit* unit):
Attack(ActionSetting::UnitActionMeleeAttack, unit)
{
	_polygon.SetAsBox(World::b2Val(_owner->getWidth()*0.5f), 0.0005f);
}

void MeleeAttack::onAttack()
{
	Sensor* sensor = _owner->getAttackSensor();
	if (sensor)
	{
		ARRAY_START(Body, body, sensor->getSensedBodies())
		{
			Unit* target = DoraCast<Unit>(body->getOwner());
			BLOCK_START
			{
				BREAK_UNLESS(target);
				bool attackRight = _owner->getPosition().x < target->getPosition().x;
				bool faceRight = _owner->isFaceRight();
				BREAK_IF(attackRight != faceRight); // !(hitRight == faceRight || hitLeft == faceLeft)
				Relation relation = SharedData.getRelation(_owner, target);
				BREAK_IF(!_owner->targetAllow.isAllow(relation));
				/* Get hit point */
				Hit* hitUnitAction = s_cast<Hit*>(target->getAction(ActionSetting::UnitActionHit));
				if (hitUnitAction)
				{
					Vec2 hitPoint = UnitDef::usePreciseHit ? Attack::getHitPoint(_owner, target, &_polygon) : Vec2(target->getPosition());
					hitUnitAction->setHitInfo(hitPoint, _owner->attackPower, !attackRight);
				}
				/* Make damage */
				float damage = Attack::getDamage(target);
				target->properties["hp"] -= damage;
				if (damaged)
				{
					damaged(_owner, target, damage);
				}
			}
			BLOCK_END
		}
		ARRAY_END
	}
}

Own<UnitAction> MeleeAttack::alloc(Unit* unit)
{
	UnitAction* action = new MeleeAttack(unit);
	return MakeOwn(action);
}

// RangeAttack

RangeAttack::RangeAttack(Unit* unit):
Attack(ActionSetting::UnitActionRangeAttack, unit)
{ }

Own<UnitAction> RangeAttack::alloc(Unit* unit)
{
	UnitAction* action = new RangeAttack(unit);
	return MakeOwn(action);
}

void RangeAttack::onAttack()
{
	BulletDef* bulletDef = _owner->getBulletDef();
	if (bulletDef)
	{
		Bullet* bullet = Bullet::create(bulletDef, _owner);
		bullet->targetAllow = _owner->targetAllow;
		bullet->hitTarget = std::make_pair(this, &RangeAttack::onHitTarget);
		_owner->getWorld()->addChild(bullet, _owner->getOrder());
	}
}

bool RangeAttack::onHitTarget(Bullet* bullet, Unit* target)
{
	/* Get hit point */
	Hit* hitUnitAction = s_cast<Hit*>(target->getAction(ActionSetting::UnitActionHit));
	if (hitUnitAction)
	{
		b2Shape* shape = bullet->getDetectSensor()->getFixture()->GetShape();
		Vec2 hitPoint = UnitDef::usePreciseHit ? Attack::getHitPoint(_owner, target, shape) : Vec2(target->getPosition());
		bool attackRight = bullet->getVelocityX() > 0.0f;
		hitUnitAction->setHitInfo(hitPoint, _owner->attackPower, !attackRight);
	}
	/* Make damage */
	float damage = Attack::getDamage(target);
	target->properties["hp"] -= damage;
	if (damaged)
	{
		damaged(_owner, target, damage);
	}
	return true;
}

Hit::Hit(Unit* unit):
UnitAction(ActionSetting::UnitActionHit, ActionSetting::PriorityHit, unit),
_effect(nullptr),
_hitFromRight(true),
_attackPower{},
_hitPoint{}
{
	UnitAction::recovery = ActionSetting::RecoveryHit;
	const string& hitEffect = _owner->getUnitDef()->hitEffect;
	if (!hitEffect.empty())
	{
		_effect = Visual::create(hitEffect);
		_effect->addTo(_owner);
	}
	Model* model = unit->getModel();
	model->handlers[ActionSetting::AnimationHit] += std::make_pair(this, &Hit::onAnimationEnd);
}

Hit::~Hit()
{
	Model* model = _owner->getModel();
	model->handlers[ActionSetting::AnimationHit] -= std::make_pair(this, &Hit::onAnimationEnd);
}

void Hit::run()
{
	Model* model = _owner->getModel();
	model->setLook(ActionSetting::LookSad);
	model->setLoop(false);
	model->setRecovery(UnitAction::recovery);
	model->setSpeed(1.0f);
	model->play(ActionSetting::AnimationHit);
	Vec2 key = _owner->convertToNodeSpace(_hitPoint);
	if (_effect)
	{
		_effect->setPosition(key);
		_effect->start();
	}
	_owner->setVelocityX(_hitFromRight ? -_attackPower.x : _attackPower.x);
	_owner->setVelocityY(_attackPower.y);
	_owner->setFaceRight(_hitFromRight);
	UnitAction::run();
}

void Hit::update(float dt)
{ }

void Hit::setHitInfo(const Vec2& hitPoint, const Vec2& attackPower, bool hitFromRight)
{
	_hitPoint = hitPoint;
	_hitFromRight = hitFromRight;
	_attackPower = attackPower;
}

void Hit::onAnimationEnd(Model* model)
{
	if (UnitAction::isDoing())
	{
		this->stop();
	}
}

void Hit::stop()
{
	UnitAction::stop();
	_owner->getModel()->stop();
}

Own<UnitAction> Hit::alloc(Unit* unit )
{
	UnitAction* action = new Hit(unit);
	return MakeOwn(action);
}

Fall::Fall(Unit* unit):
UnitAction(ActionSetting::UnitActionFall, ActionSetting::PriorityFall, unit)
{
	UnitAction::recovery = ActionSetting::RecoveryFall;
	Model* model = unit->getModel();
	model->handlers[ActionSetting::AnimationFall] += std::make_pair(this, &Fall::onAnimationEnd);
}

Fall::~Fall()
{
	Model* model = _owner->getModel();
	model->handlers[ActionSetting::AnimationFall] -= std::make_pair(this, &Fall::onAnimationEnd);
}

void Fall::run()
{
	Model* model = _owner->getModel();
	model->setLook(ActionSetting::LookFallen);
	model->setLoop(false);
	model->setRecovery(UnitAction::recovery);
	model->setSpeed(1.0f);
	model->play(ActionSetting::AnimationFall);
	const string& hitEffect = _owner->getUnitDef()->hitEffect;
	if (!hitEffect.empty())
	{
		Visual* effect = Visual::create(hitEffect);
		effect->addTo(_owner);
		effect->autoRemove();
		effect->start();
	}
	if (!_owner->getUnitDef()->sndDeath.empty())
	{
		SharedAudio.play(_owner->getUnitDef()->sndDeath);
	}
	UnitAction::run();
}

void Fall::update(float dt)
{ }

void Fall::stop()
{
	UnitAction::stop();
	_owner->getModel()->stop();
}

void Fall::onAnimationEnd(Model* model)
{
	if (UnitAction::isDoing())
	{
		this->stop();
	}
}

Own<UnitAction> Fall::alloc(Unit* unit)
{
	UnitAction* action = new Fall(unit);
	return MakeOwn(action);
}

const Slice ActionSetting::AnimationWalk = "walk"_slice;
const Slice ActionSetting::AnimationAttack = "attack"_slice;
const Slice ActionSetting::AnimationIdle = "idle"_slice;
const Slice ActionSetting::AnimationJump = "jump"_slice;
const Slice ActionSetting::AnimationHit = "hit"_slice;
const Slice ActionSetting::AnimationFall = "fall"_slice;

const Slice ActionSetting::UnitActionWalk = "walk"_slice;
const Slice ActionSetting::UnitActionTurn = "turn"_slice;
const Slice ActionSetting::UnitActionMeleeAttack = "meleeAttack"_slice;
const Slice ActionSetting::UnitActionRangeAttack = "rangeAttack"_slice;
const Slice ActionSetting::UnitActionIdle = "idle"_slice;
const Slice ActionSetting::UnitActionCancel = "cancel"_slice;
const Slice ActionSetting::UnitActionJump = "jump"_slice;
const Slice ActionSetting::UnitActionHit = "hit"_slice;
const Slice ActionSetting::UnitActionFall = "fall"_slice;

typedef Own<UnitAction> (*UnitActionFunc)(Unit* unit);
static const unordered_map<string,UnitActionFunc> g_createFuncs =
{
	{ActionSetting::UnitActionWalk, &Walk::alloc},
	{ActionSetting::UnitActionTurn, &Turn::alloc},
	{ActionSetting::UnitActionMeleeAttack, &MeleeAttack::alloc},
	{ActionSetting::UnitActionRangeAttack, &RangeAttack::alloc},
	{ActionSetting::UnitActionIdle, &Idle::alloc},
	{ActionSetting::UnitActionCancel, &Cancel::alloc},
	{ActionSetting::UnitActionJump, &Jump::alloc},
	{ActionSetting::UnitActionHit, &Hit::alloc},
	{ActionSetting::UnitActionFall, &Fall::alloc}
};

Own<UnitAction> UnitAction::alloc(String name, Unit* unit)
{
	auto it = _actionDefs.find(name);
	if (it != _actionDefs.end())
	{
		return it->second->toAction(unit);
	}
	else
	{
		auto it = g_createFuncs.find(name);
		if (it != g_createFuncs.end())
		{
			return it->second(unit);
		}
	}
	return Own<UnitAction>();
}

int ActionSetting::PriorityIdle = 0;
int ActionSetting::PriorityWalk = 1;
int ActionSetting::PriorityTurn = 2;
int ActionSetting::PriorityJump = 2;
int ActionSetting::PriorityAttack = 3;
int ActionSetting::PriorityCancel = std::numeric_limits<int>::max();
int ActionSetting::PriorityHit = 4;
int ActionSetting::PriorityFall = 5;

float ActionSetting::ReactionWalk = 1.5f;
float ActionSetting::ReactionIdle = 2.0f;
float ActionSetting::ReactionJump = 1.5f;

float ActionSetting::RecoveryWalk = 0.1f;
float ActionSetting::RecoveryAttack = 0.2f;
float ActionSetting::RecoveryIdle = 0.1f;
float ActionSetting::RecoveryJump = 0.2f;
float ActionSetting::RecoveryHit = 0.05f;
float ActionSetting::RecoveryFall = 0.05f;

const Slice ActionSetting::LookNormal = "normal"_slice;
const Slice ActionSetting::LookFight = "fight"_slice;
const Slice ActionSetting::LookSad = "sad"_slice;
const Slice ActionSetting::LookFallen = "fallen"_slice;

NS_DOROTHY_PLATFORMER_END