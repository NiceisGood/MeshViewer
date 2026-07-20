#!/usr/bin/env python3
"""Check surface mesh for consistency by analyzing edge sharing."""
import struct

def read_stl_binary(path):
    """Read binary STL, return list of triangles (each = 3 (x,y,z) tuples)."""
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
            tris.append((v0, v1, v2))
    return tris

def quantize(v, eps=1e-6):
    """Quantize a vertex to a grid for deduplication."""
    return (round(v[0] / eps) * eps,
            round(v[1] / eps) * eps,
            round(v[2] / eps) * eps)

def analyze_surface(path):
    tris = read_stl_binary(path)
    print(f"Total triangles: {len(tris)}")
    
    # Deduplicate vertices
    vtx_map = {}
    vtx_list = []
    tri_verts = []
    for tri in tris:
        idxs = []
        for v in tri:
            qv = quantize(v)
            if qv not in vtx_map:
                vtx_map[qv] = len(vtx_list)
                vtx_list.append(qv)
            idxs.append(vtx_map[qv])
        tri_verts.append(tuple(idxs))
    
    print(f"Unique vertices: {len(vtx_list)}")
    
    # Build edge->triangle mapping
    edge_to_tris = {}
    for ti, (i0, i1, i2) in enumerate(tri_verts):
        edges = [
            (min(i0, i1), max(i0, i1)),
            (min(i1, i2), max(i1, i2)),
            (min(i2, i0), max(i2, i0)),
        ]
        for e in edges:
            if e not in edge_to_tris:
                edge_to_tris[e] = []
            edge_to_tris[e].append(ti)
    
    # Analyze edge counts
    count_dist = {}
    for e, tris_list in edge_to_tris.items():
        n = len(tris_list)
        count_dist[n] = count_dist.get(n, 0) + 1
    
    print("\nEdge sharing statistics:")
    for cnt in sorted(count_dist.keys()):
        print(f"  {cnt} triangle(s) share edge: {count_dist[cnt]} edges")
    
    # A closed manifold: each edge belongs to exactly 2 triangles
    non_manifold = [n for n in count_dist if n != 2]
    if non_manifold:
        print(f"\n⚠️  Non-manifold edges found! (should be 2 per edge)")
        # Show sample non-manifold edges
        for e, tris_list in edge_to_tris.items():
            if len(tris_list) != 2:
                v0 = vtx_list[e[0]]
                v1 = vtx_list[e[1]]
                print(f"  Edge ({e[0]},{e[1]}) ~ ({v0[0]:.3f},{v0[1]:.3f},{v0[2]:.3f})-({v1[0]:.3f},{v1[1]:.3f},{v1[2]:.3f}): {len(tris_list)} tris")
                if len(non_manifold) > 1 and len([x for x in edge_to_tris if len(edge_to_tris[x]) != 2]) > 10:
                    print(f"  ... and {len([x for x in edge_to_tris if len(edge_to_tris[x]) != 2]) - 1} more")
                    break
    else:
        print("\n✅ Surface is a closed 2-manifold!")
    
    # Check for boundary edges (belongs to 1 triangle) - these are holes
    boundary_edges = count_dist.get(1, 0)
    if boundary_edges > 0:
        print(f"\n❌ {boundary_edges} boundary edges found = {boundary_edges // 2} boundary edge-pairs = SURFACE HAS HOLES")
    else:
        print("\n✅ No boundary edges — surface is watertight")

if __name__ == '__main__':
    import sys
    path = sys.argv[1] if len(sys.argv) > 1 else "data/octree_sphere_spider_binary.stl"
    analyze_surface(path)
