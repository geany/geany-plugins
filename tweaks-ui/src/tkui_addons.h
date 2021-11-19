/*
 * Addons - Tweaks-UI Plugin for Geany
 * Copyright 2021 xiota
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <geanyplugin.h>

#include <string>

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

#define GFREE(_z_) \
  do {             \
    g_free(_z_);   \
    _z_ = nullptr; \
  } while (0)

#define GERROR_FREE(_z_) \
  do {                   \
    g_error_free(_z_);   \
    _z_ = nullptr;       \
  } while (0)

#define GEANY_PSC(sig, cb, data)                                            \
  plugin_signal_connect(geany_plugin, nullptr, (sig), false, G_CALLBACK(cb), \
                        data)

#define GEANY_PSC_AFTER(sig, cb, data)                                            \
  plugin_signal_connect(geany_plugin, nullptr, (sig), true, G_CALLBACK(cb), \
                        data)

#if GLIB_MAJOR_VERSION <= 2 && GLIB_MINOR_VERSION < 58
#define G_SOURCE_FUNC(f) ((GSourceFunc)(void (*)(void))(f))
#endif  // G_SOURCE_FUNC

#if GLIB_MAJOR_VERSION <= 2 && GLIB_MINOR_VERSION < 62
#include "gobject/gsignal.h"
#define g_clear_signal_handler(handler, instance)      \
  do {                                                 \
    if (handler != nullptr && *handler != 0) {         \
      g_signal_handler_disconnect(instance, *handler); \
      *handler = 0;                                    \
    }                                                  \
  } while (0)
#endif  // g_clear_signal_handler
