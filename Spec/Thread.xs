// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Thread system:

//
// The Thread module provides:
//
// - Native thread creation
// - Move-based thread closures
// - Join handles
// - Thread result values
// - Detached threads
// - Thread sleeping
// - Cooperative yielding
// - Single-sender, single-receiver channels
//
// Thread synchronization failures use the Result error channel.
//
// Thread values and channel endpoints follow the language's ownership,
// move and Send rules.
//

import thread, result;


// ============================================================
// Thread creation
// ============================================================


// thread without result

fn spawn_thread() {
    std::thread::spawn(move fn() {
        // Thread body
    });
}


// thread with result

fn spawn_thread_with_result() {
    thread: Thread::handle<Int> =
        std::thread::spawn(move fn() {
            return 42;
        });

    value: Int = thread.join();
}


// resultless thread handle

fn resultless_thread_handle() {
    thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            return;
        });

    thread.join();
}


// std::thread::spawn():
//
// - Creates a native thread.
// - Accepts a move closure.
// - Moves captured values into the new thread.
// - Returns Thread::handle<T> when the closure returns T.
// - Returns Thread::handle<()> when the closure returns no value.
// - Begins execution immediately.
// - The closure must satisfy the required Send rules.


// ============================================================
// Move captures
// ============================================================

fn move_value_to_thread() {
    value: Str = "Alpha";

    thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            local_value: Str = value;
        });

    thread.join();

    // value can no longer be used here.
}


// A value captured by a move thread closure:
//
// - Is moved into the new thread.
// - Can no longer be used by the spawning thread.
// - Must satisfy Send.


// invalid non-Send capture

fn invalid_non_send_capture() {
    value: NonSendType = new NonSendType();

    std::thread::spawn(move fn() {
        Use(value);
    });
}

// Moving a non-Send value into another thread is a compile-time error.


// ============================================================
// Thread handles
// ============================================================

fn move_thread_handle() {
    first: Thread::handle<Int> =
        std::thread::spawn(move fn() {
            return 42;
        });

    second: Thread::handle<Int> = first;

    value: Int = second.join();

    // first can no longer be used.
}


// Thread::handle<T>:
//
// - Is move-only.
// - Cannot be copied.
// - May move to another thread when T: Send.
// - Is not Sync.
// - Represents ownership of the join operation.
//
// Trait rules:
//
//
// Thread::handle<T>: Send if T: Send
// Thread::handle<T>: not Sync
//


// ============================================================
// Joining
// ============================================================

fn join_thread() {
    thread: Thread::handle<Int> =
        std::thread::spawn(move fn() {
            return 42;
        });

    value: Int = thread.join();
}

// join():
//
// - Blocks until the thread finishes.
// - Returns Result<T, Error>.
// - Consumes the Thread::handle<T>.
// - May only be called once.
// - Carries thread failure through Error.
// - Returns an Error variant carrying an Error object for invalid runtime join operations.


// invalid second join

fn invalid_second_join() {
    thread: Thread::handle<Int> =
        std::thread::spawn(move fn() {
            return 42;
        });

    value: Int = thread.join();
    second_value: Int = thread.join();
}

// The second join() is a compile-time error because join()
// consumes the thread handle.


// self join

// A thread attempting to join itself returns an Error variant carrying an Error object.


// ============================================================
// Thread Result failures
// ============================================================

fn joined_thread_failure() {
    thread: Thread::handle<Result<Int, Error>> =
        std::thread::spawn(move fn() {
            return Error(new Error("I/O error"));
        });

    result: Result<Int, Error> = thread.join()@;
    if (result.is_error()) {
        eprintln!("thread failed");
    }
}

// If a joined thread returns Error:
//
// - The error is stored by the thread runtime.
// - join() transfers that error to the joining thread.
// - The error may be matched or propagated with @.


// detached thread failure

fn detached_thread_failure() {
    std::thread::spawn(move fn() {
        return Error(new Error("I/O error"));
    });
}

// If a detached thread returns Error:
//
// - Only that thread is terminated.
// - The runtime emits a diagnostic.
// - The entire process is not automatically terminated.


// ============================================================
// Detached threads
// ============================================================

fn detached_thread() {
    std::thread::spawn(move fn() {
        // Continues independently.
    });
}

// A thread is detached when its returned handle is not stored.
//
// Detached threads:
//
// - Continue running independently.
// - Cannot later be joined.
// - Do not provide their result to the spawning thread.
// - Are terminated when the process itself exits.


// ============================================================
// Sleeping and yielding
// ============================================================

fn sleep_thread() {
    std::thread::sleep(1000);
}

// std::thread::sleep(milliseconds):
//
// - Suspends the current native thread.
// - Uses milliseconds.
// - Blocks the current thread.
// - Does not suspend only the current async task.


// cooperative yield

fn yield_thread() {
    std::thread::yield();
}

// std::thread::yield():
//
// - Voluntarily gives the scheduler an opportunity to run
//   another ready thread.
// - Does not guarantee that another thread will run.
// - Does not release held Mutex or RwLock guards.


// ============================================================
// Channels
// ============================================================


// basic channel

fn channel_example() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    std::thread::spawn(move fn() {
        tx.send(42);
    });

    received: Result<Int, Error> = rx.recv();
}


// std::thread::Channel<T>:
//
// - Creates one sender endpoint and one receiver endpoint.
// - Uses a single-sender, single-receiver model.
// - Sender and receiver endpoints are move-only.
// - Endpoints cannot be cloned.
// - Endpoints may move between threads when T: Send.


// sending

fn send_channel_value() {
    (tx, rx): std::thread::Channel<Str> =
        std::thread::channel();

    message: Str = "Alpha";

    tx.send(message);

    // message can no longer be used.
}

// tx.send(value):
//
// - Transfers ownership of value into the channel.
// - The sent value can no longer be used by the sender.
// - Returns Result<(), Error>; Error means that the channel is closed.


// receiving

fn receive_channel_value() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    std::thread::spawn(move fn() {
        tx.send(42);
    });

    received: Result<Int, Error> = rx.recv();
}

// rx.recv():
//
// - Blocks until a value becomes available.
// - Returns Result<T, Error>.
// - Transfers ownership of the received value to the receiver.
// - Returns an Error variant carrying an Error object if the channel is closed and no value can be received.


// moving receiver to another thread

fn move_receiver_to_thread() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    receiver_thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            received: Result<Int, Error> = rx.recv();
        });

    tx.send(42);

    receiver_thread.join();
}


// moving sender to another thread

fn move_sender_to_thread() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    sender_thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            tx.send(42);
        });

    received: Result<Int, Error> = rx.recv();

    sender_thread.join();
}


// invalid sender clone

fn invalid_sender_clone() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    other_tx: std::thread::Channel<Int> =
        std::thread::Channel::clone(&tx);
}

// Channel endpoints cannot be cloned.
// This is a compile-time error.


// invalid receiver clone

fn invalid_receiver_clone() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    other_rx: std::thread::Channel<Int> =
        std::thread::Channel::clone(&rx);
}

// Channel endpoints cannot be cloned.
// This is a compile-time error.


// ============================================================
// Interaction with synchronization types
// ============================================================

import mutex, rw_lock, arc;


// moving a mutex into a thread

fn move_mutex_into_thread() {
    mutex: Mutex<Int> = new Mutex<Int>(42);

    thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            guard: Mutex<Int> = mutex.lock();
            *guard += 1;
        });

    thread.join();
}


// sharing a mutex between threads

fn share_mutex_between_threads() {
    shared: Arc<Mutex<Int>> =
        new Arc<Mutex<Int>>(new Mutex<Int>(42));

    worker: Arc<Mutex<Int>> =
        Arc::clone(&shared);

    thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            guard: Mutex<Int> = worker.lock();
            *guard += 1;
        });

    thread.join();

    guard: Mutex<Int> = shared.lock();
}


// sharing an rwlock between threads

fn share_rw_lock_between_threads() {
    shared: Arc<RwLock<Int>> =
        new Arc<RwLock<Int>>(new RwLock<Int>(42));

    worker: Arc<RwLock<Int>> =
        Arc::clone(&shared);

    thread: Thread::handle<()> =
        std::thread::spawn(move fn() {
            reader: RwLock<Int> = worker.read();
            value: Int = *reader;
        });

    thread.join();
}


// Guard values:
//
// - Mutex lock guards cannot move to another thread.
// - RwLock reader guards cannot move to another thread.
// - RwLock writer guards cannot move to another thread.
//
// Moving such a guard into std::thread::spawn() is a compile-time error.


// ============================================================
// Unsupported thread operations
// ============================================================

// The Thread module does not provide:
//
// - Thread cancellation
// - Thread termination or kill
// - Thread priority control
// - CPU affinity control
// - Thread naming
// - Public thread identifier API
// - Cloneable thread handles
// - Cloneable channel endpoints
