#include "Effect.h"
#include "Definitions.h"
#include <math.h>


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
				= (uint8_t)(0.299f * src[i]
					+ 0.587f * src[i + 1]
					+ 0.114f * src[i + 2]);

		}

	}

	void blurGaussianX(int w, int h, uint8_t* src, uint8_t* dst, int rx) {

		int size = rx * 2 + 1;

		float* mat = new float[size];

		float sigma = (rx - 1) * 0.3f + 0.8f;
		float sigma2 = -0.5f / (sigma * sigma);
		float sum = 0.0f;

		for (int i = 0; i < size; i++) {

			int x = i - rx;
			int x2 = x * x;

			float val = ::expf(sigma2 * x2);

			mat[i] = val;

			sum += val;

		}

		sum = 1.0f / sum;

		for (int i = 0; i < size; i++) {

			mat[i] *= sum;

		}

		for (int i = 0; i < w * h * 3; i += 3) {

			dst[i] = dst[i + 1] = dst[i + 2] = 0;

		}

		for (int i = 0; i < size; i++) {

			int dx = i - rx;

			float val = mat[i];

			for (int j = 0; j < w; j++) {

				int x = j + dx;
				if (x < 0) {

					x = 0;

				}
				if (x >= w) {

					x = w - 1;

				}

				for (int k = 0; k < h; k++) {

					int indexsrc = (k * w + j) * 3;
					int indexdst = (k * w + x) * 3;

					dst[indexdst] += (uint8_t)(val * src[indexsrc]);
					dst[indexdst + 1] += (uint8_t)(val * src[indexsrc + 1]);
					dst[indexdst + 2] += (uint8_t)(val * src[indexsrc + 2]);

				}

			}

		}

		delete mat;

	}

	void blurGaussianY(int w, int h, uint8_t* src, uint8_t* dst, int ry) {

		int size = ry * 2 + 1;

		float* mat = new float[size];

		float sigma = (ry - 1) * 0.3f + 0.8f;
		float sigma2 = -0.5f / (sigma * sigma);
		float sum = 0.0f;

		for (int i = 0; i < size; i++) {

			int y = i - ry;
			int y2 = y * y;

			float val = ::expf(sigma2 * y2);

			mat[i] = val;

			sum += val;

		}

		sum = 1.0f / sum;

		for (int i = 0; i < size; i++) {

			mat[i] *= sum;

		}

		for (int i = 0; i < w * h * 3; i += 3) {

			dst[i] = dst[i + 1] = dst[i + 2] = 0;

		}

		for (int i = 0; i < size; i++) {

			int dy = i - ry;

			float val = mat[i];

			for (int j = 0; j < h; j++) {

				int y = j + dy;
				if (y < 0) {

					y = 0;

				}
				if (y >= h) {

					y = h - 1;

				}

				for (int k = 0; k < w; k++) {

					int indexsrc = (j * w + k) * 3;
					int indexdst = (y * w + k) * 3;

					dst[indexdst] += (uint8_t)(val * src[indexsrc]);
					dst[indexdst + 1] += (uint8_t)(val * src[indexsrc + 1]);
					dst[indexdst + 2] += (uint8_t)(val * src[indexsrc + 2]);

				}

			}

		}

		delete mat;

	}

}
