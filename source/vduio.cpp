#include "vduio.h"

bool TRU_MODE_NO_CREATE = false;


Tree::Tree(std::string fname){
	//Try to open the file with out mood to check exsistance
	f.open(fname, std::ios::out);
	if (f.bad()) {
		f.close();
		if (!TRU_MODE_NO_CREATE) {
			f.open(fname, std::ios::out | std::ios::binary);
			index[0] = 2;
			index[1] = 3;
			f.write((char*)&index, sizeof(index));
			//we know it sized 2 chunks,so write it again to init
			f.write((char*)&index, sizeof(index));
			f.close();
		}
	}
	f.open(fname, std::ios::in | std::ios::out | std::ios::binary);
	//Import the index
	f.read((char*)&index, sizeof(index));
	//Load the dir
	//Note that we will only load the path here
	char* _ = new char[(index[1] - index[0]) * 65536];
	f.read(_, (index[1] - index[0]) * 65536);
	unsigned int *__ = (unsigned int*)_;
	datalast = __[2];
	for (unsigned int i = 1; (i < (((index[1] - index[0]) * 65536) / (sizeof(unsigned int) * 3))&&(__[i*3]!=TRU_VOID)); ++i) {
		_Tree[__[i * 3]] = { __[i * 3] ,__[i * 3 + 1] ,__[i * 3 + 2] ,nullptr };
	}
	//free the ptr
	delete[]_;
}

Tree::~Tree() {
	f.close();
}

char * Tree::ReadPart(int s_pos, int length)
{
	int s_c = s_pos / 65536, s_bt = s_pos % 65536, e_c = (s_pos + length) / 65536, e_bt = (s_pos + length) % 65536;
	char* res = new char[length];
	if (res==nullptr) return nullptr;	//Called on memory allocation error
	if (s_c == e_c) {
		f.seekp(index[s_c] + s_bt);
		f.read(res, length);
		return res;
	};
	if (s_c + 1 == e_c) {
		char*p = res;
		f.seekp(index[s_c] * 65536 + (65536 - s_bt));
		f.read(res, length); 
		res += 65536 - e_bt;
		f.seekp(index[e_c] * 65536);
		f.read(res, e_bt);
		return p;
	}
	if (s_c + 1 < e_c) {
		char*p = res;
		f.seekp(index[s_c] * 65536 + (65536 - s_bt));
		f.read(res, length);
		res += 65536 - e_bt;
		++s_c;
		for (; s_c < e_c; ++s_c) {
			f.seekp(index[s_c] * 65536);
			f.read(res, 65536);
			res += 65536;
		}
		f.seekp(index[e_c] * 65536);
		f.read(res, e_bt);
		return p;
	}
	//if it didn't exit before here,a bug must have been exsisted in the code
	return nullptr;//function failed
}

void Tree::ExpandPath() {
	int _ = index[1];
	for (int __ = 65536; __ > _; --__) {
		index[__] = index[__ - 1];
	}
	index[_] = 0;
	++index[1];
	//flush the changes
	f.seekp(0);
	f.write((char*)&index, sizeof(index));
	f.flush();
}


void* Tree::LoadINT(unsigned int* pos)
{
	return nullptr;
}

void* Tree::LoadFLOAT(unsigned int* pos)
{
	return nullptr;
}

void* Tree::LoadBOOL(unsigned int* pos)
{
	return nullptr;
}

void* Tree::LoadSTRING(unsigned int* pos)
{
	return nullptr;
}

void* Tree::LoadNNN(unsigned int* pos)
{
	return nullptr;
}