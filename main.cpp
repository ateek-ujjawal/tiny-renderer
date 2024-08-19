#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "our_gl.h"
#include "geometry.h"

Model* model = NULL;
TGAImage shadowbuffer;
const int width = 800;
const int height = 800;
const int depth = 255;

// Line sweeping triangle rasterizer
//void triangle(Vec3f *pts, float *zbuffer, TGAImage& image, TGAColor color) {
//	if (pts[0].y == pts[1].y && pts[0].y == pts[2].y) return;
//	if (pts[0].y > pts[1].y) std::swap(pts[0], pts[1]);
//	if (pts[0].y > pts[2].y) std::swap(pts[0], pts[2]);
//	if (pts[1].y > pts[2].y) std::swap(pts[1], pts[2]);
//	int total_height = pts[2].y - pts[0].y;
//	for (int i = 0; i < total_height; i++) {
//		bool second_half = i > pts[1].y - pts[0].y || pts[1].y == pts[0].y;
//		int segment_height = second_half ? pts[2].y - pts[1].y : pts[1].y - pts[0].y;
//		float alpha = (float)i / total_height;
//		float beta = (float)(i - (second_half ? pts[1].y - pts[0].y : 0)) / segment_height; // be careful: with above conditions no division by zero here
//		Vec3f A = pts[0] + (pts[2] - pts[0]) * alpha;
//		Vec3f B = second_half ? pts[1] + (pts[2] - pts[1]) * beta : pts[0] + (pts[1] - pts[0]) * beta;
//		if (A.x > B.x) std::swap(A, B);
//		Vec3f P;
//		for (int j = A.x; j <= B.x; j++) {
//			P.x = j;
//			P.y = pts[0].y + i;
//			Vec3f bc_screen = barycentric(pts[0], pts[1], pts[2], P);
//			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;
//			P.z = 0;
//			for (int i = 0; i < 3; i++)
//				P.z += pts[i][2] * bc_screen[i];
//			if (zbuffer[int(P.x + P.y * width)] < P.z) {
//				zbuffer[int(P.x + P.y * width)] = P.z;
//				image.set(P.x, P.y, color);
//			}
//			//image.set(j, t0.y + i, color); // attention, due to int casts t0.y+i != A.y
//		}
//	}
//}

Vec3f light_dir(5, 5, 1);
Vec3f eye(0, 0, 3);
Vec3f center(0, 0, 0);
Vec3f up(0, 1, 0);

// Gouraud shader implementation
struct GouraudShader : public IShader {
	Vec3f varying_intensity;
	mat<2, 3, float> varying_texture;

	virtual Vec4f vertex(int iface, int nthvert) {
		varying_texture.set_col(nthvert, model->texture(iface, nthvert));
		varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir);
		Vec4f gl_vertex = embed<4>(model->vert(iface, nthvert));
		return Viewport * Projection * ModelView * gl_vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor& color) {
		float intensity = varying_intensity * bar;
		Vec2f tex = varying_texture * bar;
		color = model->diffuse(tex) * intensity;
		return false;
	}
};

// Blinn phong reflection model based shader
struct BlinnPhongShader : public IShader {
	mat<2, 3, float> varying_texture;
	mat<4, 4, float> uniform_M; // Projection * ModelView
	mat<4, 4, float> uniform_MIT; // Inverse transpose of Projection * ModelView

	virtual Vec4f vertex(int iface, int nthvert) {
		varying_texture.set_col(nthvert, model->texture(iface, nthvert));
		Vec4f gl_vertex = embed<4>(model->vert(iface, nthvert));
		return Viewport * Projection * ModelView * gl_vertex;
	}

	// Fragment shader using blinn phong reflection model
	virtual bool fragment(Vec3f bar, TGAColor& color) {
		Vec2f tex = varying_texture * bar;
		Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(tex))).normalize(); // Get normal vector mapping for this texture pixel
		Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // Get light direction vector
		Vec3f v = proj<3>(uniform_M * embed<4>(eye)).normalize(); // Get view direction vector

		//Phong reflection
		//Vec3f r = (n * (n * l * 2.f) - l).normalize(); // Here n * l is dot product, so it gets multiplied with the float value
		//											   // Which is then multiplied with the normal to get a vector then subtracted
		//											   // from the light vector
		//float spec_l = pow(std::max(0.f, r.z), model->spec(tex)); // Get specular highlight and calculate cosine with power

		Vec3f h = (l + v).normalize(); // Calculate halfway vector
		float spec_l = pow(std::max(0.f, h * n), 4 * model->spec(tex)); // alpha multiplied by 4
		float diffuse_l = std::max(0.f, n * l);
		TGAColor c = model->diffuse(tex);
		color = c;
		for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diffuse_l + 0.6 * spec_l), 255);
		return false;
	}
};

struct DepthShader : public IShader {
	mat<3, 3, float> varying_tri;

	DepthShader() : varying_tri() {}

	virtual Vec4f vertex(int iface, int nthvert) {
		Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
		gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;          // transform it to screen coordinates
		varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
		return gl_Vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor& color) {
		Vec3f p = varying_tri * bar;
		color = TGAColor(255, 255, 255) * (p.z / depth);
		return false;
	}
};

// Blinn phong shader with shadows
struct Shader : public IShader {
	mat<2, 3, float> varying_texture;
	mat<4, 4, float> uniform_M; // Projection * ModelView
	mat<4, 4, float> uniform_MIT; // Inverse transpose of Projection * ModelView
	mat<4, 4, float> uniform_Mshadow; // Shadow buffer frame
	mat<3, 3, float> varying_vert;

	Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_texture(), varying_vert() {}

	virtual Vec4f vertex(int iface, int nthvert) {
		varying_texture.set_col(nthvert, model->texture(iface, nthvert));
		Vec4f gl_vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
		varying_vert.set_col(nthvert, proj<3>(gl_vertex / gl_vertex[3]));
		return gl_vertex;
	}

	// Fragment shader using blinn phong reflection model
	virtual bool fragment(Vec3f bar, TGAColor& color) {
		// Convert framebuffer(screen) coordinates to shadowbuffer frame coordinates and calculate if it is lit or not due to the shadow
		// Check in shadowbuffer if that pixel is lit and calculate shadow value
		Vec4f sb_p = uniform_Mshadow * embed<4>(varying_vert * bar);
		sb_p = sb_p / sb_p[3];
		float shadow = 0.3 + 0.7 * (shadowbuffer.get(sb_p[0], sb_p[1])[0] < sb_p[2]+43.34); // error correction to fix z-fighting

		Vec2f tex = varying_texture * bar;
		Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(tex))).normalize(); // Get normal vector mapping for this texture pixel
		Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // Get light direction vector
		Vec3f v = proj<3>(uniform_M * embed<4>(eye)).normalize(); // Get view direction vector

		//Phong reflection
		//Vec3f r = (n * (n * l * 2.f) - l).normalize(); // Here n * l is dot product, so it gets multiplied with the float value
		//											   // Which is then multiplied with the normal to get a vector then subtracted
		//											   // from the light vector
		//float spec_l = pow(std::max(0.f, r.z), model->spec(tex)); // Get specular highlight and calculate cosine with power

		Vec3f h = (l + v).normalize(); // Calculate halfway vector
		float spec_l = pow(std::max(0.f, h * n), 4 * model->spec(tex));
		float diffuse_l = std::max(0.f, n * l);
		TGAColor c = model->diffuse(tex);
		for (int i = 0; i < 3; i++) color[i] = std::min<float>(1 + c[i] * shadow * (diffuse_l + 0.6 * spec_l), 255);
		return false;
	}
};

int main(int argc, char** argv) {
	if (argc == 5) {
		model = new Model(argv[1], argv[2], argv[3], argv[4]);
	}
	else {
		model = new Model("obj/african_head.obj", "obj/african_head_diffuse.tga", "obj/african_head_nm.tga", "obj/african_head_spec.tga");
		//model = new Model("obj/diablo3_pose.obj", "obj/diablo3_pose_diffuse.tga", "obj/diablo3_pose_nm.tga", "obj/diablo3_pose_spec.tga");
	}

	light_dir.normalize();

	TGAImage image(width, height, TGAImage::RGB);
	TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);
	shadowbuffer = TGAImage(width, height, TGAImage::GRAYSCALE);

	//GouraudShader shader;
	//for (int i = 0; i < model->nfaces(); i++) {
	//	Vec4f screen_coords[3];
	//	for (int j = 0; j < 3; j++) {
	//		screen_coords[j] = shader.vertex(i, j);
	//	}
	//	triangle(screen_coords, shader, image, zbuffer);
	//}

	//BlinnPhongShader shader;
	//shader.uniform_M = Viewport * Projection * ModelView;
	//shader.uniform_MIT = (Projection * ModelView).invert_transpose();
	//for (int i = 0; i < model->nfaces(); i++) {
	//	Vec4f screen_coords[3];
	//	for (int j = 0; j < 3; j++) {
	//		screen_coords[j] = shader.vertex(i, j);
	//	}
	//	triangle(screen_coords, shader, image, zbuffer);
	//}

	// Compute final render in 2 passes
	// One for computing shadow buffer values
	// Another for getting screen space values from shadow buffer and rendering them respectively
	{ // rendering the shadow buffer
		TGAImage depth(width, height, TGAImage::RGB);
		lookat(light_dir, center, up);
		projection(0);
		viewport(width / 2, height / 2, width / 20, height / 20);

		DepthShader depthshader;
		Vec4f screen_coords[3];
		for (int i = 0; i < model->nfaces(); i++) {
			for (int j = 0; j < 3; j++) {
				screen_coords[j] = depthshader.vertex(i, j);
			}
			triangle(screen_coords, depthshader, depth, shadowbuffer);
		}
		depth.flip_vertically(); // to place the origin in the bottom left corner of the image
		depth.write_tga_file("depth.tga");
	}

	Matrix M = Viewport * Projection * ModelView;

	{	// rendering the frame buffer
		lookat(eye, center, up);
		projection(-1.f / (eye - center).norm());
		viewport(width / 2, height / 2, width / 20, height / 20);

		Shader shader(ModelView, (Projection * ModelView).invert_transpose(), M * (Viewport * Projection * ModelView).invert());
		Vec4f screen_coords[3];
		for (int i = 0; i < model->nfaces(); i++) {
			for (int j = 0; j < 3; j++) {
				screen_coords[j] = shader.vertex(i, j);
			}
			triangle(screen_coords, shader, image, zbuffer);
		}
	}

	image.flip_vertically();
	zbuffer.flip_vertically();
	image.write_tga_file("output.tga");
	zbuffer.write_tga_file("zbuffer.tga");

	delete model;
	return 0;
}