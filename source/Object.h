#pragma once
#include "Definitions.h"
#include "Vec3.h"
//Object ����������ƶ����塢�Ǿ�̬����Ļ���
class Object {
public:
	Object(Vec3d pos_) :pos(pos_), VBO(0), vtxs(0) {};
	virtual ~Object() {};
	virtual void render() const = 0;
	const Vec3d getPos() const { return pos; }
protected:
	int _id;
	Vec3d pos;//λ��
	vtxCount vtxs;
	VBOID VBO;
};