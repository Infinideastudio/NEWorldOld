#include "stdinclude.h"
#include <fstream>
//This Part Provides A Acress to A Dyamic File Less Than 4GB
class Tree{
	unsigned short index[65536];
	std::fstream f;
	Tree(std::string	);

};