#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Textures.h"

namespace World { class chunk; }

namespace ChunkRenderer {
	const int delta[6][3] = { { 1,0,0 },{ -1,0,0 },{ 0,1,0 },{ 0,-1,0 },{ 0,0,1 },{ 0,0,-1 } };

	//�ϲ����һ������ | One face in merge face
	struct QuadPrimitive {
		int x, y, z, length, direction;
		/*
		* ���������ɫ��ͬ��ƽ����������ʱ����������ξͲ��ܺͱ�ķ���ƴ��������
		* �������ͬʱ��ζ���ĸ�������ɫ�Ƿ�ͬ��
		* If the vertexes have different colors (smoothed lighting), the primitive cannot connect with others.
		* This variable also means whether the vertexes have different colors.
		*/
		bool once;
		//������ɫ | Vertex colors
		int col0, col1, col2, col3;
		//����ID | Texture ID
		TextureID tex;
		QuadPrimitive() : x(0), y(0), z(0), length(0), direction(0), once(false),
			tex(Textures::NULLBLOCK), col0(0), col1(0), col2(0), col3(0) {}
	};
	//���ģ�͵��� | Face in depth model
	struct QuadPrimitive_Depth {
		int x, y, z, length, direction;
		QuadPrimitive_Depth() : x(0), y(0), z(0), length(0), direction(0) {}
	};
	void RenderPrimitive(QuadPrimitive& p);
	void RenderPrimitive_Depth(QuadPrimitive_Depth& p);
	void RenderChunk(World::chunk* c);
	void MergeFaceRender(World::chunk* c);
	void RenderDepthModel(World::chunk* c);

}