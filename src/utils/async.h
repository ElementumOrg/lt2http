#pragma once

#include <future>

// This is a trick to run std::async without waiting for a promise.
template <class F>
void call_async(F&& fun) {
    auto futptr = std::make_shared<std::future<void>>();
    *futptr = std::async(std::launch::async, [futptr, fun]() {
        fun();
    });
}
