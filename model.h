#ifndef __MODEL_H__
#define __MODEL_H__

#include <vector>
#include "geometry.h"
#include "tgaimage.h"

class Model {
private:
	TGAImage texture_img; // Texture maps
	TGAImage normal_img; // Normal maps
	TGAImage spec_img; // Specular highlight map
	std::vector<Vec3f> verts_;
	std::vector<Vec2f> textures_;
	std::vector<Vec3f> normals_;
	std::vector<std::vector<int> > faces_verts_;
	std::vector<std::vector<int>> faces_textures_;
	std::vector<std::vector<int>> faces_normals_;
public:
	Model(const char* filename, const char* diffuse_f, const char* nm_f, const char* spec_f);
	~Model();
	int nverts();
	int nfaces();
	int ntextures();
	int nnormals();
	Vec3f vert(int i, int j);
	Vec2f texture(int i, int j);
	Vec3f normal(int i, int j);
	Vec3f normal(Vec2f P);
	int spec(Vec2f P);
	TGAColor diffuse(Vec2f P);
};

#endif //__MODEL_H__