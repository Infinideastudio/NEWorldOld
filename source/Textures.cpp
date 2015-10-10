#include "Textures.h"
#include <fstream>

int BLOCKTEXTURE_SIZE, BLOCKTEXTURE_UNITSIZE, BLOCKTEXTURE_UNITS;

namespace Textures {

	void Init() {
		BLOCKTEXTURE_SIZE = 256;
		BLOCKTEXTURE_UNITSIZE = 32;
		BLOCKTEXTURE_UNITS = 8;
	}

	ubyte getTextureIndex(block blockname, ubyte side) {
		switch (blockname) {
		case blocks::AIR:
			return AIR;
		case blocks::ROCK:
			return ROCK;
		case blocks::GRASS:
			switch (side) {
			case 1:
				return GRASS_TOP;
			case 2:
				return GRASS_SIDE;
			case 3:
				return DIRT;
			}
		case blocks::DIRT:
			return DIRT;
		case blocks::STONE:
			return STONE;
		case blocks::PLANK:
			return PLANK;
		case blocks::WOOD:
			switch (side) {
			case 1:
				return WOOD_TOP;
			case 2:
				return WOOD_SIDE;
			case 3:
				return WOOD_TOP;
			}
		case blocks::BEDROCK:
			return BEDROCK;
		case blocks::LEAF:
			return LEAF;
		case blocks::GLASS:
			return GLASS;
		case blocks::WATER:
			return WATER;
		case blocks::LAVA:
			return LAVA;
		case blocks::GLOWSTONE:
			return GLOWSTONE;
		case blocks::SAND:
			return SAND;
		case blocks::CEMENT:
			return CEMENT;
		case blocks::ICE:
			return ICE;
		case blocks::COAL:
			return COAL;
		case blocks::IRON:
			return IRON;
		default:
			return UNKNOWN;
		}
	}

	double getTexcoordX(block iblock, ubyte side) {
		//return ((getTextureIndex(iblock, side) - 1) % (BLOCKTEXTURE_SIZE / BLOCKTEXTURE_UNITSIZE))*(BLOCKTEXTURE_UNITSIZE / (double)BLOCKTEXTURE_SIZE);
		return ((getTextureIndex(iblock, side) - 1) & 7) / 8.0;
	}

	double getTexcoordY(block iblock, ubyte side) {
		//return (int((getTextureIndex(iblock, side) - 1) / (BLOCKTEXTURE_SIZE / (double)BLOCKTEXTURE_UNITSIZE)))*(BLOCKTEXTURE_UNITSIZE / (double)BLOCKTEXTURE_SIZE);
		return ((getTextureIndex(iblock, side) - 1) >> 3) / 8.0;
	}

	TEXTURE_RGB LoadRGBImage(string Filename) {
		unsigned char col[3];
		unsigned int ind = 0;
		TEXTURE_RGB bitmap; //返回位图
		bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
		std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in); //位图文件（二进制）
		if (!bmpfile.is_open()) {
			printf("[console][Warning] Cannot load %s\n", Filename.c_str());
			return TEXTURE_RGB();
		}
		BITMAPINFOHEADER bih; //各种关于位图的参数
		BITMAPFILEHEADER bfh; //各种关于文件的参数
							  //开始读取
		bmpfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER));
		bmpfile.read((char*)&bih, sizeof(BITMAPINFOHEADER));
		bitmap.sizeX = bih.biWidth;
		bitmap.sizeY = bih.biHeight;
		bitmap.buffer = unique_ptr<ubyte[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 3]);
		for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
			//把BGR格式转换为RGB格式
			bmpfile.read((char*)col, 3);
			bitmap.buffer.get()[ind++] = col[2]; //R
			bitmap.buffer.get()[ind++] = col[1]; //G
			bitmap.buffer.get()[ind++] = col[0]; //B
		}
		bmpfile.close();
		return bitmap;
	}

	TEXTURE_RGBA LoadRGBAImage(string Filename, string MkFilename) {
		unsigned char col[3];
		unsigned int ind = 0;
		TEXTURE_RGBA bitmap; //返回位图
		bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
		std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in); //位图文件（二进制）
		std::ifstream maskfile(MkFilename, std::ios::binary | std::ios::in); //遮罩位图文件（二进制）
		if (!bmpfile.is_open()) {
			printf("[console][Warning] Cannot load %s\n", Filename.c_str());
			return TEXTURE_RGBA();
		}
		BITMAPFILEHEADER bfh; //各种关于文件的参数
		BITMAPINFOHEADER bih; //各种关于位图的参数
							  //开始读取
		maskfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER)); //这两个是占位mask文件的
		maskfile.read((char*)&bih, sizeof(BITMAPINFOHEADER)); //到了后面mask可以直接从颜色部分开始读取
		bmpfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER)); //真正的info以这个bmp文件为准
		bmpfile.read((char*)&bih, sizeof(BITMAPINFOHEADER)); //它将覆盖之前从mask文件读出来的info数据
		bitmap.sizeX = bih.biWidth;
		bitmap.sizeY = bih.biHeight;
		bitmap.buffer = unique_ptr<ubyte[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 4]);
		bool noMaskFile = MkFilename == "";
		for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
			//把BGR格式转换为RGB格式
			bmpfile.read((char*)col, 3);
			bitmap.buffer.get()[ind++] = col[2]; //R
			bitmap.buffer.get()[ind++] = col[1]; //G
			bitmap.buffer.get()[ind++] = col[0]; //B
			if (noMaskFile) {
				bitmap.buffer.get()[ind++] = 255;
			}
			else {
				//将遮罩图的红色通道反相作为Alpha通道
				maskfile.read((char*)col, 3);
				bitmap.buffer.get()[ind++] = 255 - col[2]; //A
			}
		}
		bmpfile.close();
		maskfile.close();
		return bitmap;
	}

	TextureID LoadRGBTexture(string Filename) {
		TEXTURE_RGB image;
		ubyte *ip, *tp;
		TextureID ret;
		image = LoadRGBImage(Filename);
		glGenTextures(1, &ret);
		glBindTexture(GL_TEXTURE_2D, ret);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.sizeX, image.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image.buffer.get());
		return ret;
	}
	TextureID LoadFontTexture(string Filename) {
		TEXTURE_RGBA Texture;
		TEXTURE_RGB image;
		ubyte *ip, *tp;
		TextureID ret;
		image = LoadRGBImage(Filename);
		Texture.sizeX = image.sizeX;
		Texture.sizeY = image.sizeY;
		Texture.buffer = unique_ptr<ubyte[]>(new unsigned char[image.sizeX * image.sizeY * 4]);
		if (Texture.buffer == nullptr) {
			printf("[console][Warning] Cannot alloc memory when loading %s\n", Filename.c_str());
			return 0;
		}
		ip = image.buffer.get();
		tp = Texture.buffer.get();
		for (unsigned int i = 0; i != image.sizeX*image.sizeY; i++) {
			*tp = 255; tp++;
			*tp = 255; tp++;
			*tp = 255; tp++;
			*tp = 255 - *ip; tp++; ip += 3;
		}
		glGenTextures(1, &ret);
		glBindTexture(GL_TEXTURE_2D, ret);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Texture.sizeX, Texture.sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texture.buffer.get());
		return ret;
	}

	void SaveRGBImage(string filename, TEXTURE_RGB& image) {
		BITMAPFILEHEADER bitmapfileheader;
		BITMAPINFOHEADER bitmapinfoheader;
		bitmapfileheader.bfType = BITMAP_ID;
		bitmapfileheader.bfSize = image.sizeX*image.sizeY * 3 + 54;
		bitmapfileheader.bfReserved1 = 0;
		bitmapfileheader.bfReserved2 = 0;
		bitmapfileheader.bfOffBits = 54;
		bitmapinfoheader.biSize = 40;
		bitmapinfoheader.biWidth = image.sizeX;
		bitmapinfoheader.biHeight = image.sizeY;
		bitmapinfoheader.biPlanes = 1;
		bitmapinfoheader.biBitCount = 24;
		bitmapinfoheader.biCompression = 0;
		bitmapinfoheader.biSizeImage = image.sizeX*image.sizeY * 3;
		bitmapinfoheader.biXPelsPerMeter = 0;
		bitmapinfoheader.biYPelsPerMeter = 0;
		bitmapinfoheader.biClrUsed = 0;
		bitmapinfoheader.biClrImportant = 0;
		ubyte* p;
		ubyte r, g, b;
		std::ofstream ofs(filename, std::ios::out | std::ios::binary);
		ofs.write((char*)&bitmapfileheader, sizeof(bitmapfileheader));
		ofs.write((char*)&bitmapinfoheader, sizeof(bitmapinfoheader));
		p = image.buffer.get();
		for (unsigned int index = 0; index != image.sizeX*image.sizeY; index++) {
			r = *p; p += 1;
			g = *p; p += 1;
			b = *p; p += 1;
			ofs.write((char*)&b, sizeof(ubyte));
			ofs.write((char*)&g, sizeof(ubyte));
			ofs.write((char*)&r, sizeof(ubyte));
		}
		ofs.close();
	}

	TextureID LoadRGBATexture(string Filename, string MkFilename) {
		TextureID ret;
		TEXTURE_RGBA image = LoadRGBAImage(Filename, MkFilename);
		glGenTextures(1, &ret);
		glBindTexture(GL_TEXTURE_2D, ret);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (int)log(BLOCKTEXTURE_UNITSIZE));
		gluBuild2DMipmaps(GL_TEXTURE_2D, 4, image.sizeX, image.sizeY, GL_RGBA, GL_UNSIGNED_BYTE, image.buffer.get());
		return ret;
	}

}