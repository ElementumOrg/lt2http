#pragma once

#include <type_traits>

template <typename T, typename = void>
struct HasUnderlyingType
    : std::false_type
{
};

template <typename T>
struct HasUnderlyingType<T, std::void_t<typename T::underlying_type>>
    : std::true_type
{
};

template <typename T, typename = void>
struct LTUnderlying
{
    using type = T;
};

template <typename T>
struct LTUnderlying<T, typename std::enable_if_t<HasUnderlyingType<T>::value>>
{
    using type = typename T::underlying_type;
};

template <typename T>
using LTUnderlyingType = typename LTUnderlying<T>::type;
