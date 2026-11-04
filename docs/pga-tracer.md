# The PGA Raytracer

FlyTracer's raytracer (`engine/shaders/raytracer.comp`) is written in PGA throughout: the camera is a motor, rays are points with directions, triangles are tested with the Plücker products of their edges, reflections are sandwiches with planes. It replaced a raytracer that used PGA only for its instance transforms and vector math for everything else, and it renders the same images. This page explains its techniques, with the formulas in FlyFish's notation: `a & b` is the join, `a ^ b` the meet, `a | b` the inner product.

The shader functions use FlyFish's names and signs (`engine/shaders/flyfish.glsl`), so a formula reads the same in C++ and GLSL:

| C++ (FlyFish) | GLSL (`flyfish.glsl`) |
|---|---|
| `a & b` | `join(a, b)` |
| `a ^ b` | `meet(a, b)` |
| `a \| b` | `inner(a, b)` |
| `a * b` | `geometricProduct(a, b)` |
| `M * X * ~M` | `sandwich(M, X)` |
| `P * X * ~P` (unit plane) | `reflectInPlane(P, X)` |

`FlyTracer_ShaderTest` runs every one of them on the GPU and compares it with FlyFish.

## Camera

The camera is the motor that moves the camera's own frame, at the origin looking down `-z` with `+y` up, into the world (`CameraMotor` in `Camera.h`). It turns `-z` towards the viewing direction, rolls around that direction until `+y` points up, and then translates to the eye:

```cpp
const Motor camera = TranslationMotor(eye) * roll * turn;
```

The ray through a pixel starts at `M o ~M` (the eye) and runs along `M d ~M`, with `d` the pixel's direction in the camera frame. `ViewRay`, which scenes use for mouse picking, applies the same motor, so a picked line is exactly the traced ray.

## Rays and Planes

A ray is a point `o` and a direction `d` (an ideal point). Its line is `o & d`.

**Where a ray meets a plane.** The join of a point with a plane is linear along the ray: `(o + t d) & P = (o & P) + t (d & P)`. It is zero on the plane, so

```cpp
t = -(o & P) / (d & P)
```

`rayParameter` in `common.glsl` computes it. It intersects planes, finds where a ray meets a triangle's plane, and finds the point of a line closest to another point: there the line meets the plane through that point perpendicular to it, `p | L`.

**Origin split.** The coordinates of a point in PGA work relative to any origin ("Clean up your Mesh!", section 3c). The tracer moves the origin to an anchor near the geometry before computing joins (`relativeTo`): the numbers stay small, so the results stay precise.

## Spheres

With the sphere's center as the origin, the line `L = o & d` passes the center at the distance `|moment| / |direction|`. When that is less than the radius, the ray enters and leaves half a chord before and after its closest point, where it meets the plane `c | L` through the center perpendicular to it.

## Triangles

The Plücker product of two lines, `L1 & L2` (a scalar), is zero when they intersect, and its sign tells on which side of each other they pass. A ray passes through a triangle when its products with the three edges, all run through in winding order, have the same sign. Each product is proportional to the barycentric weight of the corner opposite the edge, so the weights come free:

```cpp
w_i = (L & E_i) / ((L & E_0) + (L & E_1) + (L & E_2))
```

where `E_i` runs between the two other corners. The triangle's plane, needed for the distance `t`, is the join of one edge with the direction of another: `E_a & direction(E_b)`.

`BuildEdges` (`MeshLines.h`) stores every edge once: vertices at the same position are welded, so neighboring triangles share their edge.

- **Watertight.** Both triangles of an edge evaluate the same stored line, so they get the same product with opposite sign. A ray through the edge cannot slip between them. Rays aimed exactly at the shared edges and the shared vertex of a fan of triangles never missed. Möller-Trumbore in floats missed 16% of the edge rays and 23% of the vertex rays (`MeshLinesTest.EdgeTestIsWatertight`). In the shader the product is computed with `precise`, so the compiler cannot round it differently where it inlines it. This only applies away from silhouettes: where two triangles fold over each other as seen from the ray, a ray just beside the edge rightly hits neither.
- **Anchored.** Plücker coordinates relative to the world origin lose precision far from it: at 1000 units the error is 400 times that of Möller-Trumbore. Each edge is therefore stored as an anchor point and a direction, and its product with the ray is computed with the anchor as the origin. There the edge's line has no moment, so the product is the ray's moment around the anchor along the edge. That is as precise at 1000 units as at the origin (`EdgeTestIsPreciseFarFromTheOrigin`).

## Bounding Volumes

Both BVHs (the top level over instances, the bottom level per mesh) store **node pairs**: a node holds the bounding boxes of both its children, so one load serves the test of both (`PairChildren` in `BVH.h`). The ray meets the axis plane `x = c`, the plane `e1 - c e0`, at `t = (c - o.x) / d.x`; it is inside a box where it is between all three pairs of planes (the slab test).

## Instances

A ray enters an instance's object space through the inverse of the instance's versor: first the mirror plane (its own inverse), then `~M`, then the inverse scale. The direction is not renormalized, so a distance `t` along the ray is the same in both spaces. A reflection reverses a line's orientation, `P L P = -((P o P) & (P d P))`, but the edge test only compares signs, so mirrored instances need no special case.

## Shading

- **Normals** are directions (ideal points). The cosine between two unit directions is the inner product of the planes through the origin perpendicular to them.
- **Mirror reflections** reflect the incoming direction in the tangent plane: `reflectInPlane(Vector(normal, 0), -V)`. Glossy reflections reflect it in the plane of a sampled GGX microfacet.
- **The GGX tangent frame is a rotor**, after "Look, Ma, No Matrices!": two reflections turn `+z` into the normal, in the plane perpendicular to `+z` and then in the plane perpendicular to the direction halfway between `+z` and the normal. The sampled direction is moved into the world with `sandwich(frame, h)` instead of a basis of three vectors.
- **Shadow terminator.** Shadow rays of smooth-shaded triangles start on the curved surface their normals describe (Hanika, Ray Tracing Gems II ch. 4). In PGA that takes only the **tangent planes** at the corners, `T_i = -(p_i | (p_i & n_i))`. Their join with a point is its height above them, and the start point is lifted by `sum_i w_i min(h & T_i, 0) n_i`. The corner positions cancel out because the weights sum to 1.
- **Texture level of detail.** Texture coordinates are linear over a triangle: `u = h & U` and `v = h & V`, with planes `U` and `V` made from the barycentric planes. Where those two planes meet, their line's length is the texture area per surface area, `‖U ^ V‖`, which sets the mip level together with the ray cone's width.

## Debug Lines

All camera rays start at the eye, so the overlay takes the eye as the origin: the ray's line then has no moment and its joins are single cross or dot products. The plane through the eye and a debug segment, `eye & segment`, contains every point of the segment. A ray whose angle with that plane is larger than the line's width cannot come near the segment, which rules out almost every line for almost every pixel with one dot product. For the remaining lines, the segment point closest to the ray lies where the segment's line meets the plane through the ray along their common perpendicular. That perpendicular is the normal of the plane through the ray parallel to the segment. The point's distance to the ray is the norm of their join, a plane.

## Mesh Measures

`Mesh` computes the measures of the solid its triangles enclose, with the joins of "Clean up your Mesh!". They are useful for physics in a game, for example the mass and the rotation axes of a mesh:

```cpp
if (const Mesh* mesh = GetMesh(meshId); mesh && mesh->IsClosed()) {
    const float volume = mesh->Volume();
    const TriVector center = mesh->CenterOfMass();
    const PrincipalInertia inertia = mesh->Inertia();  // About the center, for unit density
    const TriVector axis = ApplyMotor(inertia.rotor, TriVector(1.0f, 0.0f, 0.0f, 0.0f));  // Axis with moments[0]
}
```

- **Volume.** A triangle `F = p0 & p1 & p2` and any point `o` form a tetrahedron with six times the volume `o & F`. Its sign comes from the winding, so the tetrahedra of all triangles add up to the solid: `V = 1/6 sum_F (o & F)`.
- **Center of mass.** The tetrahedra's centroids `(p0 + p1 + p2 + o) / 4`, weighted by their volumes. As homogeneous points their sum is the center of mass with weight `4 V`, and `Euclidean` divides that out.
- **Inertia.** The inertia tensor is a frame of three planes, the rows of its matrix, accumulated per tetrahedron. It is diagonalized with Jacobi rotations that are rotors: every step rotates around one axis by the angle that zeroes one off-diagonal element. Their product gives the principal axes as a rotor, without a rotation matrix.
- **Closed.** The measures are only meaningful for a closed, consistently wound mesh: `IsClosed` checks that every welded edge is run through as often in one direction as in the other. Summing the triangles' planes (the paper's "gap" measure) is not enough: an open tube passes that test, because its two missing caps cancel.

## Design Choices

The raytracer was developed next to the vector math raytracer it replaced, with variants that could be switched live and benchmarked. On an M4 Pro at 1920 x 1080 with 3 bounces, without the debug line overlay, relative to the vector math raytracer:

| Scene | Edges, spheres | Edges, boxes (kept) | Planes, spheres | Planes, boxes |
|---|---|---|---|---|
| bireflection | 1.51x | 1.30x | 1.42x | 1.20x |
| birot | 2.12x | 1.26x | 1.77x | 1.14x |
| bitrans | 2.17x | 1.16x | 1.92x | 1.04x |
| quaternion | 1.43x | 1.28x | 1.35x | 1.17x |
| hornet | 1.42x | 1.19x | 1.31x | 1.12x |
| testbox | 1.55x | 1.12x | 1.38x | 1.05x |
| plucker (no meshes) | 1.00x | 1.00x | 1.00x | 0.99x |
| debug (no meshes) | 1.05x | 1.03x | 1.04x | 1.03x |

- **PGA costs nothing outside the meshes.** The scenes without meshes are as fast as with vector math: camera motor, sphere and plane joins, plane reflections and rotor frames.
- **Boxes, not spheres.** A sphere stays a sphere under a motor, but for a random ray the chance to hit a convex volume is proportional to its surface area. The spheres around the pheasant's and the teapot's BVH nodes have 1.6 to 1.9 times the area of the boxes, around a flat box 8 to 14 times, and the sphere variants were the slowest.
- **Edges, not planes.** A triangle can also be tested with three planes (Havel and Herout, "Yet Faster Ray-Triangle Intersection", 2010): its own plane, and two barycentric planes `B_i = (E_i & n) / (p_i & (E_i & n))` whose joins with the hit point are the weights of two corners. That was about 10% faster, but it is not watertight. The anchored edge test is, and it stays precise far from the origin.

The debug line overlay is about 2.5 times faster than the vector math version (0.43 ms against 1.07 ms in the plucker scene), so the scenes with many debug lines (plucker, debug) render in 60 to 65% of the time they took before.

## Tests

- `FlyTracer_ShaderTest` runs every function of `flyfish.glsl` on the GPU and compares it with FlyFish.
- `FlyTracer_SceneTest` renders every game scene and compares it with a golden image. The references in `tests/reference/scenes` were rendered by the vector math raytracer, so the test also checks that the PGA formulation renders the same images: they differ by at most 0.25/255 on average, from sampling noise in glossy reflections (the rotor tangent frame draws different random samples).
- `MeshLinesTest` checks the edge test against Möller-Trumbore in double precision, its precision far from the origin, and its watertightness.

!!! note "GPU timestamps"
    On macOS with KosmicKrisp all timestamps in a command buffer have the same value, so `FlyTracer --benchmark` reports the time from submitting a frame until it completed. That includes about 0.02 ms of overhead, the time of an empty submission, which the benchmark prints. Drivers with working timestamps also get GPU times.

## References

- Steven De Keninck, *Look, Ma, No Matrices!* (SIGGRAPH 2024): <https://github.com/enkimute/LookMaNoMatrices>. Motors instead of matrices in a renderer, and tangent frames as rotors.
- Steven De Keninck, Martin Roelfs, Leo Dorst and David Eelbode, *Clean up your Mesh! Part 1: Plane and simplex* (2025): <https://arxiv.org/abs/2511.08058>. Simplices as joins, magnitudes from norms, volume, center of mass and inertia.
- Leo Dorst and Steven De Keninck, *A Guided Tour to the Plane-Based Geometric Algebra PGA*: <https://bivector.net/PGA4CS.html>
- Jiří Havel and Adam Herout, *Yet Faster Ray-Triangle Intersection (Using SSE4)*, IEEE TVCG 16(3), 2010: the three-plane triangle test that was compared.
- Johannes Hanika, *Hacking the Shadow Terminator*, Ray Tracing Gems II, chapter 4.
- Tomas Akenine-Möller et al., *Texture Level of Detail Strategies for Real-Time Ray Tracing*, Ray Tracing Gems, chapter 20.
- Jonathan Dupuy and Anis Benyoub, *Sampling Visible GGX Normals with Spherical Caps*, 2023.
