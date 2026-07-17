# Project Directory Structure

## 根目录布局

```
delaunay/
├── CMakeLists.txt          # 顶层构建配置，串联所有模块和测试
├── STRUCTURE.md            # 本文件 — 目录结构管理规则
│
├── modules/                # [核心模块] 每个模块一个子目录，编译为静态库
│   ├── delaunay2d/         #   2D Delaunay 三角剖分
│   ├── delaunay3d/         #   3D Delaunay 四面体剖分
│   ├── aft2d/              #   2D Advancing Front 网格生成
│   ├── mesher2d/           #   2D 网格生成器（上层接口）
│   ├── quadtree/           #   自适应四叉树网格生成
│   ├── octree/             #   自适应八叉树 3D 网格生成
│   ├── meshutils/          #   网格统计、质量优化、导出（OBJ/STL/VTK）
│   ├── meshviewer/         #   Qt5 + OpenGL 3.3 网格可视化
│   └── benchmark/          #   性能基准测试
│
├── test/                   # [测试程序] 所有测试源文件集中存放
│   ├── CMakeLists.txt      #   测试目标构建配置
│   ├── test_*.cpp          #   每个文件编译为一个独立可执行文件
│   └── ...
│
├── data/                   # [测试数据] 测试的输入/输出数据文件
│   ├── *.obj               #   2D/3D OBJ 文件
│   ├── *.stl               #   ASCII/二进制 STL 文件
│   ├── *.vtk               #   VTK 可视化文件
│   ├── *.qmesh             #   Quadtree 格式文件
│   └── ...
│
├── lib/                    # [构建产物] 可执行文件，按配置分目录
│   ├── Debug/              #   Debug 版 .exe
│   └── Release/            #   Release 版 .exe
│
└── build/                  # [构建中间文件] CMake 编译临时文件
    └── ...                 #   可随时删除，不影响源码
```

## 规则

### 1. 模块代码 → `modules/<name>/`

- 每个独立功能单元必须是一个模块，放在 `modules/<name>/` 下
- 模块目录结构固定：
  ```
  modules/<name>/
  ├── CMakeLists.txt       # 模块构建配置（add_library）
  ├── include/             # 公开头文件
  │   └── <Name>.h
  ├── src/                 # 实现文件
  │   └── <Name>.cpp
  └── test/ (可选)         # 模块私有测试
  ```
- 模块应自包含，只依赖其他模块的公开接口
- 顶层 `CMakeLists.txt` 通过 `add_subdirectory()` 引入

### 2. 测试代码 → `test/`

- **所有**测试源文件必须放在 `test/` 目录下，**不允许**放在根目录
- 每个测试文件编译为一个独立可执行文件
- 在 `test/CMakeLists.txt` 中添加对应目标，链接所需的模块库

### 3. 测试数据 → `data/`

- 测试程序生成的输出文件（`.obj`, `.stl`, `.vtk`, `.qmesh` 等）
  必须写入 `data/` 目录，路径前缀为 `"data/"`
- 测试输入数据文件也应放在 `data/` 下
- 禁止在根目录或其他位置生成数据文件

### 4. 构建产物 → `lib/`

- 所有编译生成的可执行文件由 CMake 自动输出到 `lib/$<CONFIG>/`
- **不允许**手动编译的可执行文件留在根目录 — 应移入 `lib/Debug/` 或 `lib/Release/`
- 如需手动编译，指定输出路径为 `lib/Debug/`（Debug 版）或 `lib/Release/`（Release 版）

### 5. 构建中间文件 → `build/`

- CMake 构建的临时文件（`.obj`, `.d`, `compile_commands.json` 等）
  全部在 `build/` 下，可随时删除重建
- 不纳入版本控制（已在 `.gitignore` 中排除）

### 6. 根目录

- 只保留：`CMakeLists.txt`、`STRUCTURE.md`、`.gitignore`、`README.md` 等
  顶层元文件
- **禁止**在根目录放置任何 `.cpp`、`.h`、`.exe`、`.obj`、`.stl`、`.vtk`、
  `.qmesh` 等代码或数据文件
