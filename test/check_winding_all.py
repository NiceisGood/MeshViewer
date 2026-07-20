"""Verify ALL tetrahedron face windings for outward-facing correctness."""
v0, v1, v2, v3 = (0,0,0), (1,0,0), (0,1,0), (0,0,1)

def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])

def dot(a, b):
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]

def check_face(name, verts, away_from):
    e1 = sub(verts[1], verts[0]); e2 = sub(verts[2], verts[0])
    n = cross(e1, e2)
    d = dot(n, sub(away_from, verts[0]))
    ok = d < 0  # normal should point AWAY from the opposite vertex
    print(f"{name}: ({verts[0]},{verts[1]},{verts[2]}) "
          f"normal={n} dot={d:+d} -> {'OUTWARD ✓' if ok else 'INWARD ✗'}")
    return ok

all_ok = True
# Face opposite v3: vertices v0,v1,v2, code uses (v0, v2, v1)
all_ok &= check_face("opposite v3", [v0, v2, v1], v3)
# Face opposite v2: vertices v0,v1,v3, code uses (v0, v1, v3)
all_ok &= check_face("opposite v2", [v0, v1, v3], v2)
# Face opposite v0: vertices v1,v2,v3, code uses (v1, v2, v3)
all_ok &= check_face("opposite v0", [v1, v2, v3], v0)
# Face opposite v1: vertices v0,v2,v3, code uses (v0, v3, v2)
all_ok &= check_face("opposite v1", [v0, v3, v2], v1)

print(f"\n{'✅ All faces outward' if all_ok else '❌ Some faces inward'}")
