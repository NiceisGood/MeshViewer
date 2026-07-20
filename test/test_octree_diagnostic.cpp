#include "Octree.h"
#include "MeshUtils.h"
#include <cstdio>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <tuple>

struct STLFace { float v[3][3]; };
static bool read_stl_ascii(const std::string& path, std::vector<STLFace>& faces)
{
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    char line[256];
    float verts[3][3];
    int vi = 0;
    while (std::fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (std::strncmp(s, "vertex", 6) == 0) {
            float x, y, z;
            if (std::sscanf(s + 6, "%f %f %f", &x, &y, &z) == 3) {
                verts[vi][0] = x; verts[vi][1] = y; verts[vi][2] = z;
                ++vi;
                if (vi == 3) {
                    STLFace face;
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            face.v[i][j] = verts[i][j];
                    faces.push_back(face);
                    vi = 0;
                }
            }
        }
    }
    std::fclose(f);
    return true;
}

int main()
{
    // 1. Read input STL
    std::vector<STLFace> faces;
    read_stl_ascii("data/sphere_spider.stl", faces);
    std::printf("Input: %zu triangles\n", faces.size());

    // 2. Build octree
    float xmin=1e30f,ymin=1e30f,zmin=1e30f,xmax=-1e30f,ymax=-1e30f,zmax=-1e30f;
    for (const auto& f : faces)
        for (int i=0;i<3;++i)
            for (int j=0;j<3;++j) {
                if (f.v[i][j] < (&xmin)[j]) (&xmin)[j] = f.v[i][j];
                if (f.v[i][j] > (&xmax)[j]) (&xmax)[j] = f.v[i][j];
            }
    float pad = std::max({xmax-xmin,ymax-ymin,zmax-zmin})*0.1f;
    xmin-=pad;ymin-=pad;zmin-=pad;xmax+=pad;ymax+=pad;zmax+=pad;

    Octree oct(xmin,ymin,zmin,xmax,ymax,zmax,6);
    oct.build([&](double cx,double cy,double cz,double hs,int d)->bool{
        if(d>=5)return false;
        double th=hs*2*1.5,md2=1e30;
        for(auto&f:faces)for(int i=0;i<3;++i){
            double d2=(cx-f.v[i][0])*(cx-f.v[i][0])+(cy-f.v[i][1])*(cy-f.v[i][1])+(cz-f.v[i][2])*(cz-f.v[i][2]);
            if(d2<md2)md2=d2;
        }
        return std::sqrt(md2)<th;
    });

    // 3. Tetrahedralize
    std::vector<OctPoint3D> pts;
    std::vector<OctTetrahedron> tets;
    tets=oct.tetrahedralize(pts);
    std::printf("Mesh: %zu pts, %zu tets, %d nodes, %d leaves\n",
                pts.size(),tets.size(),oct.num_nodes(),oct.num_leaves());

    // 4. Build face->tet map: each face (sorted) -> list of (tet_idx, orig_winding)
    struct FaceRec{
        int sv[3]; // sorted vertices
        int ov[3]; // original winding
        int ti;
        bool operator<(const FaceRec& o)const{
            for(int i=0;i<3;++i)if(sv[i]!=o.sv[i])return sv[i]<o.sv[i];
            return false;
        }
    };
    std::vector<FaceRec> all_faces;
    for(size_t ti=0;ti<tets.size();++ti){
        auto&t=tets[ti];
        int faces[4][3]={{t.v0,t.v2,t.v1},{t.v0,t.v1,t.v3},{t.v1,t.v2,t.v3},{t.v0,t.v3,t.v2}};
        for(auto&fv:faces){
            int sv[3]={fv[0],fv[1],fv[2]};
            std::sort(sv,sv+3);
            all_faces.push_back({{sv[0],sv[1],sv[2]},{fv[0],fv[1],fv[2]},(int)ti});
        }
    }
    std::sort(all_faces.begin(),all_faces.end());

    // Extract boundary faces
    struct BndTri{
        int ov[3]; double cx,cy,cz;
        int depth; // depth of the tetrahedron's cube
    };
    std::vector<BndTri> bnd;
    for(size_t i=0;i<all_faces.size();++i){
        bool unique=true;
        if(i>0){auto&p=all_faces[i-1];if(p.sv[0]==all_faces[i].sv[0]&&p.sv[1]==all_faces[i].sv[1]&&p.sv[2]==all_faces[i].sv[2])unique=false;}
        if(i+1<all_faces.size()){auto&n=all_faces[i+1];if(n.sv[0]==all_faces[i].sv[0]&&n.sv[1]==all_faces[i].sv[1]&&n.sv[2]==all_faces[i].sv[2])unique=false;}
        if(unique){
            BndTri bt;
            bt.ov[0]=all_faces[i].ov[0];bt.ov[1]=all_faces[i].ov[1];bt.ov[2]=all_faces[i].ov[2];
            auto&p0=pts[bt.ov[0]],&p1=pts[bt.ov[1]],&p2=pts[bt.ov[2]];
            bt.cx=(p0.x+p1.x+p2.x)/3;bt.cy=(p0.y+p1.y+p2.y)/3;bt.cz=(p0.z+p1.z+p2.z)/3;
            bnd.push_back(bt);
        }
    }
    std::printf("Surface: %zu triangles\n\n",bnd.size());

    // 5. For each surface triangle, compute normal and check winding
    int inward_count=0, outward_count=0;
    double eps=1e-10;
    for(auto&bt:bnd){
        auto&p0=pts[bt.ov[0]],&p1=pts[bt.ov[1]],&p2=pts[bt.ov[2]];
        double ex1=p1.x-p0.x,ey1=p1.y-p0.y,ez1=p1.z-p0.z;
        double ex2=p2.x-p0.x,ey2=p2.y-p0.y,ez2=p2.z-p0.z;
        double nx=ey1*ez2-ez1*ey2, ny=ez1*ex2-ex1*ez2, nz=ex1*ey2-ey1*ex2;
        
        // Normal should point away from mesh center (0,0,0)
        double dot=nx*bt.cx+ny*bt.cy+nz*bt.cz;
        if(dot>0)outward_count++;
        else inward_count++;
    }
    std::printf("Winding check (center heuristic):\n");
    std::printf("  Outward: %d/%zu (%.1f%%)\n",outward_count,bnd.size(),100.0*outward_count/bnd.size());
    std::printf("  Inward:  %d/%zu (%.1f%%)\n",inward_count,bnd.size(),100.0*inward_count/bnd.size());

    // 6. Check a specific cell: pick a leaf near the surface and list all faces
    //    We look for cells at depth 4 or 5 near the mesh center, and check
    //    if the 6 faces of the cube have 2 triangles each.
    std::printf("\n=== Specific cell face check ===\n");
    
    // Pick leaf cells near center of the domain
    int checked=0;
    for(int li=0;li<(int)oct.num_nodes()&&checked<3;++li){
        // We can't access nodes_ directly, so let's check using the surface mesh
        // Instead, find a face on a known plane
    }
    
    // Alternative: check by finding groups of 2 triangles on same plane
    // with shared edges (they should form a cube face)
    std::map<std::string,int> plane_counts;
    for(auto&bt:bnd){
        auto&p0=pts[bt.ov[0]],&p1=pts[bt.ov[1]],&p2=pts[bt.ov[2]];
        // Check for axis-aligned faces
        if(std::abs(p0.x-p1.x)<eps&&std::abs(p1.x-p2.x)<eps){
            char k[64];std::snprintf(k,64,"x=%.10f",p0.x);plane_counts[k]++;
        }
        if(std::abs(p0.y-p1.y)<eps&&std::abs(p1.y-p2.y)<eps){
            char k[64];std::snprintf(k,64,"y=%.10f",p0.y);plane_counts[k]++;
        }
        if(std::abs(p0.z-p1.z)<eps&&std::abs(p1.z-p2.z)<eps){
            char k[64];std::snprintf(k,64,"z=%.10f",p0.z);plane_counts[k]++;
        }
    }
    
    std::printf("\nAll unique axis-aligned planes and triangle counts:\n");
    for(auto&pc:plane_counts){
        const char* mark=pc.second%2==0?"✓":"⚠️ ODD";
        std::printf("  %s: %d tri %s\n",pc.first.c_str(),pc.second,mark);
    }
    
    // Count how many planes have exactly 2 triangles (ideal cube face)
    int perfect=0,odd=0;
    for(auto&pc:plane_counts){
        if(pc.second==2)perfect++;
        else if(pc.second%2==1)odd++;
    }
    std::printf("\nPerfect cube faces (2 tris): %d\n",perfect);
    std::printf("Odd-count planes: %d\n",odd);
    
    return 0;
}
