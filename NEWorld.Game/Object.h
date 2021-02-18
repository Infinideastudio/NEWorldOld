#pragma once

#include "Definitions.h"
#include <Math/Vector3.h>
#include "Hitbox.h"

//Object 所有生物、可移动物体、非静态方块的基类
class Object {
public:
    Object(double x, double y, double z) : _x(x), _y(y), _z(z), VBO(0), vtxs(0) {};

    virtual ~Object() {};

    virtual void render() const = 0;

    [[nodiscard]] const double getXPos() const { return _x; }

    [[nodiscard]] const double getYPos() const { return _y; }

    [[nodiscard]] const double getZPos() const { return _z; }

protected:
    int _id;
    double _x, _y, _z;  //位置
    vtxCount vtxs;
    VBOID VBO;

};

struct ObjectPhyData {
    Double3 Position{0.0}, Speed{0.0}, Acceleration{0.0};
    Double3 Rotation{0.0}, AngularVelocity{0.0}, AngularAcceleration{0.0};
    Hitbox::AABB Bounds{};

    Hitbox::AABB GetLoseBounds() const noexcept {

    }

    Hitbox::AABB GetExpandedBounds() const noexcept {
        return GetLoseBounds();
    }
};

class ObjectPhyOperator {
public:
    virtual std::unique_ptr<ObjectPhyData> evaluate(const ObjectPhyData&) { return nullptr; }
};

class ObjectStateData {

};

class ObjectManipulatorDecision {

};

class ObjectManipulator {
public:
    virtual std::unique_ptr<ObjectManipulatorDecision> manipulate(const ObjectStateData&) { return nullptr; }
};

class ObjectMultiGoalManipulator: public ObjectManipulator {

};
