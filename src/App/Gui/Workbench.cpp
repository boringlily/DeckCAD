// Unity include hub for App.cpp: brings the shared 3D canvas helpers (Canvas.cpp) into
// App.cpp's translation unit so the Ui tree (UiCanvas) can reach them.
//
// Toolbox.cpp's toolset registry is gone: the toolbox no longer has a hardcoded tab
// list filtered by a TabContext enum. It renders whatever the current context reports
// from AvailableTools(), grouped by the ToolInfo table (Core/Context/ToolId.h).
#include "Canvas/Canvas.cpp"
