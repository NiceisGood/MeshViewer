#!/usr/bin/env python3
"""Check if triangles face outward by comparing normal with center-to-triangle direction."""
import struct, sys, math

def read_stl_binary(path):
    with open(path, 'rb') as f:
        f.read(80)
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

def tri_center(a, b, c):
    return ((a[0]+b[0]+c[0])/3, (a[1]+b[1]+c[1])/3, (a[2]+b[2]+c[2])/3)

def check_outward(path):
    tris = read_stl_binary(path)
    print(f"Total triangles: {len(tris)}")
    
    # Compute mesh center (average of all vertices)
    all_v = []
    for v0, v1, v2, _ in tris:
        all_v.extend([v0, v1, v2])
    center = (sum(v[0] for v in all_v)/len(all_v),
              sum(v[1] for v in all_v)/len(all_v),
              sum(v[2] for v in all_v)/len(all_v))
    print(f"Mesh center: ({center[0]:.4f}, {center[1]:.4f}, {center[2]:.4f})")
    
    inward = 0
    outward = 0
    samples = []
    
    for i, (v0, v1, v2, stl_normal) in enumerate(tris):
        # Compute geometric normal from winding
        e1 = vec_sub(v1, v0)
        e2 = vec_sub(v2, v0)
        geo_normal = cross(e1, e2)
        nlen = vec_len(geo_normal)
        if nlen < 1e-12:
            continue
        
        # Direction from mesh center to triangle center
        tc = tri_center(v0, v1, v2)
        to_center = vec_sub(center, tc)
        
        # The normal should point away from center (outward)
        # So dot(normal, tc - center) > 0 means outward
        to_tri = vec_sub(tc, center)
        d = dot(geo_normal, to_tri)
        
        if d > 0:
            outward += 1
        else:
            inward += 1
            if len(samples) < 10:
                samples.append((i, v0, v1, v2, d))
    
    total = inward + outward
    print(f"Outward-facing: {outward}/{total} ({100*outward/total:.1f}%)")
    print(f"Inward-facing:  {inward}/{total} ({100*inward/total:.1f}%)")
    
    if inward > 0:
        print(f"\n❌ {inward} triangles face INWARD and will be culled with back-face culling")
        print("\nSample inward-facing triangles:")
        for i, v0, v1, v2, d in samples[:5]:
            tc = tri_center(v0, v1, v2)
            print(f"  Tri {i}: dot={d:.4f} center=({tc[0]:.4f},{tc[1]:.4f},{tc[2]:.4f})")
    else:
        print(f"\n✅ All triangles face outward")

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else "data/octree_sphere_spider_binary.stl"
    check_outward(path)
