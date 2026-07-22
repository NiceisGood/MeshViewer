import struct, sys
# Quick test: write a v2 file and verify read-back works
with open('data/test_quad_reader.qmesh3d', 'wb') as f:
    f.write(b'QMESH3D')
    struct.pack_into('<I', bytearray(4), 0, 2)
    f.write(struct.pack('<I', 2))  # version 2
    # bounds (6 doubles)
    f.write(struct.pack('<6d', 0,0,0,1,1,1))
    # 2 points
    f.write(struct.pack('<I', 2))
    f.write(struct.pack('<6d', 0,0,0, 1,1,1))
    # 1 tet (v0,v1,v2,v3 - but we use v0=v1=v2=v3 for a degenerate tet)
    f.write(struct.pack('<I', 1))
    f.write(struct.pack('<4i', 0,1,0,1))
    # 1 quad (4 indices)
    f.write(struct.pack('<I', 1))
    f.write(struct.pack('<4i', 0,1,2,3))

print('Wrote test file. Now reading with meshviewer reader...')
