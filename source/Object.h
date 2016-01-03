#pragma once
#include "Definitions.h"

//Object ����������ƶ����塢�Ǿ�̬����Ļ���
class Object {
public:
	Object(double x, double y, double z) :_x(x), _y(y), _z(z), VBO(0), vtxs(0) {};
	virtual ~Object() {};
	virtual void render() const = 0;
	const double getXPos() const { return _x; }
	const double getYPos() const { return _y; }
	const double getZPos() const { return _z; }

protected:
	int _id;
	double _x, _y, _z;  //λ��
	vtxCount vtxs;
	VBOID VBO;

};