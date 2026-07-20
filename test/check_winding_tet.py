"""Verify tetrahedron face winding."""
v0, v1, v2, v3 = (0,0,0), (1,0,0), (0,1,0), (0,0,1)

def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])

def dot(a, b):
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]

# Face opposite v1: vertices v0, v2, v3
e1 = sub(v2, v0); e2 = sub(v3, v0)
n = cross(e1, e2)
print(f"(v0,v2,v3) normal={n} dot(v1-v0)={dot(n, sub(v1,v0)):+.0f} -> {'INWARD' if dot(n, sub(v1,v0))>0 else 'OUTWARD'}")

e1 = sub(v3, v0); e2 = sub(v2, v0)
n = cross(e1, e2)
print(f"(v0,v3,v2) normal={n} dot(v1-v0)={dot(n, sub(v1,v0)):+.0f} -> {'INWARD' if dot(n, sub(v1,v0))>0 else 'OUTWARD'}")
