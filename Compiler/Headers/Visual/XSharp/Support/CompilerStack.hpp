// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace Visual::XSharp::Support
{
    /**
     * @brief Stack size, in bytes, of a thread that runs the compiler.
     *
     * The stages that walk Core recurse once per level of nesting in the
     * program, so how deep a program may nest is decided by the stack of the
     * thread that compiles it. The stack a process starts with differs by
     * platform: one megabyte on Windows, eight on Linux and macOS, and half
     * a megabyte for secondary threads on macOS. A limit that is safe on
     * one of them would be either a crash on another or needlessly small
     * everywhere. The compiler therefore runs on a thread whose stack it
     * chooses itself, and the nesting limits of the frontend are stated
     * against this size.
     *
     * Only address space is reserved; pages are committed as they are
     * touched, so an ordinary program pays for the stack it uses.
     */
    inline constexpr std::size_t kCompilerStackBytes
        = std::size_t{ 256U } * 1024U * 1024U;

    /**
     * @brief Run a function on a new thread with the compiler stack and
     * wait for it.
     *
     * The stack is reserved, not committed. The process is terminated with
     * a report when the thread cannot be created: there is no smaller stack
     * on which the caller's limits would still hold.
     *
     * @param function Called once on the new thread with `argument`.
     * @param argument Passed through unchanged.
     */
    void
    RunOnCompilerStackRaw(void (*function)(void *), void *argument);

    /**
     * @brief Run a callable on a thread with the compiler stack and return
     * its result.
     *
     * The caller waits for the thread, so the callable may refer to the
     * caller's locals. The callable must not throw; first-party code is
     * built without exceptions.
     */
    template<typename Callable>
    auto
    RunOnCompilerStack(Callable &&callable) -> std::invoke_result_t<Callable &>
    {
        using Result = std::invoke_result_t<Callable &>;
        if constexpr (std::is_void_v<Result>)
        {
            RunOnCompilerStackRaw(
                [](void *context) {
                    (*static_cast<std::remove_reference_t<Callable> *>(
                        context))();
                },
                &callable);
        }
        else
        {
            struct Call final
            {
                std::remove_reference_t<Callable> *callable;
                std::optional<Result> result;
            };
            Call call{ &callable, std::nullopt };
            RunOnCompilerStackRaw(
                [](void *context) {
                    auto *self = static_cast<Call *>(context);
                    self->result.emplace((*self->callable)());
                },
                &call);
            return std::move(*call.result);
        }
    }
} // namespace Visual::XSharp::Support
