tiny-renderer
=

A small 3D rendering engine based on the tinyrenderer tutorial. Built parts of the OpenGL pipeline in C++.

Features included
-
- Loading models from obj files and corresponding texture, normal and specular maps.
- Bresenham's line drawing algorithm.
- Rasterization of triangles using barycentric coordinates.
- Backface culling through normal vectors.
- Hidden surface removal using z-buffer(depth buffer).
- Perspective and orthogonal projections.
- Programmable vertex and fragment shaders.
- Math library for operations on matrices, vectors and points
- Texture, normals and specular intensities mapped through Blinn Phong shaders.
- Shadow mapping the scene by rendering it in 2 passes.
