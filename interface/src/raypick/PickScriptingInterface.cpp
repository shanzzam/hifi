//
//  Created by Sam Gondelman 10/20/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PickScriptingInterface.h"

#include <QVariant>
#include "GLMHelpers.h"

#include "Application.h"
#include <PickManager.h>

#include "RayPick.h"
#include "StylusPick.h"
#include "ParabolaPick.h"
#include "CollisionPick.h"

#include "SpatialParentFinder.h"
#include "PickTransformNode.h"
#include "MouseTransformNode.h"
#include "avatar/MyAvatarHeadTransformNode.h"
#include "avatar/AvatarManager.h"
#include "NestableTransformNode.h"
#include "avatars-renderer/AvatarTransformNode.h"
#include "EntityTransformNode.h"

#include <ScriptEngine.h>

static const float WEB_TOUCH_Y_OFFSET = 0.105f;  // how far forward (or back with a negative number) to slide stylus in hand
static const glm::vec3 TIP_OFFSET = glm::vec3(0.0f, StylusPick::WEB_STYLUS_LENGTH - WEB_TOUCH_Y_OFFSET, 0.0f);

unsigned int PickScriptingInterface::createPick(const PickQuery::PickType type, const QVariant& properties) {
    switch (type) {
        case PickQuery::PickType::Ray:
            return createRayPick(properties);
        case PickQuery::PickType::Stylus:
            return createStylusPick(properties);
        case PickQuery::PickType::Parabola:
            return createParabolaPick(properties);
        case PickQuery::PickType::Collision:
            return createCollisionPick(properties);
        default:
            return PickManager::INVALID_PICK_ID;
    }
}

PickFilter getPickFilter(unsigned int filter) {
    // FIXME: Picks always intersect visible and collidable things right now
    filter = filter | (PickScriptingInterface::PICK_INCLUDE_VISIBLE() | PickScriptingInterface::PICK_INCLUDE_COLLIDABLE());
    return PickFilter(filter);
}

/**jsdoc
 * A set of properties that can be passed to {@link Picks.createPick} when creating a new ray pick.
 *
 * @typedef {object} Picks.RayPickProperties
 * @property {boolean} [enabled=false] - <code>true</code> if this pick should start enabled, <code>false</code> if it should 
 *     start disabled. Disabled picks do not update their pick results.
 * @property {FilterFlags} [filter=0] - The filter for this pick to use. Construct using {@link Picks} FilterFlags property 
 *     values (e.g., <code>Picks.PICK_DOMAIN_ENTTITIES</code>) combined with <code>|</code> (bitwise OR) operators.
 * @property {number} [maxDistance=0.0] - The maximum distance at which this pick will intersect. A value of <code>0.0</code> 
 *     means no maximum.
 * @property {Uuid} [parentID] - The ID of the parent: an avatar, an entity, or another pick.
 * @property {number} [parentJointIndex=0] - The joint of the parent to parent to, for example, an avatar joint. 
 *     A value of <code>0</code> means no joint.<br />
 *     <em>Used only if <code>parentID</code> is specified.</em>
 * @property {string} [joint] - <code>"Mouse"</code> parents the pick to the mouse; <code>"Avatar"</code> parents the pick to 
 *     the user's avatar head; a joint name parents to the joint in the user's avatar; otherwise, the pick is "static", not 
 *     parented to anything.<br />
 *     <em>Used only if <code>parentID</code> is not specified.</em>
 * @property {Vec3} [position=Vec3.ZERO] - The offset of the ray origin from its parent if parented, otherwise the ray origin 
 *     in world coordinates.
 * @property {Vec3} [posOffset] - Synonym for <code>position</code>.
 * @property {Vec3} [direction] - The offset of the ray direction from its parent's y-axis if parented, otherwise the ray
 *     direction in world coordinates.
 *     <p><strong>Default Value:</strong> <code>Vec3.UP</code> direction if <code>joint</code> is specified, otherwise
 *     <code>-Vec3.UP</code>.</p>
 * @property {Vec3} [dirOffset] - Synonym for <code>direction</code>.
 * @property {Quat} [orientation] - Alternative property for specifying <code>direction</code>. The value is applied to the 
 *     default <code>direction</code> value.
 */
unsigned int PickScriptingInterface::createRayPick(const QVariant& properties) {
    QVariantMap propMap = properties.toMap();

#if defined (Q_OS_ANDROID)
    QString jointName { "" };
    if (propMap["joint"].isValid()) {
        QString jointName = propMap["joint"].toString();
        const QString MOUSE_JOINT = "Mouse";
        if (jointName == MOUSE_JOINT) {
            return PointerEvent::INVALID_POINTER_ID;
        }
    }
#endif

    bool enabled = false;
    if (propMap["enabled"].isValid()) {
        enabled = propMap["enabled"].toBool();
    }

    PickFilter filter = PickFilter();
    if (propMap["filter"].isValid()) {
        filter = getPickFilter(propMap["filter"].toUInt());
    }

    float maxDistance = 0.0f;
    if (propMap["maxDistance"].isValid()) {
        maxDistance = propMap["maxDistance"].toFloat();
    }

    glm::vec3 position = Vectors::ZERO;
    if (propMap["position"].isValid()) {
        position = vec3FromVariant(propMap["position"]);
    } else if (propMap["posOffset"].isValid()) {
        position = vec3FromVariant(propMap["posOffset"]);
    }

    // direction has two defaults to ensure compatibility with older scripts
    // Joint ray picks had default direction = Vec3.UP
    // Static ray picks had default direction = -Vec3.UP
    glm::vec3 direction = propMap["joint"].isValid() ? Vectors::UP : -Vectors::UP;
    if (propMap["orientation"].isValid()) {
        direction = quatFromVariant(propMap["orientation"]) * Vectors::UP;
    } else if (propMap["direction"].isValid()) {
        direction = vec3FromVariant(propMap["direction"]);
    } else if (propMap["dirOffset"].isValid()) {
        direction = vec3FromVariant(propMap["dirOffset"]);
    }

    auto rayPick = std::make_shared<RayPick>(position, direction, filter, maxDistance, enabled);
    setParentTransform(rayPick, propMap);

    return DependencyManager::get<PickManager>()->addPick(PickQuery::Ray, rayPick);
}

/**jsdoc
 * A set of properties that can be passed to {@link Picks.createPick} when creating a new stylus pick.
 *
 * @typedef {object} Picks.StylusPickProperties
 * @property {number} [hand=-1] <code>0</code> for the left hand, <code>1</code> for the right hand, invalid (<code>-1</code>) 
 *     otherwise.
 * @property {boolean} [enabled=false] - <code>true</code> if this pick should start enabled, <code>false</code> if it should
 *     start disabled. Disabled picks do not update their pick results.
 * @property {number} [filter=0] - The filter for this pick to use. Construct using {@link Picks} FilterFlags property
 *     values (e.g., <code>Picks.PICK_DOMAIN_ENTTITIES</code>) combined with <code>|</code> (bitwise OR) operators.
 *     <p><strong>Note:</strong> Stylus picks do not intersect avatars or the HUD.</p>
 * @property {number} [maxDistance=0.0] - The maximum distance at which this pick will intersect. A value of <code>0.0</code>
 *     means no maximum.
 * @property {Vec3} [tipOffset=0,0.095,0] - The position of the stylus tip relative to the hand position at default avatar 
 *     scale.
 */
unsigned int PickScriptingInterface::createStylusPick(const QVariant& properties) {
    QVariantMap propMap = properties.toMap();

    bilateral::Side side = bilateral::Side::Invalid;
    {
        QVariant handVar = propMap["hand"];
        if (handVar.isValid()) {
            side = bilateral::side(handVar.toInt());
        }
    }

    bool enabled = false;
    if (propMap["enabled"].isValid()) {
        enabled = propMap["enabled"].toBool();
    }

    PickFilter filter = PickFilter();
    if (propMap["filter"].isValid()) {
        filter = getPickFilter(propMap["filter"].toUInt());
    }

    float maxDistance = 0.0f;
    if (propMap["maxDistance"].isValid()) {
        maxDistance = propMap["maxDistance"].toFloat();
    }

    glm::vec3 tipOffset = TIP_OFFSET;
    if (propMap["tipOffset"].isValid()) {
        tipOffset = vec3FromVariant(propMap["tipOffset"]);
    }

    return DependencyManager::get<PickManager>()->addPick(PickQuery::Stylus, std::make_shared<StylusPick>(side, filter, maxDistance, enabled, tipOffset));
}

// NOTE: Laser pointer still uses scaleWithAvatar. Until scaleWithAvatar is also deprecated for pointers, scaleWithAvatar should not be removed from the pick API.
/**jsdoc
 * A set of properties that can be passed to {@link Picks.createPick} when creating a new parabola pick.
 *
 * @typedef {object} Picks.ParabolaPickProperties
 * @property {boolean} [enabled=false] - <code>true</code> if this pick should start enabled, <code>false</code> if it should 
 *     start disabled. Disabled picks do not update their pick results.
 * @property {number} [filter=0] - The filter for this pick to use. Construct using {@link Picks} FilterFlags property 
 *     values (e.g., <code>Picks.PICK_DOMAIN_ENTTITIES</code>) combined with <code>|</code> (bitwise OR) operators.
 * @property {number} [maxDistance=0.0] - The maximum distance at which this pick will intersect. A value of <code>0.0</code> 
 *     means no maximum.
 * @property {Uuid} [parentID] - The ID of the parent: an avatar, an entity, or another pick.
 * @property {number} [parentJointIndex=0] - The joint of the parent to parent to, for example, an avatar joint.
 *     A value of <code>0</code> means no joint.<br />
 *     <em>Used only if <code>parentID</code> is specified.</em>
 * @property {string} [joint] - <code>"Mouse"</code> parents the pick to the mouse; <code>"Avatar"</code> parents the pick to 
 *     the user's avatar head; a joint name parents to the joint in the user's avatar; otherwise, the pick is "static", not 
 *     parented to anything.
 *     <em>Used only if <code>parentID</code> is not specified.</em>
 * @property {Vec3} [position=Vec3.ZERO] - The offset of the parabola origin from its parent if parented, otherwise the 
 *     parabola origin in world coordinates.
 * @property {Vec3} [posOffset] - Synonym for <code>position</code>.
 * @property {Vec3} [direction] - The offset of the parabola direction from its parent's y-axis if parented, otherwise the 
 *     parabola direction in world coordinates.
 *     <p><strong>Default Value:</strong> <code>Vec3.UP</code> direction if <code>joint</code> is specified, otherwise
 *     <code>Vec3.FRONT</code>.</p>
 * @property {Vec3} [dirOffset] - Synonym for <code>direction</code>.
 * @property {Quat} [orientation] - Alternative property for specifying <code>direction</code>. The value is applied to the 
 *     default <code>direction</code> value.
 * @property {number} [speed=1] - The initial speed of the parabola in m/s, i.e., the initial speed of a virtual projectile 
 *     whose trajectory defines the parabola.
 * @property {Vec3} [accelerationAxis=-Vec3.UP] - The acceleration of the parabola in m/s<sup>2</sup>, i.e., the acceleration
 *     of a virtual projectile whose trajectory defines the parabola, both magnitude and direction.
 * @property {boolean} [rotateAccelerationWithAvatar=true] - <code>true</code> if the acceleration axis should rotate with the
 *     avatar about the avatar's y-axis, <code>false</code> if it shouldn't.
 * @property {boolean} [rotateAccelerationWithParent=false] - <code>true</code> if the acceleration axis should rotate with the
 *     parent about the parent's y-axis, if available.
 * @property {boolean} [scaleWithParent=true] - <code>true</code> if the velocity and acceleration of the pick should scale
 *     with the avatar or other parent.
 * @property {boolean} [scaleWithAvatar=true] - Synonym for <code>scalewithParent</code>.
 *     <p class="important">Deprecated: This property is deprecated and will be removed.</p>
 */
unsigned int PickScriptingInterface::createParabolaPick(const QVariant& properties) {
    QVariantMap propMap = properties.toMap();

    bool enabled = false;
    if (propMap["enabled"].isValid()) {
        enabled = propMap["enabled"].toBool();
    }

    PickFilter filter = PickFilter();
    if (propMap["filter"].isValid()) {
        filter = getPickFilter(propMap["filter"].toUInt());
    }

    float maxDistance = 0.0f;
    if (propMap["maxDistance"].isValid()) {
        maxDistance = propMap["maxDistance"].toFloat();
    }

    float speed = 1.0f;
    if (propMap["speed"].isValid()) {
        speed = propMap["speed"].toFloat();
    }

    glm::vec3 accelerationAxis = -Vectors::UP;
    if (propMap["accelerationAxis"].isValid()) {
        accelerationAxis = vec3FromVariant(propMap["accelerationAxis"]);
    }

    bool rotateAccelerationWithAvatar = true;
    if (propMap["rotateAccelerationWithAvatar"].isValid()) {
        rotateAccelerationWithAvatar = propMap["rotateAccelerationWithAvatar"].toBool();
    }

    bool rotateAccelerationWithParent = false;
    if (propMap["rotateAccelerationWithParent"].isValid()) {
        rotateAccelerationWithParent = propMap["rotateAccelerationWithParent"].toBool();
    }

    bool scaleWithParent = true;
    if (propMap["scaleWithParent"].isValid()) {
        scaleWithParent = propMap["scaleWithParent"].toBool();
    } else if (propMap["scaleWithAvatar"].isValid()) {
        scaleWithParent = propMap["scaleWithAvatar"].toBool();
    }

    glm::vec3 position = Vectors::ZERO;
    glm::vec3 direction = propMap["joint"].isValid() ? Vectors::UP : -Vectors::FRONT;
    if (propMap["position"].isValid()) {
        position = vec3FromVariant(propMap["position"]);
    } else if (propMap["posOffset"].isValid()) {
        position = vec3FromVariant(propMap["posOffset"]);
    }
    if (propMap["orientation"].isValid()) {
        direction = quatFromVariant(propMap["orientation"]) * Vectors::UP;
    } else if (propMap["direction"].isValid()) {
        direction = vec3FromVariant(propMap["direction"]);
    } else if (propMap["dirOffset"].isValid()) {
        direction = vec3FromVariant(propMap["dirOffset"]);
    }

    auto parabolaPick = std::make_shared<ParabolaPick>(position, direction, speed, accelerationAxis,
        rotateAccelerationWithAvatar, rotateAccelerationWithParent, scaleWithParent, filter, maxDistance, enabled);
    setParentTransform(parabolaPick, propMap);
    return DependencyManager::get<PickManager>()->addPick(PickQuery::Parabola, parabolaPick);
}


/**jsdoc
 * A set of properties that can be passed to {@link Picks.createPick} when creating a new collision pick.
 *
 * @typedef {object} Picks.CollisionPickProperties
 * @property {boolean} [enabled=false] - <code>true</code> if this pick should start enabled, <code>false</code> if it should
 *     start disabled. Disabled picks do not update their pick results.
 * @property {FilterFlags} [filter=0] - The filter for this pick to use. Construct using {@link Picks} FilterFlags property
 *     values (e.g., <code>Picks.PICK_DOMAIN_ENTTITIES</code>) combined with <code>|</code> (bitwise OR) operators.
 *     <p><strong>Note:</strong> Collision picks do not intersect the HUD.</p>
 * @property {number} [maxDistance=0.0] - The maximum distance at which this pick will intersect. A value of <code>0.0</code>
 *     means no maximum.
 * @property {Uuid} [parentID] - The ID of the parent: an avatar, an entity, or another pick.
 * @property {number} [parentJointIndex=0] - The joint of the parent to parent to, for example, an avatar joint.
 *     A value of <code>0</code> means no joint.<br />
 *     <em>Used only if <code>parentID</code> is specified.</em>
 * @property {string} [joint] - <code>"Mouse"</code> parents the pick to the mouse; <code>"Avatar"</code> parents the pick to
 *     the user's avatar head; a joint name parents to the joint in the user's avatar; otherwise, the pick is "static", not
 *     parented to anything.<br />
 *     <em>Used only if <code>parentID</code> is not specified.</em>
 * @property {boolean} [scaleWithParent=true] - <code>true</code> to scale the pick's dimensions and threshold according to the 
 *     scale of the parent.
 *
 * @property {Shape} shape - The collision region's shape and size. Dimensions are in world coordinates but scale with the 
 *     parent if defined.
 * @property {Vec3} position - The position of the collision region, relative to the parent if defined.
 * @property {Quat} orientation - The orientation of the collision region, relative to the parent if defined.
 * @property {number} threshold - The approximate minimum penetration depth for a test object to be considered in contact with
 *     the collision region. The depth is in world coordinates but scales with the parent if defined.
 * @property {CollisionMask} [collisionGroup=8] - The type of objects the collision region collides as. Objects whose collision
 *     masks overlap with the region's collision group are considered to be colliding with the region.
 */
unsigned int PickScriptingInterface::createCollisionPick(const QVariant& properties) {
    QVariantMap propMap = properties.toMap();

    bool enabled = false;
    if (propMap["enabled"].isValid()) {
        enabled = propMap["enabled"].toBool();
    }

    PickFilter filter = PickFilter();
    if (propMap["filter"].isValid()) {
        filter = getPickFilter(propMap["filter"].toUInt());
    }

    float maxDistance = 0.0f;
    if (propMap["maxDistance"].isValid()) {
        maxDistance = propMap["maxDistance"].toFloat();
    }

    bool scaleWithParent = true;
    if (propMap["scaleWithParent"].isValid()) {
        scaleWithParent = propMap["scaleWithParent"].toBool();
    }

    CollisionRegion collisionRegion(propMap);
    auto collisionPick = std::make_shared<CollisionPick>(filter, maxDistance, enabled, scaleWithParent, collisionRegion, qApp->getPhysicsEngine());
    setParentTransform(collisionPick, propMap);

    return DependencyManager::get<PickManager>()->addPick(PickQuery::Collision, collisionPick);
}

void PickScriptingInterface::enablePick(unsigned int uid) {
    DependencyManager::get<PickManager>()->enablePick(uid);
}

void PickScriptingInterface::disablePick(unsigned int uid) {
    DependencyManager::get<PickManager>()->disablePick(uid);
}

void PickScriptingInterface::removePick(unsigned int uid) {
    DependencyManager::get<PickManager>()->removePick(uid);
}

QVariantMap PickScriptingInterface::getPrevPickResult(unsigned int uid) {
    QVariantMap result;
    auto pickResult = DependencyManager::get<PickManager>()->getPrevPickResult(uid);
    if (pickResult) {
        result = pickResult->toVariantMap();
    }
    return result;
}

void PickScriptingInterface::setPrecisionPicking(unsigned int uid, bool precisionPicking) {
    DependencyManager::get<PickManager>()->setPrecisionPicking(uid, precisionPicking);
}

void PickScriptingInterface::setIgnoreItems(unsigned int uid, const QScriptValue& ignoreItems) {
    DependencyManager::get<PickManager>()->setIgnoreItems(uid, qVectorQUuidFromScriptValue(ignoreItems));
}

void PickScriptingInterface::setIncludeItems(unsigned int uid, const QScriptValue& includeItems) {
    DependencyManager::get<PickManager>()->setIncludeItems(uid, qVectorQUuidFromScriptValue(includeItems));
}

bool PickScriptingInterface::isLeftHand(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isLeftHand(uid);
}

bool PickScriptingInterface::isRightHand(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isRightHand(uid);
}

bool PickScriptingInterface::isMouse(unsigned int uid) {
    return DependencyManager::get<PickManager>()->isMouse(uid);
}

QScriptValue pickTypesToScriptValue(QScriptEngine* engine, const PickQuery::PickType& pickType) {
    return pickType;
}

void pickTypesFromScriptValue(const QScriptValue& object, PickQuery::PickType& pickType) {
    pickType = static_cast<PickQuery::PickType>(object.toUInt16());
}

void PickScriptingInterface::registerMetaTypes(QScriptEngine* engine) {
    QScriptValue pickTypes = engine->newObject();
    auto metaEnum = QMetaEnum::fromType<PickQuery::PickType>();
    for (int i = 0; i < PickQuery::PickType::NUM_PICK_TYPES; ++i) {
        pickTypes.setProperty(metaEnum.key(i), metaEnum.value(i));
    }
    engine->globalObject().setProperty("PickType", pickTypes);

    qScriptRegisterMetaType(engine, pickTypesToScriptValue, pickTypesFromScriptValue);
}

unsigned int PickScriptingInterface::getPerFrameTimeBudget() const {
    return DependencyManager::get<PickManager>()->getPerFrameTimeBudget();
}

void PickScriptingInterface::setPerFrameTimeBudget(unsigned int numUsecs) {
    DependencyManager::get<PickManager>()->setPerFrameTimeBudget(numUsecs);
}

void PickScriptingInterface::setParentTransform(std::shared_ptr<PickQuery> pick, const QVariantMap& propMap) {
    QUuid parentUuid;
    int parentJointIndex = 0;
    auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();

    if (propMap["parentID"].isValid()) {
        parentUuid = propMap["parentID"].toUuid();
        if (propMap["parentJointIndex"].isValid()) {
            parentJointIndex = propMap["parentJointIndex"].toInt();
        }
    } else if (propMap["joint"].isValid()) {
        QString joint = propMap["joint"].toString();
        if (joint == "Mouse") {
            pick->parentTransform = std::make_shared<MouseTransformNode>();
            pick->setJointState(PickQuery::JOINT_STATE_MOUSE);
            return;
        } else if (joint == "Avatar") {
            pick->parentTransform = std::make_shared<MyAvatarHeadTransformNode>();
            return;
        } else {
            parentUuid = myAvatar->getSessionUUID();
            parentJointIndex = myAvatar->getJointIndex(joint);
        }
    }

    if (parentUuid == myAvatar->getSessionUUID()) {
        if (parentJointIndex == CONTROLLER_LEFTHAND_INDEX || parentJointIndex == CAMERA_RELATIVE_CONTROLLER_LEFTHAND_INDEX) {
            pick->setJointState(PickQuery::JOINT_STATE_LEFT_HAND);
        } else if (parentJointIndex == CONTROLLER_RIGHTHAND_INDEX || parentJointIndex == CAMERA_RELATIVE_CONTROLLER_RIGHTHAND_INDEX) {
            pick->setJointState(PickQuery::JOINT_STATE_RIGHT_HAND);
        }

        pick->parentTransform = std::make_shared<AvatarTransformNode>(myAvatar, parentJointIndex);
    } else if (!parentUuid.isNull()) {
        // Infer object type from parentID
        // For now, assume a QUuid is a SpatiallyNestable. This should change when picks are converted over to QUuids.
        bool success;
        std::weak_ptr<SpatiallyNestable> nestablePointer = DependencyManager::get<SpatialParentFinder>()->find(parentUuid, success, nullptr);
        auto sharedNestablePointer = nestablePointer.lock();

        if (success && sharedNestablePointer) {
            NestableType nestableType = sharedNestablePointer->getNestableType();
            if (nestableType == NestableType::Avatar) {
                pick->parentTransform = std::make_shared<AvatarTransformNode>(std::static_pointer_cast<Avatar>(sharedNestablePointer), parentJointIndex);
            } else if (nestableType == NestableType::Entity) {
                pick->parentTransform = std::make_shared<EntityTransformNode>(std::static_pointer_cast<EntityItem>(sharedNestablePointer), parentJointIndex);
            } else {
                pick->parentTransform = std::make_shared<NestableTransformNode>(nestablePointer, parentJointIndex);
            }
        }
    } else {
        unsigned int pickID = propMap["parentID"].toUInt();

        if (pickID != 0) {
            pick->parentTransform = std::make_shared<PickTransformNode>(pickID);
        }
    }
}
