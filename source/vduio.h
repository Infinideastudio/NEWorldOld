#include "stdinclude.h"
#include <fstream>
//This Part Provides A Acress to A Dyamic File Less Than 4GB

//Data types
enum {
	TRU_VOID,
	TRU_ROOT,
	TRU_TNODE,
	TRU_INT,
	TRU_FLOAT,
	TRU_BOOL,
	TRU_STRING,
	TRU_NNN,
	TRU_EOE
};

extern bool TRU_MODE_NO_CREATE;

struct TNode {
	unsigned int globalid;
	unsigned int type;
	unsigned int datapos;
	void* data;
};

class Tree{
	//Data
	std::fstream f;
	unsigned short index[65536];
	unsigned int datalast;
	std::map<int, TNode> _Tree;//Where the key table is stored
	std::map<string, TNode*> FastAcressTable;//Only Keep The Handle Of the Loaded Datas

	//Corn Functions,NEVER CALL THESE DIRECTLY!!!
	char* ReadPart(int s_pos, int length);
	void ExpandPath();
	void* LoadINT(unsigned int* pos);
	void* LoadFLOAT(unsigned int* pos);
	void* LoadBOOL(unsigned int* pos);
	void* LoadSTRING(unsigned int* pos);
	void* LoadNNN(unsigned int* pos);
public:
	//Load and Unload
	Tree(std::string fname);
	~Tree();
};