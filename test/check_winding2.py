#!/usr/bin/env python3
"""Check triangle winding direction (CW vs CCW) in a binary STL file."""
import struct, sys, math

def read_stl_binary(path):
    with open(path, 'rb') as f:
        header = f.read(80)
        count = struct.unpack('<I', f.read(4))[0]
        tris = []
        for _ in range(count):
            normal = struct.unpack('<3f', f.read(12))
            v0 = struct.unpack('<3f', f.read(12))
            v1 = struct.unpack('<3f', f.read(12))
            v2 = struct.unpack('<3f', f.read(12))
            attr = struct.unpack('<H', f.read(2))[0]
            tris.append((v0, v1, v2, normal))
    return tris

def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0])

def dot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]

def vec_sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])

def vec_add(a, b):
    return (a[0]+b[0], a[1]+b[1], a[2]+b[2])

def vec_scale(a, s):
    return (a[0]*s, a[1]*s, a[2]*s)

def vec_len(a):
    return math.sqrt(a[0]**2 + a[1]**2 + a[2]**2)

def check_winding_direction(path):
    tris = read_stl_binary(path)
    print(f"Total triangles: {len(tris)}")
    
    # Build vertex deduplication
    vtx_map = {}
    vtx_list = []
    tri_verts = []
    
    for v0, v1, v2, _ in tris:
        idxs = []
        for v in (v0, v1, v2):
            qv = (round(v[0], 12), round(v[1], 12), round(v[2], 12))
            if qv not in vtx_map:
                vtx_map[qv] = len(vtx_list)
                vtx_list.append(qv)
            idxs.append(vtx_map[qv])
        tri_verts.append(tuple(idxs))
    
    # For each triangle, compute the signed area in the dominant plane
    # to determine winding direction
    # A CCW triangle (v0, v1, v2) has positive signed area when projected
    # onto the plane perpendicular to the triangle's normal
    
    ccw_count = 0
    cw_count = 0
    
    # Check a few sample triangles in detail
    sample_count = 0
    for i, (v0, v1, v2, stl_normal) in enumerate(tris):
        e1 = vec_sub(v1, v0)
        e2 = vec_sub(v2, v0)
        computed_normal = cross(e1, e2)
        nlen = vec_len(computed_normal)
        if nlen < 1e-12:
            continue
        
        # The sign of the z-component of computed_normal tells us the winding
        # when viewed from the +z direction. But we need the winding from
        # the OUTSIDE. Since we don't know which side is outside, we check
        # whether the computed normal agrees with the STL normal.
        computed_normal_n = vec_scale(computed_normal, 1.0/nlen)
        
        # If the computed normal (from winding) and the STL normal agree,
        # then the winding is such that the normal points in the direction
        # of (v1-v0)×(v2-v0). In a right-handed coordinate system, this
        # means the triangle is CCW when viewed from the normal's direction.
        
        # For OpenGL with glFrontFace(GL_CCW), triangles are front-facing
        # when CCW from the viewer's perspective. If the STL normal points
        # outward (which it should for a closed surface), then:
        #   dot(computed_normal_n, stl_normal) > 0 → CCW from outside → front-facing
        #   dot(computed_normal_n, stl_normal) < 0 → CW from outside → back-facing
        
        d = dot(computed_normal_n, stl_normal)
        if d > 0:
            ccw_count += 1
        else:
            cw_count += 1
            if sample_count < 10:
                print(f"  Tri {i}: CW winding (dot={d:.4f}) "
                      f"v0=({v0[0]:.3f},{v0[1]:.3f},{v0[2]:.3f}) "
                      f"v1=({v1[0]:.3f},{v1[1]:.3f},{v1[2]:.3f}) "
                      f"v2=({v2[0]:.3f},{v2[1]:.3f},{v2[2]:.3f})")
                sample_count += 1
    
    total = ccw_count + cw_count
    print(f"\nCCW (front-facing): {ccw_count}/{total} ({100*ccw_count/total:.1f}%)")
    print(f"CW  (back-facing):  {cw_count}/{total} ({100*cw_count/total:.1f}%)")
    
    if cw_count > 0:
        print(f"\n❌ {cw_count} triangles will be culled by glCullFace(GL_BACK) with glFrontFace(GL_CCW)")
    else:
        print(f"\n✅ All triangles are CCW (front-facing)")

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else "data/octree_sphere_spider_binary.stl"
    check_winding_direction(path)
