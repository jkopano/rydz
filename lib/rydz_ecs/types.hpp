#pragma once
#include <cstddef>
#include <cstdint>

// Typy całkowite bez znaku (Unsigned)
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Typy całkowite ze znakiem (Signed)
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Zmiennoprzecinkowe
using f32 = float;
using f64 = double;

// Typy zależne od architektury procesora (odpowiedniki usize/isize w Rust)
// Używane do indeksowania tablic i pętli
using usize =
    std::size_t; // 64-bit na systemie 64-bitowym, 32-bit na 32-bitowym
using isize = std::ptrdiff_t;
