#include "Definitions.h"
#ifndef EFFECT_H
#define EFFECT_H
namespace Effect {
	void readScreen(int x, int y, int w, int h, uint8_t* data);
	void writeScreen(int x, int y, int w, int h, uint8_t* data);

	void gray(int w, int h, uint8_t* src, uint8_t* dst);
	void blurGaussian(int w, int h, uint8_t* src, uint8_t* dst, int r);
}
#endif