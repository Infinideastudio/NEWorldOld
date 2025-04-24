#pragma once
#include "Definitions.h"
#include "Renderer.h"

class Object {
public:
	Object(double x, double y, double z, Renderer::VertexBuffer&& vbo) : _x(x), _y(y), _z(z), vbo(std::move(vbo)) {};
	virtual ~Object() {};
	virtual void render() const = 0;
	const double getXPos() const { return _x; }
	const double getYPos() const { return _y; }
	const double getZPos() const { return _z; }

protected:
	int _id;
	double _x, _y, _z;
	Renderer::VertexBuffer vbo;
};
