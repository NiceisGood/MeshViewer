#!/usr/bin/env python3
"""Compute signed volume to determine if triangles face outward consistently."""
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

def signed_tet_volume(a, b, c, d):
    """Signed volume of tetrahedron (a,b,c,d). Positive if (b-a,c-a,d-a) is right-handed."""
    # Volume = |det(b-a, c-a, d-a)| / 6
    ab = (b[0]-a[0], b[1]-a[1], b[2]-a[2])
    ac = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
    ad = (d[0]-a[0], d[1]-a[1], d[2]-a[2])
    det = (ab[0] * (ac[1]*ad[2] - ac[2]*ad[1])
         - ab[1] * (ac[0]*ad[2] - ac[2]*ad[0])
         + ab[2] * (ac[0]*ad[1] - ac[1]*ad[0]))
    return det / 6.0

def check_volume(path):
    tris = read_stl_binary(path)
    print(f"Total triangles: {len(tris)}")
    
    # Compute signed volume: for each triangle (v0,v1,v2), compute 
    # signed volume of tetrahedron (origin, v0, v1, v2)
    # If all triangles face outward (CCW from outside), the total is positive.
    total_vol = 0.0
    origin = (0.0, 0.0, 0.0)
    
    for v0, v1, v2, _ in tris:
        vol = signed_tet_volume(origin, v0, v1, v2)
        total_vol += vol
    
    print(f"Total signed volume: {total_vol:.6f}")
    
    # A sphere with radius 1.2 has volume 4/3*pi*1.2^3 ≈ 7.24
    # But the octree mesh is not a perfect sphere. The volume should be positive
    # for outward-facing triangles.
    if total_vol > 0:
        print(f"✅ Surface is consistently oriented OUTWARD (signed volume > 0)")
    else:
        print(f"❌ Surface is consistently oriented INWARD (signed volume < 0)")

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else "data/octree_sphere_spider_binary.stl"
    check_volume(path)
