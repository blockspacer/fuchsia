// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stdint.h>

namespace overnet {
namespace varint {

// Return the number of bytes required to represent x
// This result must be passed into Write, and may be cached
uint8_t WireSizeFor(uint64_t x);

// Write varint based on pre-calculated length, returns dst + wire_length as a
// convenience
uint8_t* Write(uint64_t x, uint8_t wire_length, uint8_t* dst);

}  // namespace varint

}  // namespace overnet
