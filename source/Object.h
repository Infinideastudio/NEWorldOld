#pragma once
#include "Definitions.h"
#include "Vec3.h"
//Object 所有生物、可移动物体、非静态方块的基类
class Object {
public:
	Object(Vec3d pos_) :pos(pos_), VBO(0), vtxs(0) {};
	virtual ~Object() {};
	virtual void render() const = 0;
	const Vec3d getPos() const { return pos; }
protected:
	int _id;
	Vec3d pos;//位置
	vtxCount vtxs;
	VBOID VBO;
};