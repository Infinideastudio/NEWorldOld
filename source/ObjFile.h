#ifndef OBJ_H_
#define OBJ_H_

#include "Definitions.h"
#include "Mat4.h"
#include "Renderer.h"

class Obj {
public:
    void loadFromFile(const std::string& filename, float scale);
    std::pair<VBOID, int> buildRender();
    
private:
	std::vector<Vec3f> mVertexes, mTexcoords, mNormals;
};

#endif

