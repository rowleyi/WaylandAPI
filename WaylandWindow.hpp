#pragma once

#include "../../Common/IWindow.hpp"

#include "../stdafx.hpp"

#include <cstring>
#include <string>

#include <wayland-client.h>

extern "C"
{
	#include "Protocols/xdg-shell-client-protocol.h"
	#include "Protocols/xdg-decoration-client-protocol.h"
}

class WaylandWindow final : public IWindow
{
private:
	wl_display* pDisplay = nullptr;
	wl_registry* pRegistry = nullptr;
	wl_compositor* pCompositor = nullptr;
	wl_surface* pSurface = nullptr;

	xdg_wm_base* pWmBase = nullptr;
	xdg_surface* pXdgSurface = nullptr;
	xdg_toplevel* pXdgTopLevel = nullptr;

	wl_shm* pShm = nullptr;
	wl_buffer* pBuffer = nullptr;

	zxdg_decoration_manager_v1* pDecorationManager = nullptr;
	zxdg_toplevel_decoration_v1* pDecoration = nullptr;

	int32_t currentWidth = 1;
	int32_t currentHeight = 1;

	WindowConfig config{};
private:
	static void HandleGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
	static void HandleGlobalRemove(void* data, wl_registry* registry, uint32_t name);
	inline static const wl_registry_listener gRegistryListener = { HandleGlobal, HandleGlobalRemove };
	static void HandlePing(void* data, xdg_wm_base* base, uint32_t serial) { xdg_wm_base_pong(base, serial); }
	inline static const xdg_wm_base_listener gWmBaseListener = { HandlePing };
	static void HandleXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial);
	inline static const xdg_surface_listener xXdgSurfaceListener = { HandleXdgSurfaceConfigure };
	static void HandleTopLevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
	static void HandleTopLevelClose(void* data, xdg_toplevel* toplevel);
	inline static const xdg_toplevel_listener gTopLevelListener = { HandleTopLevelConfigure, HandleTopLevelClose };
public:
	explicit WaylandWindow(const WindowConfig& cfg);
	~WaylandWindow() override;

	inline const WindowConfig& GetConfig() const override { return config; }

	inline WindowBackend GetBackend() const override { return WindowBackend::Unknown; }
	inline bool IsValid() const override { return pSurface != nullptr; }
	inline void* GetNativeWindow() const override { return reinterpret_cast<void*>(pSurface); }
#if defined(__linux__) || defined(__unix__)
	inline void* GetNativeDisplay() const override { return reinterpret_cast<void*>(pDisplay); }
#endif

	inline void Show() override { return; }
	inline void Hide() override { return; }
};
