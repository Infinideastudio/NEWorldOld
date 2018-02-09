#include "ObjFile.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <string.h>

void Obj::loadFromFile(const std::string& filename, float scale) {
	mVertexes.clear();
	mTexcoords.clear();
	mNormals.clear();
	std::vector<Vec3f> vertexes, texcoords, normals;
    std::ifstream file(filename);
    if (!file.is_open()) return;
    while (!file.eof()) {
        std::string s, id;
        std::getline(file, s);
        std::stringstream ss;
        ss << s;
        ss >> id;
        std::transform(id.begin(), id.end(), id.begin(), tolower);
        if (id == "v") {
            double x, y, z;
            ss >> x >> y >> z;
            vertexes.push_back(Vec3f(x, y, z));
        } else if (id == "vt") {
            double u, v;
            ss >> u >> v;
            texcoords.push_back(Vec3f(u, 1.0f - v, 0.0f));
        } else if (id == "vn") {
            double x, y, z;
            ss >> x >> y >> z;
            normals.push_back(Vec3f(x, y, z));
        } else if (id == "f") {
            bool hasExtraInfo = s.find('/') != std::string::npos;
            std::replace(s.begin(), s.end(), '/', ' ');
            ss.str(s.substr(2));
            int vind1, tind1, nind1, vind2, tind2, nind2, vind3, tind3, nind3;
            ss >> vind1 >> tind1 >> nind1 >> vind2 >> tind2 >> nind2 >> vind3 >> tind3 >> nind3;
            vind1--; tind1--; nind1--; vind2--; tind2--; nind2--; vind3--; tind3--; nind3--;

			mVertexes.push_back(vertexes[vind1] * scale);
			mVertexes.push_back(vertexes[vind2] * scale);
			mVertexes.push_back(vertexes[vind3] * scale);
			mVertexes.push_back(vertexes[vind3] * scale);

			mTexcoords.push_back(texcoords[tind1]);
			mTexcoords.push_back(texcoords[tind2]);
			mTexcoords.push_back(texcoords[tind3]);
			mTexcoords.push_back(texcoords[tind3]);

			mNormals.push_back(normals[nind1]);
			mNormals.push_back(normals[nind2]);
			mNormals.push_back(normals[nind3]);
			mNormals.push_back(normals[nind3]);
        }
    }
}

std::pair<VBOID, int> Obj::buildRender() {
	VBOID res = 0;
	int vtxCount = 0;
	Renderer::Init(2, 3, 3, 1);
	Renderer::Attrib1f(233.0f);
	for (size_t i = 0; i < mVertexes.size(); i++) {
		Renderer::TexCoord2f(mTexcoords[i].x, mTexcoords[i].y);
		Renderer::Color3f(1.0f, 1.0f, 1.0f);
		Renderer::Normal3f(mNormals[i].x, mNormals[i].y, mNormals[i].z);
		Renderer::Vertex3f(mVertexes[i].x, mVertexes[i].y, mVertexes[i].z);
	}
	Renderer::Flush(res, vtxCount);
	return std::make_pair(res, vtxCount);
}
