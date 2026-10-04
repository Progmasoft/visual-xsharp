// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <llvm/Support/ErrorHandling.h>

#include "Visual/XSharp/Support/CompilerStack.hpp"

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <process.h>
#    include <windows.h>
#else
#    include <pthread.h>
#endif

namespace Visual::XSharp::Support
{
    namespace
    {
        struct Start final
        {
            void (*function)(void *);
            void *argument;
        };

#if defined(_WIN32)
        unsigned __stdcall
        Enter(void *context)
        {
            const auto *start = static_cast<const Start *>(context);
            start->function(start->argument);
            return 0U;
        }
#else
        void *
        Enter(void *context)
        {
            const auto *start = static_cast<const Start *>(context);
            start->function(start->argument);
            return nullptr;
        }
#endif
    } // namespace

    void
    RunOnCompilerStackRaw(void (*function)(void *), void *argument)
    {
        Start start{ function, argument };
#if defined(_WIN32)
        // Without the reservation flag the size would be committed up
        // front: every run would charge the whole stack against the
        // system's commit limit and pay for mapping it.
        const auto handle
            = _beginthreadex(nullptr,
                             static_cast<unsigned>(kCompilerStackBytes),
                             Enter,
                             &start,
                             STACK_SIZE_PARAM_IS_A_RESERVATION,
                             nullptr);
        if (handle == 0U)
            llvm::report_fatal_error("could not start the compiler thread");
        const auto thread = reinterpret_cast<HANDLE>(handle);
        if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
            llvm::report_fatal_error("could not wait for the compiler thread");
        CloseHandle(thread);
#else
        // A pthread stack is mapped lazily, so the size is a reservation.
        pthread_attr_t attributes;
        if (pthread_attr_init(&attributes) != 0
            || pthread_attr_setstacksize(&attributes, kCompilerStackBytes) != 0)
            llvm::report_fatal_error(
                "could not size the stack of the compiler thread");
        pthread_t thread;
        if (pthread_create(&thread, &attributes, Enter, &start) != 0)
            llvm::report_fatal_error("could not start the compiler thread");
        pthread_attr_destroy(&attributes);
        if (pthread_join(thread, nullptr) != 0)
            llvm::report_fatal_error("could not wait for the compiler thread");
#endif
    }
} // namespace Visual::XSharp::Support
