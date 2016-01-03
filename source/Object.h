#pragma once
#include "stdinclude.h"
#include "Definitions.h"


namespace InfinideaStudio
{
	namespace NEWorld
	{
		//Object 所有生物、可移动物体、非静态方块的基类
		class Object
		{
		public:
			Object(double x, double y, double z) :_x(x), _y(y), _z(z), VBO(0), vtxs(0) { };
			virtual ~Object() { };
			virtual void render() const = 0;
			const double getXPos() const { return _x; }
			const double getYPos() const { return _y; }
			const double getZPos() const { return _z; }

		protected:
			int _id;
			double _x, _y, _z;  //位置
			vtxCount vtxs;
			VBOID VBO;

		};
	}
}