#pragma once
// App: patch use cases — dirty tracking, slot save/load, boot init, factory
// defaults, autosave. Called by UI/main; built on app/serialization.h
// (format) and app/storage_port.h (transport port, implemented in infra).
#include "app/serialization.h"

// Autosave contract with main.cpp: UI edits mark_dirty(); the main loop
// ticks patch_autosave_tick() to persist SLOT_ACTIVE.
bool state_dirty();
void mark_dirty();

// Bank/active slot use cases.
void patch_save(StoreSlot slot);
bool patch_load(StoreSlot slot);
// Boot: engine defaults, then restore SLOT_ACTIVE when valid.
void patch_init();
// Factory defaults behind the SYS "defaults" action.
void patch_defaults();
// Main-loop tick: saves SLOT_ACTIVE at most every 3 s while dirty.
void patch_autosave_tick(uint32_t now_ms);
