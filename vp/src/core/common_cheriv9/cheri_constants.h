#pragma once
#include <cstdint>

// This file contains constants, that are independent of rv32 / rv64 architecture

constexpr uint8_t reserved_otypes = 4;
constexpr int8_t cReservedOtypes = 4;
constexpr int8_t cOtypeUnsealed = -1;
constexpr int8_t cOtypeSentry = -2;

constexpr uint8_t cPccIdx = 0b100000;
constexpr uint8_t cDdcIdx = 0b100001;
