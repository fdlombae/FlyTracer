// Closest-hit and any-hit queries over the scene. Meshes use a two-level hierarchy: a top-level BVH
// over the mesh instances in world space, and per mesh a bottom-level BVH over its triangles in
// object space. Every node holds the bounding boxes of both its children.

#ifndef TRAVERSAL_GLSL
#define TRAVERSAL_GLSL

Motor instanceMotor(MeshInstance instance) {
    return Motor(instance.rotor.x, instance.rotor.yzw, instance.dual.xyz, instance.dual.w);
}

// World space to an instance's object space: undo the mirror, then ~M, then the scale.
// The direction is not renormalized, so a distance t along the ray is the same in both spaces.
Ray toObjectSpace(Ray ray, MeshInstance instance) {
    if (instance.hasMirror != 0u) {
        Vector mirror = Vector(instance.mirror.xyz, instance.mirror.w);
        ray = Ray(reflectInPlane(mirror, ray.origin), reflectInPlane(mirror, ray.direction));
    }
    Motor toObject = reverse(instanceMotor(instance));
    TriVector origin = sandwich(toObject, ray.origin);
    TriVector towards = sandwich(toObject, ray.direction);
    return Ray(point(origin.xyz / instance.scale), direction(towards.xyz / instance.scale));
}

// The children of a node pair, with the distances where the ray enters them (INFINITY for a miss)
struct Children {
    vec2 enter;
    ivec2 index;
    ivec2 count;
};

Children childrenOf(NodePair pair, Ray ray, vec3 inverseDirection, float tMax) {
    Children children = Children(vec2(INFINITY), pair.index, pair.count);
    for (int i = 0; i < 2; ++i) {
        if (pair.count[i] >= 0) children.enter[i] = enterBox(ray, inverseDirection, pair.boxes[i], tMax);
    }
    return children;
}


// The triangles of a leaf, in its records
void intersectLeaf(Ray ray, uint instanceIndex, int first, int count, bool anyHit, inout Hit hit) {
    for (int record = first; record < first + count; ++record) {
        EdgeTriangle triangle = edgeTriangles[record];
        float t;
        vec2 barycentrics;
        if (intersectEdgeTriangle(ray, triangle, hit.t, t, barycentrics)) {
            hit = Hit(t, HIT_TRIANGLE, triangle.triangle, instanceIndex, barycentrics);
            if (anyHit) return;
        }
    }
}

// Both traversals keep a stack of pairs to visit with the distance where the ray enters them.
// Of a pair's children, leaves are handled nearest first, and internal nodes pushed farthest first
// so the nearest is visited next; nodes entered beyond the closest hit so far are skipped.

// Bottom level: the triangles of one mesh instance
void traverseMesh(Ray worldRay, uint instanceIndex, bool anyHit, inout Hit hit) {
    MeshInstance instance = instances[instanceIndex];
    Ray ray = toObjectSpace(worldRay, instance);
    vec3 inverseDirection = inverseOf(ray.direction);

    int stack[BVH_STACK_SIZE];
    float stackDistance[BVH_STACK_SIZE];
    stack[0] = int(meshRootPairs[instance.meshId]);
    stackDistance[0] = 0.0;
    int stackSize = 1;

    while (stackSize > 0) {
        --stackSize;
        if (stackDistance[stackSize] >= hit.t) continue;
        Children children = childrenOf(meshPairs[stack[stackSize]], ray, inverseDirection, hit.t);
        int nearest = children.enter.y < children.enter.x ? 1 : 0;

        for (int i = nearest, k = 0; k < 2; ++k, i = 1 - i) {
            if (children.count[i] > 0 && children.enter[i] < hit.t) {
                intersectLeaf(ray, instanceIndex, children.index[i], children.count[i], anyHit, hit);
                if (anyHit && hit.type != HIT_NONE) return;
            }
        }
        for (int i = 1 - nearest, k = 0; k < 2; ++k, i = 1 - i) {
            if (children.count[i] == 0 && children.enter[i] < hit.t) {
                stack[stackSize] = children.index[i];
                stackDistance[stackSize++] = children.enter[i];
            }
        }
    }
}

// Top level: the mesh instances. A leaf holds one, or several when they cannot be split apart
// (e.g. with the same center).
void traverseInstances(Ray ray, bool anyHit, inout Hit hit) {
    vec3 inverseDirection = inverseOf(ray.direction);

    int stack[BVH_STACK_SIZE];
    float stackDistance[BVH_STACK_SIZE];
    stack[0] = 0;
    stackDistance[0] = 0.0;
    int stackSize = 1;

    while (stackSize > 0) {
        --stackSize;
        if (stackDistance[stackSize] >= hit.t) continue;
        Children children = childrenOf(scenePairs[stack[stackSize]], ray, inverseDirection, hit.t);
        int nearest = children.enter.y < children.enter.x ? 1 : 0;

        for (int i = nearest, k = 0; k < 2; ++k, i = 1 - i) {
            if (children.count[i] > 0 && children.enter[i] < hit.t) {
                for (int instance = children.index[i]; instance < children.index[i] + children.count[i]; ++instance) {
                    traverseMesh(ray, uint(instance), anyHit, hit);
                    if (anyHit && hit.type != HIT_NONE) return;
                }
            }
        }
        for (int i = 1 - nearest, k = 0; k < 2; ++k, i = 1 - i) {
            if (children.count[i] == 0 && children.enter[i] < hit.t) {
                stack[stackSize] = children.index[i];
                stackDistance[stackSize++] = children.enter[i];
            }
        }
    }
}

// The closest hit before tMax, or with anyHit (shadow rays) the first hit found
Hit traceScene(Ray ray, float tMax, bool anyHit) {
    Hit hit = Hit(tMax, HIT_NONE, 0u, 0u, vec2(0.0));
    float t;

    for (uint i = 0u; i < pc.sphereCount; ++i) {
        if (intersectSphere(ray, spheres[i].center, spheres[i].radius, hit.t, t)) {
            hit = Hit(t, HIT_SPHERE, i, 0u, vec2(0.0));
            if (anyHit) return hit;
        }
    }

    for (uint i = 0u; i < pc.planeCount; ++i) {
        if (intersectPlane(ray, Vector(planes[i].normal, planes[i].e0), hit.t, t)) {
            hit = Hit(t, HIT_PLANE, i, 0u, vec2(0.0));
            if (anyHit) return hit;
        }
    }

    if (pc.instanceCount > 0u) {
        traverseInstances(ray, anyHit, hit);
    }
    return hit;
}

#endif // TRAVERSAL_GLSL
