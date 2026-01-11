#include "WaylandWindow.hpp"
#include "WaylandEvents.hpp"

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static int CreateAnonymousFile(off_t size)
{
    const char* name = "/wayland-shm-buffer";
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink(name);
    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static wl_buffer* CreateDummyBuffer(wl_shm* shm, int width, int height)
{
    const int stride = width * 4;
    const int size = stride * height;

    int fd = CreateAnonymousFile(size);
    if (fd < 0)
        return nullptr;

    uint32_t* data = (uint32_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        close(fd);
        return nullptr;
    }

    // Simple solid color (dark grey) in XRGB8888
    for (int i = 0; i < width * height; ++i)
        data[i] = 0xFF202020;

    wl_shm_pool* pool = wl_shm_create_pool(shm, fd, size);
    wl_buffer* buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        width,
        height,
        stride,
        WL_SHM_FORMAT_XRGB8888
    );

    wl_shm_pool_destroy(pool);
    close(fd);

    return buffer;
}

WaylandWindow::WaylandWindow(const WindowConfig& cfg)
    : config(cfg)
{
    currentWidth = cfg.size.width > 0 ? cfg.size.width : cfg.minSize.width;
    currentHeight = cfg.size.height > 0 ? cfg.size.height : cfg.minSize.height;

    pDisplay = wl_display_connect(nullptr);
    if (!pDisplay)
    {
        std::cout << "Failed to connect display." << std::endl;
        return;
    }

    pRegistry = wl_display_get_registry(pDisplay);
    wl_registry_add_listener(pRegistry, &gRegistryListener, this);

    // Process global announcements (compositor, xdg_wm_base, shm, decoration manager, etc.)
    wl_display_roundtrip(pDisplay);

    if (!pCompositor || !pWmBase)
    {
        std::cout << "Compositor or WmBase is nullptr." << std::endl;
        return;
    }

    pSurface = wl_compositor_create_surface(pCompositor);
    if (!pSurface)
    {
        std::cout << "Failed to create compositor surface." << std::endl;
        return;
    }

    pXdgSurface = xdg_wm_base_get_xdg_surface(pWmBase, pSurface);
    xdg_surface_add_listener(pXdgSurface, &xXdgSurfaceListener, this);

    pXdgTopLevel = xdg_surface_get_toplevel(pXdgSurface);
    xdg_toplevel_add_listener(pXdgTopLevel, &gTopLevelListener, this);

    xdg_toplevel_set_title(pXdgTopLevel, config.title.c_str());

    // Request server-side decorations if supported
    if (pDecorationManager)
    {
        pDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            pDecorationManager,
            pXdgTopLevel
        );

        zxdg_toplevel_decoration_v1_set_mode(
            pDecoration,
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
        );
    }

    // Initial empty commit to trigger the first configure
    wl_surface_commit(pSurface);
    wl_display_flush(pDisplay);

    // Ensure the initial configure events are delivered
    wl_display_roundtrip(pDisplay);
}

WaylandWindow::~WaylandWindow()
{
    if (pDecoration)
    {
        zxdg_toplevel_decoration_v1_destroy(pDecoration);
        pDecoration = nullptr;
    }

    if (pXdgTopLevel)
    {
        xdg_toplevel_destroy(pXdgTopLevel);
        pXdgTopLevel = nullptr;
    }

    if (pXdgSurface)
    {
        xdg_surface_destroy(pXdgSurface);
        pXdgSurface = nullptr;
    }

    if (pSurface)
    {
        wl_surface_destroy(pSurface);
        pSurface = nullptr;
    }

    if (pDisplay)
    {
        wl_display_disconnect(pDisplay);
        pDisplay = nullptr;
    }

    pCompositor = nullptr;
    pRegistry = nullptr;
    pShm = nullptr;
    pBuffer = nullptr;
    pWmBase = nullptr;
    pDecorationManager = nullptr;
}

void WaylandWindow::HandleGlobal(void* data, wl_registry* registry,
    uint32_t name, const char* interface, uint32_t version)
{
    auto* window = reinterpret_cast<WaylandWindow*>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        window->pCompositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4)
        );
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    {
        window->pWmBase = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1)
        );
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        window->pShm = static_cast<wl_shm*>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1)
        );
    }
    else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
    {
        window->pDecorationManager = static_cast<zxdg_decoration_manager_v1*>(
            wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1)
        );
    }
}

void WaylandWindow::HandleGlobalRemove(void* /*data*/, wl_registry* /*registry*/, uint32_t /*name*/)
{
    // For now, ignore removal; a robust client would react here.
}

void WaylandWindow::HandleXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial)
{
    auto* self = reinterpret_cast<WaylandWindow*>(data);
    if (!self)
        return;

    std::cout << "[Wayland] xdg_surface.configure serial=" << serial << "\n";
    std::cout << "  pShm=" << self->pShm << " pBuffer=" << self->pBuffer
        << " size=" << self->currentWidth << "x" << self->currentHeight << "\n";

    // Ack FIRST, per spec
    xdg_surface_ack_configure(surface, serial);

    if (!self->pShm || !self->pSurface)
    {
        std::cout << "  Missing shm or surface, cannot create buffer.\n";
        wl_surface_commit(self->pSurface);
        return;
    }

    // Create or recreate buffer matching current size
    self->pBuffer = CreateDummyBuffer(self->pShm, self->currentWidth, self->currentHeight);

    if (self->pBuffer)
    {
        wl_surface_attach(self->pSurface, self->pBuffer, 0, 0);
        std::cout << "  Buffer created and attached, committing surface.\n";
    }
    else
    {
        std::cout << "  Failed to create buffer.\n";
    }

    wl_surface_commit(self->pSurface);
}

void WaylandWindow::HandleTopLevelConfigure(void* data, xdg_toplevel* /*toplevel*/,
    int32_t width, int32_t height, wl_array* /*states*/)
{
    auto* self = reinterpret_cast<WaylandWindow*>(data);
    if (!self)
        return;

    if (width > 0)  self->currentWidth = width;
    if (height > 0) self->currentHeight = height;

    std::cout << "[Wayland] xdg_toplevel.configure width="
        << self->currentWidth << " height=" << self->currentHeight << "\n";
}

void WaylandWindow::HandleTopLevelClose(void* data, xdg_toplevel* /*toplevel*/)
{
    auto* self = reinterpret_cast<WaylandWindow*>(data);
    if (!self)
        return;

    // You likely want to push a close event into your engine's queue here.
    std::cout << "[Wayland] xdg_toplevel.close\n";
}
