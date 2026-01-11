#pragma once

#include "../../Common/IEvents.hpp"

#include "../stdafx.hpp"

#include <queue>

#include <wayland-client.h>
#include "Protocols/xdg-shell-client-protocol.h"

class WaylandEvents final : public IEvents
{
private:
    static std::queue<EventData> eventQueue;

	wl_seat* pSeat = nullptr;
	wl_keyboard* pKeyboard = nullptr;
	wl_pointer* pPointer = nullptr;
private:
    static void HandleSeatCapabilities(void* data, wl_seat* seat, uint32_t caps);
    inline static const wl_seat_listener gSeatListener = { HandleSeatCapabilities };

    static void HandleKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int fd, uint32_t size);
    static void HandleKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
    static void HandleKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
    static void HandleKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    static void HandleKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
    inline static const wl_keyboard_listener gKeyboardListener = {
        HandleKeyboardKeymap,
        HandleKeyboardEnter,
        HandleKeyboardLeave,
        HandleKeyboardKey,
        HandleKeyboardModifiers
    };

    static void HandlePointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy);
    static void HandlePointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
    static void HandlePointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    static void HandlePointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
    static void HandlePointerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
    inline static const wl_pointer_listener gPointerListener = {
        HandlePointerEnter,
        HandlePointerLeave,
        HandlePointerMotion,
        HandlePointerButton,
        HandlePointerAxis
    };

    inline static void PushEvent(const EventData& e) { eventQueue.push(e); }
public:
    WaylandEvents();
    ~WaylandEvents() override = default;

    bool Poll(EventData& outEvent) override;
};
