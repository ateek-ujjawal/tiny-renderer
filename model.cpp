#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"
#include "tgaimage.h"

Model::Model(const char* filename, const char* diffuse_f, const char* nm_f, const char* spec_f) : verts_(), faces_verts_(), textures_(), faces_textures_(), normals_(), faces_normals_() {
    texture_img.read_tga_file(diffuse_f);
    texture_img.flip_vertically();
    normal_img.read_tga_file(nm_f);
    normal_img.flip_vertically();
    spec_img.read_tga_file(spec_f);
    spec_img.flip_vertically();

    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            Vec3f v;
            for (int i = 0; i < 3; i++) iss >> v[i];
            verts_.push_back(v);
        }
        else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            Vec2f t;
            for (int i = 0; i < 2; i++) iss >> t[i];
            t[0] *= texture_img.get_width();
            t[1] *= texture_img.get_height();
            textures_.push_back(t);
        }
        else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            Vec3f n;
            for (int i = 0; i < 3; i++) iss >> n[i];
            normals_.push_back(n);
        }
        else if (!line.compare(0, 2, "f ")) {
            std::vector<int> f, t, n;
            int itrash, idx, t_idx, n_idx;
            iss >> trash;
            while (iss >> idx >> trash >> t_idx >> trash >> n_idx) {
                idx--; // in wavefront obj all indices start at 1, not zero
                t_idx--;
                n_idx--;
                f.push_back(idx);
                t.push_back(t_idx);
                n.push_back(n_idx);
            }
            faces_verts_.push_back(f);
            faces_textures_.push_back(t);
            faces_normals_.push_back(n);
        }
    }
    std::cerr << "# v# " << verts_.size() << " f# " << faces_verts_.size() << " t# " << textures_.size() << std::endl;
}

Model::~Model() {
}

int Model::nverts() {
    return (int)verts_.size();
}

int Model::nfaces() {
    return (int)faces_verts_.size();
}

int Model::ntextures() {
    return (int)textures_.size();
}

int Model::nnormals() {
    return (int)normals_.size();
}

Vec3f Model::vert(int i, int j) {
    return verts_[faces_verts_[i][j]];
}

Vec2f Model::texture(int i, int j) {
    return textures_[faces_textures_[i][j]];
}

Vec3f Model::normal(int i, int j) {
    return normals_[faces_normals_[i][j]];
}

Vec3f Model::normal(Vec2f P) {
    return Vec3f(normal_img.get((int)P.x, (int)P.y)[2], normal_img.get((int)P.x, (int)P.y)[1], normal_img.get((int)P.x, (int)P.y)[0]);
}

TGAColor Model::diffuse(Vec2f P) {
    return texture_img.get(P.x, P.y);
}

int Model::spec(Vec2f P) {
    return spec_img.get(P.x, P.y)[0];
}

