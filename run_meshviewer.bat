@echo off
set PATH=D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin;%PATH%
E:\agent\hermescode\delaunay\lib\Release\meshviewer.exe E:\agent\hermescode\delaunay\data\octree_sphere_spider.stl
echo Exit code: %ERRORLEVEL%
pause
