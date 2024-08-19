#include <algorithm>
#include "our_gl.h"

Matrix ModelView;
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}

const int width = 800;
const int height = 800;
const int depth = 255;

void line(Vec2i xy0, Vec2i xy1, TGAImage& image, TGAColor color) {
	//for (float t = 0.0; t < 1.0; t += 0.01) {
	//	int x = x0 + (x1 - x0) * t;
	//	int y = y0 + (y1 - y0) * t;
	//	image.set(x, y, color);
	//}
	bool steep = false;
	// Check if line width is greater than line height for steepness, draw in y direction
	if (std::abs(xy0.x - xy1.x) < std::abs(xy0.y - xy1.y)) {
		std::swap(xy0.x, xy0.y);
		std::swap(xy1.x, xy1.y);
		steep = true;
	}

	// If x1 is bigger than x0, then draw line opposite(so that the for loop works)
	if (xy0.x > xy1.x) {
		std::swap(xy0.x, xy1.x);
		std::swap(xy0.y, xy1.y);
	}

	int dx = xy1.x - xy0.x;
	int dy = xy1.y - xy0.y;
	float derror = std::abs(dy / (float)dx);
	float error = 0;
	int y = xy0.y;
	for (int x = xy0.x; x <= xy1.x; x++) {
		if (steep)
			image.set(y, x, color); // Swap back for steepness
		else
			image.set(x, y, color);

		error += derror;
		if (error > 0.5) {
			y += (xy1.y > xy0.y) ? 1 : -1;
			error -= 1.0;
		}
	}
}

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
	Vec3f s[2];
	for (int i = 2; i--; ) {
		s[i][0] = C[i] - A[i];
		s[i][1] = B[i] - A[i];
		s[i][2] = A[i] - P[i];
	}
	Vec3f u = cross(s[0], s[1]);
	if (std::abs(u[2]) > 1e-2) // Check if triangle area is greater than 0
		return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
	return Vec3f(-1, 1, 1);
}

// Bounding box with barycentric coordinates and z-buffer triangle rasterizer
// Includes camera perspective projection
void triangle(Vec4f* pts, IShader &shader, TGAImage& image, TGAImage& zbuffer) {

	Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 2; j++) {
			bboxmin[j] = std::min(bboxmin[j], pts[i][j] / pts[i][3]);
			bboxmax[j] = std::max(bboxmax[j], pts[i][j] / pts[i][3]);
		}
	}
	Vec2i P;
	TGAColor color;

	for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) {
		for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) {
			// Calculate barycentric coordinates(weights) of each point i bc_screen
			Vec3f bc_screen = barycentric(proj<2>(pts[0] / pts[0][3]), proj<2>(pts[1] / pts[1][3]), proj<2>(pts[2] / pts[2][3]), P);
			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;

			// Calculate depth of pixel/fragment
			float z = 0;
			for (int i = 0; i < 3; i++) z += bc_screen[i] * pts[i][2];

			float w = 0;
			for (int i = 0; i < 3; i++) w += bc_screen[i] * pts[i][3];

			int frag_depth = std::max(0, std::min(255, int(z/w+0.5)));

			if (zbuffer.get(P.x, P.y)[0] > frag_depth) continue;

			bool discard = shader.fragment(bc_screen, color);
			if (!discard) {
				zbuffer.set(P.x, P.y, TGAColor(frag_depth));
				image.set(P.x, P.y, color);
			}
		}
	}
}

// Get camera view translation matrix
void lookat(Vec3f eye, Vec3f center, Vec3f up) {
	Vec3f z = (eye - center).normalize();
	Vec3f x = cross(up, z).normalize();
	Vec3f y = cross(z, x).normalize();
	Matrix Minv = Matrix::identity();
	Matrix Translate = Matrix::identity();

	for (int i = 0; i < 3; i++) {
		Minv[0][i] = x[i];
		Minv[1][i] = y[i];
		Minv[2][i] = z[i];
		Translate[i][3] = -center[i];
	}
	ModelView = Minv * Translate;
}

// Get viewport transformation matrix
void viewport(int x, int y, int w, int h) {
	Viewport = Matrix::identity();
	Viewport[0][3] = x + w / 2.f;
	Viewport[1][3] = y + h / 2.f;
	Viewport[2][3] = depth / 2.f;

	Viewport[0][0] = x / 2.f;
	Viewport[1][1] = y / 2.f;
	Viewport[2][2] = depth / 2.f;
}

// Get projection matrix
void projection(float coeff) {
	Projection = Matrix::identity();
	Projection[3][2] = coeff;
}