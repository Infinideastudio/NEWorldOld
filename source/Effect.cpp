#include <math.h>

#include "Definitions.h"
#include "Effect.h"


namespace Effect {

	void readScreen(int x, int y, int w, int h, uint8_t* data) {

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

	}

	void writeScreen(int x, int y, int w, int h, uint8_t* data) {

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glRasterPos2i(x, y + h);
		glDrawPixels(w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

	}


	void gray(int w, int h, uint8_t* src, uint8_t* dst) {

		uintptr_t size = w * h * 3;

		for (uintptr_t i = 0; i < size; i += 3) {

			dst[i] = dst[i + 1] = dst[i + 2]
				= 0.299f * src[i] + 0.587f * src[i + 1] + 0.114f * src[i + 2];

		}

	}

	void blurGaussian(int w, int h, uint8_t* src, uint8_t* dst, int r) {

		int size = r * 2 + 1;
		int size2 = size * size;

		float* mat = new float[size2];

		float sigma = (r - 1) * 0.3f + 0.8f;
		float sigma2 = -0.5f * (sigma * sigma);
		float sum = 0.0f;

		for (int i = 0, index = 0; i < size; i++) {

			int dy = i - r;
			int dy2 = dy * dy;

			for (int j = 0; j < size; j++, index++) {

				int dx = j - r;
				int dx2 = dx * dx;

				float x = dx2 + dy2;
				float val = ::expf(sigma2 * x);

				mat[index] = val;

				sum += val;

			}

		}

		sum = 1.0f / sum;

		for (int i = 0; i < size2; i++) {

			mat[i] *= sum;

		}

		for (int i = 0; i < w * h * 3; i += 3) {

			dst[i] = dst[i + 1] = dst[i + 2] = 0;

		}

		for (int i = 0, index = 0; i < size; i++) {

			int dy = i - r;

			for (int j = 0; j < size; j++, index++) {

				int dx = j - r;

				float val = mat[index];

				for (int k = 0; k < h; k++) {

					int y = k + dy;
					if (y < 0) {

						y = 0;

					}
					if (y >= h) {

						y = h - 1;

					}

					for (int l = 0; l < w; l++) {

						int x = l + dx;
						if (x < 0) {

							x = 0;

						}
						if (x >= w) {

							x = w - 1;

						}

						int indexsrc = (k * w + l) * 3;
						int indexdst = (y * w + x) * 3;

						dst[indexdst] += val * src[indexsrc];
						dst[indexdst + 1] += val * src[indexsrc + 1];
						dst[indexdst + 2] += val * src[indexsrc + 2];

					}

				}

			}

		}

		delete mat;

	}

}
