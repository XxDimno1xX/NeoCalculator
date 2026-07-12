/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * GiacEngineInternal.h — transition-only access to the engine-owned context.
 *
 * FOR src/math/giac/ TUs ONLY (currently the legacy GiacBridge UART path).
 * Apps must use GiacEngine.h. This exists so GiacBridge can delegate instead
 * of owning a second giac::context; it disappears when the UART path is
 * rebuilt on the public API.
 */

#pragma once

namespace giac { class context; }

namespace numos {
namespace giacinternal {

/// Ensures GiacEngine::begin() ran, then returns THE context. Never null
/// after a successful begin(); returns nullptr only on allocation failure.
giac::context* sharedContext();

} // namespace giacinternal
} // namespace numos
