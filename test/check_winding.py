#!/usr/bin/env python3
"""Check triangle winding consistency in a binary STL file."""
import struct, sys

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

def check_winding(path):
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
    
    # For each triangle, compute the face normal from the vertex winding
    # and compare with the STL file's stored normal
    mismatched = 0
    inward_count = 0
    for i, (v0, v1, v2, stl_normal) in enumerate(tris):
        # Compute normal from winding: (v1-v0) × (v2-v0)
        e1 = vec_sub(v1, v0)
        e2 = vec_sub(v2, v0)
        computed_normal = cross(e1, e2)
        nlen = (computed_normal[0]**2 + computed_normal[1]**2 + computed_normal[2]**2)**0.5
        if nlen > 0:
            computed_normal = vec_scale(computed_normal, 1.0/nlen)
        
        # Dot product with STL normal
        d = dot(computed_normal, stl_normal)
        if d < 0:
            inward_count += 1
            if mismatched < 5:
                print(f"  Tri {i}: winding normal ({computed_normal[0]:.4f},{computed_normal[1]:.4f},{computed_normal[2]:.4f}) "
                      f"opposes STL normal ({stl_normal[0]:.4f},{stl_normal[1]:.4f},{stl_normal[2]:.4f}) "
                      f"dot={d:.4f}")
                mismatched += 1
    
    if mismatched > 0:
        print(f"\n❌ {inward_count}/{len(tris)} triangles have winding opposite to STL normal "
              f"({100*inward_count/len(tris):.1f}%)")
    else:
        print(f"\n✅ All {len(tris)} triangles: winding matches STL normal")
    
    # Group triangles by face (same plane, shared vertices)
    # Check if adjacent triangles on same cube face have opposite windings
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
    
    # Find edges shared by exactly 2 triangles (should be all edges)
    # Check if those 2 triangles have consistent winding on the shared edge
    inconsistent_pairs = 0
    for e, tris_list in edge_to_tris.items():
        if len(tris_list) == 2:
            t1, t2 = tris_list
            # Get the vertex indices for each triangle
            iv1 = tri_verts[t1]
            iv2 = tri_verts[t2]
            # Check if the shared edge appears in opposite order in the two triangles
            # If triangle1 has edge (a,b) and triangle2 has edge (a,b) in same order,
            # the windings are opposite (one triangle faces one way, the other the other way)
            e0, e1 = e
            # Find position of e0, e1 in each triangle
            def edge_order(tri, a, b):
                for j in range(3):
                    if tri[j] == a and tri[(j+1)%3] == b:
                        return 1  # a→b
                    if tri[j] == b and tri[(j+1)%3] == a:
                        return -1  # b→a
                return 0
            
            order1 = edge_order(iv1, e0, e1)
            order2 = edge_order(iv2, e0, e1)
            if order1 == order2:
                inconsistent_pairs += 1
    
    print(f"\nEdge sharing: {len(edge_to_tris)} unique edges")
    print(f"Adjacent triangle pairs with CONSISTENT winding on shared edge: {len(edge_to_tris) - inconsistent_pairs}")
    print(f"Adjacent triangle pairs with INCONSISTENT winding on shared edge: {inconsistent_pairs}")

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else "data/octree_sphere_spider_binary.stl"
    check_winding(path)
