#pragma once
#ifdef _WIN32
#  include <d3d11.h>

// DX11 device and context（需从你的渲染器获取）
extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

#endif