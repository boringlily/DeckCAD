// Unity include hub for App.cpp: brings the shared 3D canvas helpers (Canvas.cpp)
// and the toolset registry (Toolbox.cpp) into App.cpp's translation unit so the Ui
// tree (UiCanvas / UiToolbox) can reach them. The Clay DrawWorkbench / DrawExplorer
// were removed with the Clay teardown.
#include "Canvas/Canvas.cpp"
#include "Toolbox/Toolbox.cpp"
