/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2026  nextpnr contributors
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef DELAY_WRAP_H
#define DELAY_WRAP_H

#include <cstdint>
#include <limits>
#include <type_traits>

#include "nextpnr_namespaces.h"

NEXTPNR_NAMESPACE_BEGIN

// Arithmetic on delays that can be sentinels of the timing analysis: reset_times() starts the arrival and required
// times at (INT_MAX, INT_MIN), and the analysis goes on adding to and subtracting from them. The result has no meaning
// but is kept, so the sum, difference or negation is taken modulo 2^N on the unsigned type (a signed overflow is
// undefined) and mapped back to the signed range. A floating-point delay (generic) is used as it is.
template <typename T> T wrap_to_signed(std::make_unsigned_t<T> value)
{
    using U = std::make_unsigned_t<T>;
    // needs two's complement, where the signed type has as many values as the unsigned one
    static_assert(std::numeric_limits<T>::min() == -std::numeric_limits<T>::max() - 1,
                  "delay_t must be two's complement");
    constexpr U max = static_cast<U>(std::numeric_limits<T>::max());
    // [max + 1, 2^N - 1] stands for the negative numbers min .. -1
    if (value <= max)
        return static_cast<T>(value);
    return static_cast<T>(value - max - 1) + std::numeric_limits<T>::min();
}

template <typename T> T wrap_add(T a, T b)
{
    if constexpr (std::is_integral_v<T>) {
        using U = std::make_unsigned_t<T>;
        return wrap_to_signed<T>(static_cast<U>(static_cast<U>(a) + static_cast<U>(b)));
    } else {
        return a + b;
    }
}

template <typename T> T wrap_sub(T a, T b)
{
    if constexpr (std::is_integral_v<T>) {
        using U = std::make_unsigned_t<T>;
        return wrap_to_signed<T>(static_cast<U>(static_cast<U>(a) - static_cast<U>(b)));
    } else {
        return a - b;
    }
}

// The negation as the unary minus does it: modulo 2^N for an integer, -a for a floating-point value (the sign of a zero
// matters there)
template <typename T> T wrap_neg(T a)
{
    if constexpr (std::is_integral_v<T>) {
        return wrap_sub(T(0), a);
    } else {
        return -a;
    }
}

// A floating-point value converted to an integer type: out of range becomes the smallest or the largest value (NaN the
// smallest), anything else is truncated as in a plain conversion, which is undefined for an out-of-range value.
template <typename T, typename F> T saturating_cast(F value)
{
    if constexpr (std::is_integral_v<T>) {
        if (!(value > static_cast<F>(std::numeric_limits<T>::min())))
            return std::numeric_limits<T>::min();
        if (value >= static_cast<F>(std::numeric_limits<T>::max()))
            return std::numeric_limits<T>::max();
        return static_cast<T>(value);
    } else {
        return static_cast<T>(value);
    }
}

// The slack of the histogram: the delay is subtracted from the period in float, as it always was, and the result
// converted to the delay type.
template <typename T> T slack_from_period(float period, T delay) { return saturating_cast<T>(period - delay); }

NEXTPNR_NAMESPACE_END

#endif
