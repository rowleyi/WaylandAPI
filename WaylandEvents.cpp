#include "WaylandEvents.hpp"

#include "../../WindowManager.hpp"

static bool s_InputEnabled = true;

std::queue<EventData> WaylandEvents::eventQueue;

static KeyCodeEnum TranslateWaylandKey(uint32_t key)
{
	switch (key)
	{
	case 1: return KeyCodeEnum::Escape;
	case 28: return KeyCodeEnum::Enter;
	case 57: return KeyCodeEnum::Space;
	case 15: return KeyCodeEnum::Tab;
	case 14: return KeyCodeEnum::Backspace;
	case 105: return KeyCodeEnum::Left;
	case 106: return KeyCodeEnum::Right;
	case 103: return KeyCodeEnum::Up;
	case 108: return KeyCodeEnum::Down;
	case 42:
	case 54: return KeyCodeEnum::Shift;
	case 29:
	case 97: return KeyCodeEnum::Ctrl;
	case 56:
	case 100: return KeyCodeEnum::Alt;
	default: return KeyCodeEnum::Unknown;
	}
}

static KeyCodeEnum TranslateWaylandMouseButton(uint32_t button)
{
	constexpr uint32_t BIN_LEFT = 0x110;
	constexpr uint32_t BIN_RIGHT = 0x111;
	constexpr uint32_t BIN_MIDDLE = 0x112;

	switch (button)
	{
	case BIN_LEFT: return KeyCodeEnum::MouseLeft;
	case BIN_RIGHT: return KeyCodeEnum::MouseRight;
	case BIN_MIDDLE: return KeyCodeEnum::MouseMiddle;
	default: return KeyCodeEnum::Unknown;
	}
}

WaylandEvents::WaylandEvents()
{
	auto* pWindow = WindowManager::GetWindowBackend();
	if (!pWindow)
		return;

	wl_display* display = reinterpret_cast<wl_display*>(pWindow->GetNativeDisplay());
	wl_surface* surface = reinterpret_cast<wl_surface*>(pWindow->GetNativeWindow());
	if (!display || !surface)
		return;
}

bool WaylandEvents::Poll(EventData& outEvent)
{
	auto* pWindow = WindowManager::GetWindowBackend();
	if (pWindow)
	{
		wl_display* display = reinterpret_cast<wl_display*>(pWindow->GetNativeDisplay());
		if (display)
		{
			wl_display_dispatch(display);
			wl_display_flush(display);
		}
	}

	if (eventQueue.empty())
		return false;

	outEvent = eventQueue.front();
	eventQueue.pop();
	return true;
}

void WaylandEvents::HandleSeatCapabilities(void* data, wl_seat* seat, uint32_t caps)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self)
		return;

	const uint32_t pointerCap = WL_SEAT_CAPABILITY_POINTER;
	const uint32_t keyboardCap = WL_SEAT_CAPABILITY_KEYBOARD;

	if ((caps & pointerCap) && !self->pPointer)
	{
		self->pPointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(self->pPointer, &gPointerListener, self);
	}
	else if (!(caps & pointerCap) && self->pPointer)
	{
		wl_pointer_destroy(self->pPointer);
		self->pKeyboard = nullptr;
	}
}

void WaylandEvents::HandleKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int fd, uint32_t size)
{
	if (fd >= 0)
		close(fd);
}

void WaylandEvents::HandleKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self)
		return;

	s_InputEnabled = true;

	EventData e{};
	e.type = EventType::WindowFocus;
	PushEvent(e);
}

void WaylandEvents::HandleKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self)
		return;

	s_InputEnabled = false;

	EventData e{};
	e.type = EventType::WindowLostFocus;
	PushEvent(e);
}

void WaylandEvents::HandleKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self || !s_InputEnabled)
		return;

	const bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);

	EventData e{};
	e.type = pressed ? EventType::KeyDown : EventType::Keyup;
	e.key.key = TranslateWaylandKey(key);
	PushEvent(e);
}

void WaylandEvents::HandleKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
}

void WaylandEvents::HandlePointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy)
{
}

void WaylandEvents::HandlePointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface)
{
}

void WaylandEvents::HandlePointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self || !s_InputEnabled)
		return;

	const double x = wl_fixed_to_double(sx);
	const double y = wl_fixed_to_double(sy);

	EventData e{};
	e.type = EventType::MouseMove;
	e.mouseMove.x = static_cast<int>(x);
	e.mouseMove.y = static_cast<int>(y);
	PushEvent(e);
}

void WaylandEvents::HandlePointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self || !s_InputEnabled)
		return;

	const bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

	EventData e{};
	e.type = pressed ? EventType::MouseDown : EventType::MouseUp;
	e.mouseButton.button = TranslateWaylandMouseButton(button);
	PushEvent(e);
}

void WaylandEvents::HandlePointerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	auto* self = static_cast<WaylandEvents*>(data);
	if (!self || !s_InputEnabled)
		return;

	const double delta = wl_fixed_to_double(value);

	EventData e{};
	e.type = EventType::MouseScroll;
	e.scroll.dx = 0;
	e.scroll.dy = 0;

	constexpr uint32_t WL_POINTER_AXIS_VERTICAL_SCROLL = 0;
	constexpr uint32_t WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1;

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
		e.scroll.dy = static_cast<int>(delta);
	else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
		e.scroll.dx = static_cast<int>(delta);

	PushEvent(e);
}
