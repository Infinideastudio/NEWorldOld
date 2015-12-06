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
		return ((getTextureIndex(iblock, side) - 1) & 7) / 8.0;
	}

	double getTexcoordY(block iblock, ubyte side) {
		return ((getTextureIndex(iblock, side) - 1) >> 3) / 8.0;
	}

	void LoadRGBImage(TEXTURE_RGB& tex, string Filename) {
		unsigned int ind = 0;
		TEXTURE_RGB& bitmap = tex; //����λͼ
		bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
		std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in); //λͼ�ļ��������ƣ�
		if (!bmpfile.is_open()) {
			std::stringstream ss; ss << "Cannot load bitmap " << Filename;
			DebugWarning(ss.str()); return;
		}
		BITMAPINFOHEADER bih; //���ֹ���λͼ�Ĳ���
		BITMAPFILEHEADER bfh; //���ֹ����ļ��Ĳ���
							  //��ʼ��ȡ
		bmpfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER));
		bmpfile.read((char*)&bih, sizeof(BITMAPINFOHEADER));
		bitmap.sizeX = bih.biWidth;
		bitmap.sizeY = bih.biHeight;
		bitmap.buffer = unique_ptr<ubyte[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 3]);
		//��ȡ����
		bmpfile.read((char*)bitmap.buffer.get(), bitmap.sizeX*bitmap.sizeY * 3);
		bmpfile.close();
		//�ϲ���ת��
		for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
			//��BGR��ʽת��ΪRGB��ʽ
			unsigned char t = bitmap.buffer[ind];
			bitmap.buffer[ind] = bitmap.buffer[ind + 2];
			bitmap.buffer[ind + 2] = t;
			ind += 3;
		}
	}

	void LoadRGBAImage(TEXTURE_RGBA& tex, string Filename, string MkFilename) {
		unsigned char *rgb = nullptr, *a = nullptr;
		unsigned int ind = 0;
		bool noMaskFile = (MkFilename == "");
		TEXTURE_RGBA& bitmap = tex; //����λͼ
		bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
		std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in); //λͼ�ļ��������ƣ�
		std::ifstream maskfile;
		if (!noMaskFile)maskfile.open(MkFilename, std::ios::binary | std::ios::in); //����λͼ�ļ��������ƣ�
		if (!bmpfile.is_open()) {
			std::stringstream ss; ss << "Cannot load bitmap " << Filename;
			DebugWarning(ss.str()); return;
		}
		if (!noMaskFile && !maskfile.is_open()) {
			std::stringstream ss; ss << "Cannot load bitmap " << MkFilename;
			DebugWarning(ss.str()); return;
		}
		BITMAPFILEHEADER bfh; //���ֹ����ļ��Ĳ���
		BITMAPINFOHEADER bih; //���ֹ���λͼ�Ĳ���
							  //��ʼ��ȡ
		if (!noMaskFile) {
			maskfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER)); //��������ռλmask�ļ���
			maskfile.read((char*)&bih, sizeof(BITMAPINFOHEADER)); //���˺���mask����ֱ�Ӵ���ɫ���ֿ�ʼ��ȡ
		}
		bmpfile.read((char*)&bfh, sizeof(BITMAPFILEHEADER)); //������info�����bmp�ļ�Ϊ׼
		bmpfile.read((char*)&bih, sizeof(BITMAPINFOHEADER)); //��������֮ǰ��mask�ļ���������info����
		bitmap.sizeX = bih.biWidth;
		bitmap.sizeY = bih.biHeight;
		bitmap.buffer = unique_ptr<ubyte[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 4]);
		//��ȡ����
		rgb = new unsigned char[bitmap.sizeX * bitmap.sizeY * 3];
		bmpfile.read((char*)rgb, bitmap.sizeX*bitmap.sizeY * 3);
		bmpfile.close();
		if (!noMaskFile) {
			a = new unsigned char[bitmap.sizeX*bitmap.sizeY * 3];
			maskfile.read((char*)a, bitmap.sizeX*bitmap.sizeY * 3);
			maskfile.close();
		}
		//�ϲ���ת��
		for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
			//��BGR��ʽת��ΪRGB��ʽ
			bitmap.buffer[ind] = rgb[i * 3 + 2];
			bitmap.buffer[ind + 1] = rgb[i * 3 + 1];
			bitmap.buffer[ind + 2] = rgb[i * 3];
			//Alpha
			if (noMaskFile) bitmap.buffer[ind + 3] = 255;
			else bitmap.buffer[ind + 3] = 255 - a[i * 3];
			ind += 4;
		}
	}

	TextureID LoadRGBTexture(string Filename) {
		TEXTURE_RGB image;
		TextureID ret;
		LoadRGBImage(image, Filename);
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
		LoadRGBImage(image, Filename);
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
		bitmapfileheader.bfSize = image.sizeX*image.sizeY * 3 + 54;
		bitmapinfoheader.biWidth = image.sizeX;
		bitmapinfoheader.biHeight = image.sizeY;
		bitmapinfoheader.biSizeImage = image.sizeX*image.sizeY * 3;
		std::ofstream ofs(filename, std::ios::out | std::ios::binary);
		ofs.write((char*)&bitmapfileheader, sizeof(bitmapfileheader));
		ofs.write((char*)&bitmapinfoheader, sizeof(bitmapinfoheader));
		ofs.write((char*)image.buffer.get(), sizeof(ubyte)*image.sizeX*image.sizeY * 3);
		ofs.close();
	}

	TextureID LoadRGBATexture(string Filename, string MkFilename) {
		TextureID ret;
		TEXTURE_RGBA image;
		LoadRGBAImage(image, Filename, MkFilename);
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