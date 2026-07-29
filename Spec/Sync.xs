// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Sync system:

//
// The synchronization system provides:
//
// - Mutex
// - RwLock
// - Arc
// - Weak
// - WeakControl
// - Atomic values
// - Async tasks
// - Built-in async thread pool
// - Thread channels
//
// Synchronization misuse may produce standard Error payloads.
//
// Error is carried through an Error variant carrying an Error object.
//
// Statically provable synchronization violations are compile-time errors.
// Violations that can only be detected at runtime return an Error variant carrying an Error object.
//


// ============================================================
// Mutex
// ============================================================

import mutex;


// mutex creation

fn mutex_creation() {
    counter: Mutex<Long> = new Mutex<Long>(0);
}


// locking

fn mutex_lock() {
    counter: Mutex<Long> = new Mutex<Long>(0);

    value: Mutex<Long> = counter.lock();

    *value += 1;
}

// lock():
//
// - Waits until the mutex can be acquired.
// - Does not return None.
// - Does not provide try_lock().
// - Returns Mutex<T>.
// - The returned Mutex<T> acts as the lock guard.
// - The guard provides mutable access through *guard.
// - The lock is released automatically when the guard
//   leaves its scope.


// automatic unlocking

fn mutex_automatic_unlock() {
    counter: Mutex<Long> = new Mutex<Long>(0);

    {
        value: Mutex<Long> = counter.lock();
        *value += 1;
    }

    // The lock has been released.

    second_value: Mutex<Long> = counter.lock();
    *second_value += 1;
}


// same-thread guard movement

fn move_mutex_guard() {
    counter: Mutex<Long> = new Mutex<Long>(0);

    first_guard: Mutex<Long> = counter.lock();
    second_guard: Mutex<Long> = first_guard;

    *second_guard += 1;
}

// After the move, first_guard can no longer be used.
//
// A Mutex guard may move inside the same thread.
// A Mutex guard may not move to another thread.


// non-reentrant mutex

fn invalid_recursive_mutex_lock() {
    counter: Mutex<Long> = new Mutex<Long>(0);

    first_guard: Mutex<Long> = counter.lock();

    second_guard: Mutex<Long> = counter.lock();
}

// The second lock() is a compile-time error because the
// same thread already holds the mutex.
//
// Mutex<T> is not reentrant.


// poisoning

// If a thread terminates with an error while holding a mutex,
// the mutex becomes poisoned.
//
// A later lock() operation returns an Error variant carrying an Error object.


// poisoned mutex recovery

fn recover_poisoned_mutex() {
    counter: Mutex<Long> = new Mutex<Long>(0);

    result: Result<Mutex<Long>, Error> = counter.lock();
    match (result) {
        Ok(value) -> {},
        Error(error) -> {
            counter.unlock();
        },
    }
}

// unlock():
//
// - Primarily exists to recover a poisoned mutex.
// - May also manually unlock a normal mutex.
// - Manual unlocking during normal use is discouraged.
// - Automatic scope-based unlocking should be preferred.
// - Calling unlock() from a thread that does not own the
//   lock returns an Error variant carrying an Error object.
// - Calling unlock() when the lock is already open returns
//   an Error variant carrying an Error object.


// fairness

// Mutex uses a fair FIFO waiting queue.
//
// The thread that begins waiting first receives the lock first.
// Waiting threads are not starved.


// mutex trait rules

//
// Mutex<T>: Send if T: Send
// Mutex<T>: Sync if T: Send
//


// mutex movement between threads

import thread;

fn move_mutex_to_thread() {
    counter: Mutex<Long> = new Mutex<Long>(42);

    std::thread::spawn(move fn() {
        value: Mutex<Long> = counter.lock();
        *value += 1;
    });

    // counter can no longer be used here.
}


// ============================================================
// RwLock
// ============================================================

import rw_lock;


// rwlock creation

fn rw_lock_creation() {
    value: RwLock<Int> = new RwLock<Int>(42);
}


// read lock

fn rw_lock_read() {
    value: RwLock<Int> = new RwLock<Int>(42);

    reader: RwLock<Int> = value.read();

    result: Int = *reader;
}

// read():
//
// - Waits until a read lock can be acquired.
// - Returns RwLock<T>.
// - The returned value acts as a reader guard.
// - The reader provides immutable access.
// - The reader lock is released automatically when the
//   guard leaves its scope.


// invalid reader mutation

fn invalid_reader_mutation() {
    value: RwLock<Int> = new RwLock<Int>(42);

    reader: RwLock<Int> = value.read();

    *reader = 50;
}

// Assigning through a reader is a compile-time error.


// write lock

fn rw_lock_write() {
    value: RwLock<Int> = new RwLock<Int>(42);

    writer: RwLock<Int> = value.write();

    *writer += 1;
}

// write():
//
// - Waits until an exclusive write lock can be acquired.
// - Returns RwLock<T>.
// - The returned value acts as a writer guard.
// - The writer provides mutable access.
// - The writer lock is released automatically when the
//   guard leaves its scope.


// multiple readers

fn multiple_readers() {
    value: RwLock<Int> = new RwLock<Int>(42);

    reader1: RwLock<Int> = value.read();
    reader2: RwLock<Int> = value.read();

    first: Int = *reader1;
    second: Int = *reader2;
}

// Multiple readers may hold the lock simultaneously.


// writer exclusivity

// While a writer is active:
//
// - New readers wait.
// - New writers wait.
//
// A writer begins only after all active readers have left.


// reader and writer guard movement

fn move_rw_lock_guards() {
    value: RwLock<Int> = new RwLock<Int>(42);

    reader: RwLock<Int> = value.read();
    other_reader: RwLock<Int> = reader;

    // reader can no longer be used.
}

// Reader and writer guards:
//
// - May move within the same thread.
// - May not move to another thread.


// non-reentrant rwlock

fn invalid_read_to_write_upgrade() {
    value: RwLock<Int> = new RwLock<Int>(42);

    reader: RwLock<Int> = value.read();
    writer: RwLock<Int> = value.write();
}

// Acquiring write() while the same thread holds read()
// is a compile-time error when statically visible.


// invalid writer reentry

fn invalid_writer_reentry() {
    value: RwLock<Int> = new RwLock<Int>(42);

    writer: RwLock<Int> = value.write();

    reader: RwLock<Int> = value.read();
    second_writer: RwLock<Int> = value.write();
}

// Both operations are invalid.
//
// RwLock<T> is not reentrant.
//
// Dynamically detected reentry violations return an Error variant carrying an Error object.


// rwlock poisoning

// If a thread terminates with an error while holding a writer
// lock, the RwLock becomes poisoned.
//
// Later read() or write() calls return an Error variant carrying an Error object.


// poisoned rwlock recovery

fn recover_poisoned_rw_lock() {
    value: RwLock<Int> = new RwLock<Int>(42);

    result: Result<RwLock<Int>, Error> = value.write();
    match (result) {
        Ok(writer) -> {},
        Error(error) -> {
            value.rwunlock();
        },
    }
}

// rwunlock():
//
// - Primarily exists to recover a poisoned RwLock.
// - May manually unlock a normal RwLock.
// - Manual use during normal operation is discouraged.
// - Calling rwunlock() from a thread that does not own the
//   relevant lock returns an Error variant carrying an Error object.
// - Calling rwunlock() when no relevant lock is held returns
//   an Error variant carrying an Error object.


// rwlock fairness

// RwLock uses one shared fair FIFO queue for readers and writers.
//
// Consecutive readers at the front of the queue may run together.
//
// Example:
//
// R1, R2, W1, R3
//
// Execution:
//
// R1 and R2 run together.
// W1 runs after both readers leave.
// R3 runs after W1 leaves.
//
// Example:
//
// R1, R2, R3, W1
//
// Execution:
//
// R1, R2 and R3 run together.
// W1 runs after all three readers leave.
//
// Example:
//
// R1, W1, R2, R3
//
// Execution:
//
// R1 runs.
// W1 runs after R1 leaves.
// R2 and R3 run together after W1 leaves.


// rwlock trait rules

//
// RwLock<T>: Send if T: Send
// RwLock<T>: Sync if T: Send + Sync
//


// ============================================================
// Arc
// ============================================================

import arc;


// arc creation

fn arc_creation() {
    value: Arc<Str> = new Arc<Str>("Alpha");
}


// arc cloning

fn arc_cloning() {
    value: Arc<Str> = new Arc<Str>("Alpha");

    second: Arc<Str> = Arc::clone(&value);
    third: Arc<Str> = Arc::clone(&value);
}

// Arc::clone(&value):
//
// - Borrows value immutably.
// - Does not move value.
// - Atomically increments the strong reference counter.
// - May be called repeatedly while value remains alive.


// arc access

fn arc_read() {
    strong: Arc<Int> = new Arc<Int>(42);

    value: Int = *strong;
}

// Dereferencing Arc<T> provides immutable access.


// invalid arc mutation

fn invalid_arc_mutation() {
    strong: Arc<Int> = new Arc<Int>(42);

    *strong = 50;
}

// Direct mutation through Arc<T> is a compile-time error.


// arc destruction

// When the final strong Arc<T> owner is destroyed:
//
// - The strong counter reaches zero.
// - The contained T value is destroyed automatically.
//
// Arc uses atomic reference counting.


// arc trait rules

//
// Arc<T>: Send if T: Send + Sync
// Arc<T>: Sync if T: Send + Sync
//


// shared mutex

fn shared_mutex() {
    shared: Arc<Mutex<Str>> =
        new Arc<Mutex<Str>>(new Mutex<Str>("Alpha"));

    second: Arc<Mutex<Str>> =
        Arc::clone(&shared);

    guard: Mutex<Str> = shared.lock();

    *guard = "Beta";
}


// shared rwlock

fn shared_rw_lock() {
    shared: Arc<RwLock<Int>> =
        new Arc<RwLock<Int>>(new RwLock<Int>(42));

    second: Arc<RwLock<Int>> =
        Arc::clone(&shared);

    reader: RwLock<Int> = shared.read();
}


// ============================================================
// Weak and WeakControl
// ============================================================

import WeakControl;


// weak creation

fn weak_creation() {
    strong: Arc<Str> = new Arc<Str>("Alpha");

    weak: Weak<Str> = Arc::downgrade(&strong);
}

// Arc::downgrade(&strong):
//
// - Creates a Weak<T>.
// - Atomically increments the weak reference counter.
// - Does not increase the strong reference counter.
// - Does not extend the lifetime of the contained value.


// weak cloning

fn weak_cloning() {
    strong: Arc<Str> = new Arc<Str>("Alpha");
    weak: Weak<Str> = Arc::downgrade(&strong);

    other_weak: Weak<Str> =
        WeakControl::clone(&weak);
}

// WeakControl::clone(&weak):
//
// - Atomically increments the same weak reference counter
//   used by Arc::downgrade().
// - Does not increment the strong reference counter.


// weak upgrade

fn weak_upgrade() {
    strong: Arc<Str> = new Arc<Str>("Alpha");
    weak: Weak<Str> = Arc::downgrade(&strong);

    strong_again: Arc<Str> =
        WeakControl::upgrade(&weak);
}

// WeakControl::upgrade(&weak):
//
// - Atomically increments the strong counter when successful.
// - Returns Result<Arc<T>, Error>.
// - Returns an Error variant carrying an Error object if the contained value has already been destroyed.


// weak destruction

// Weak<T> does not keep the contained value alive.
//
// When the strong count reaches zero:
//
// - The contained value is destroyed.
//
// When the final Weak<T> is also destroyed:
//
// - The shared control block is released.


// weak trait rules

//
// Weak<T>: Send if T: Send + Sync
// Weak<T>: Sync if T: Send + Sync
//


// ============================================================
// Atomic
// ============================================================

import atomic;


// atomic creation

fn atomic_creation() {
    Atomic counter: Int = new Atomic<Int>(42);
    Atomic flag: Bool = new Atomic<Bool>(false);
}

// new Atomic<T>() requires a compile-time constant initial value.


// supported atomic value types

//
// Short
// Long
// Int
// Integer
//
// UShort
// ULong
// UInt
// UInteger
//
// Bool
//


// atomic ordering values

//
// Atomic.ordering.Relaxed
// Atomic.ordering.Acquire
// Atomic.ordering.Release
// Atomic.ordering.AcqRel
// Atomic.ordering.SeqCst
//


// atomic load

fn atomic_load() {
    Atomic counter: Int = new Atomic<Int>(42);

    value: Int =
        counter.load(Atomic.ordering.SeqCst);
}


// atomic store

fn atomic_store() {
    Atomic counter: Int = new Atomic<Int>(42);

    counter.store(
        43,
        Atomic.ordering.SeqCst
    );
}


// atomic exchange

fn atomic_exchange() {
    Atomic counter: Int = new Atomic<Int>(42);

    old_value: Int =
        counter.exchange(
            50,
            Atomic.ordering.SeqCst
        );
}

// exchange() returns the previous value.


// atomic addition

fn atomic_fetch_add() {
    Atomic counter: Int = new Atomic<Int>(42);

    old_value: Int =
        counter.fetch_add(
            1,
            Atomic.ordering.SeqCst
        );
}

// fetch_add() returns the previous value.


// atomic subtraction

fn atomic_fetch_sub() {
    Atomic counter: Int = new Atomic<Int>(42);

    old_value: Int =
        counter.fetch_sub(
            1,
            Atomic.ordering.SeqCst
        );
}

// fetch_sub() returns the previous value.


// compare and exchange

fn atomic_compare_exchange() {
    Atomic counter: Int = new Atomic<Int>(42);

    old_value: Int =
        counter.compare_exchange(
            42,
            43,
            Atomic.ordering.SeqCst,
            Atomic.ordering.SeqCst
        );
}

// compare_exchange():
//
// - First argument: expected value.
// - Second argument: replacement value.
// - Third argument: success ordering.
// - Fourth argument: failure ordering.
// - Returns the previous value.


// Bool atomic operations

fn atomic_bool() {
    Atomic flag: Bool = new Atomic<Bool>(false);

    current: Bool =
        flag.load(Atomic.ordering.SeqCst);

    flag.store(
        true,
        Atomic.ordering.SeqCst
    );

    previous: Bool =
        flag.exchange(
            false,
            Atomic.ordering.SeqCst
        );

    old_value: Bool =
        flag.compare_exchange(
            false,
            true,
            Atomic.ordering.SeqCst,
            Atomic.ordering.SeqCst
        );
}

// Atomic<Bool> supports:
//
// - load
// - store
// - exchange
// - compare_exchange
//
// Atomic<Bool> does not support:
//
// - fetch_add
// - fetch_sub


// invalid direct atomic read

fn invalid_direct_atomic_read() {
    Atomic counter: Int = new Atomic<Int>(42);

    value: Int = counter;
}

// Direct atomic reads are compile-time errors.
// load() is required.


// invalid direct atomic assignment

fn invalid_direct_atomic_assignment() {
    Atomic counter: Int = new Atomic<Int>(42);

    counter = 50;
}

// Direct assignment is a compile-time error.
// Atomic mutation must use an atomic operation.


// invalid atomic initial value

fn invalid_runtime_atomic_initialization() {
    value: Int = GetRuntimeValue();

    Atomic counter: Int = new Atomic<Int>(value);
}

// new Atomic<T>() requires a compile-time constant.


// atomic movement

fn move_atomic() {
    Atomic first: Int = new Atomic<Int>(42);

    Atomic second: Int = first;

    // first can no longer be used.
}

// Atomic values are move-only.
//
// Sharing an atomic value between multiple threads requires Arc.


// shared atomic

fn shared_atomic() {
    shared: Arc<Atomic<Int>> =
        new Arc<Atomic<Int>>(new Atomic<Int>(0));
}


// ordering validation

fn invalid_load_ordering() {
    Atomic counter: Int = new Atomic<Int>(42);

    value: Int =
        counter.load(Atomic.ordering.Release);
}

// Release is invalid for load().
// This is a compile-time error.


fn invalid_store_ordering() {
    Atomic counter: Int = new Atomic<Int>(42);

    counter.store(
        50,
        Atomic.ordering.Acquire
    );
}

// Acquire is invalid for store().
// This is a compile-time error.


// compare_exchange failure ordering

// Release and AcqRel may not be used as the failure ordering
// of compare_exchange().
//
// Invalid ordering combinations are compile-time errors.


// unsupported atomic operations

// Atomic does not provide:
//
// - fetch_and
// - fetch_or
// - fetch_xor
// - Other bitwise fetch operations


// ============================================================
// Async tasks
// ============================================================


// async result

async fn get_value() -> Task<Int> {
    return 42;
}


// async unit result

async fn run_unit_task() -> Task<()> {
    return;
}

// Task<()> may use return;.
//
// Returning a value from Task<()> is a compile-time error.


// awaiting

async fn await_value() -> Task<()> {
    value: Int = await get_value();
}

// await:
//
// - May only appear inside async fn.
// - Begins execution of a lazy task.
// - Suspends the current task until completion.
// - Produces the T value from Task<T>.


// lazy execution

async fn lazy_task() -> Task<()> {
    task: Task<Int> = get_value();

    // GetValue has not started yet.

    value: Int = await task;

    // GetValue begins when first awaited.
}


// mandatory await

async fn invalid_unawaited_task() -> Task<()> {
    task: Task<Int> = get_value();
}

// Every created Task<T> must be awaited at least once.
// Leaving a task unawaited is a compile-time error.


// repeated sequential await

async fn repeated_await() -> Task<()> {
    task: Task<Int> = get_value();

    first: Int = await task;
    second: Int = await task;
}

// The same Task<T> may be awaited multiple times sequentially.
// Completed tasks preserve their result.


// concurrent await

// The same Task<T> may not be actively awaited by multiple
// async flows at the same time.
//
// A statically visible concurrent await is a compile-time error.


// task movement

async fn move_task() -> Task<()> {
    first_task: Task<Int> = get_value();
    second_task: Task<Int> = first_task;

    value: Int = await second_task;
}

// Task<T> is move-only.
// It cannot be copied.
//
// After movement, first_task can no longer be used.


// task thread rule

//
// Task<T>: Send if T: Send
// Task<T> is not Sync
//

// A Task<T> may move to another thread when T: Send.
//
// A Task<T> is not shared concurrently between threads.


// async Result handling

async fn async_result_handling() -> Task<Result<()>> {
    value: Int = await FailingTask()@;
    return Ok();
}

// Error raised by an awaited task is transferred through the await
// expression.
//
// A possible error must be handled at the await point or propagated with @.
//
// Leaving a possible async error unhandled is a
// compile-time error.


// async runtime

//
// The async executor is built into the language.
//
// Async tasks run on a built-in thread pool.
//
// The thread pool size is selected automatically.
//
// The user does not manually initialize or configure the
// executor.
//


// ============================================================
// Thread channels
// ============================================================

import thread;


// channel creation and communication

fn channel_example() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    std::thread::spawn(move fn() {
        tx.send(42);
    });

    received: Result<Int, Error> = rx.recv();

    println!("{:?}", received);
}

// std::thread::Channel<T>:
//
// - Creates one sender endpoint and one receiver endpoint.
// - Uses a single-sender, single-receiver model.
// - Sender and receiver endpoints are move-only.
// - Endpoints cannot be cloned.


// sending

fn channel_send_moves_value() {
    (tx, rx): std::thread::Channel<String> =
        std::thread::channel();

    message: String = new String("Alpha");

    tx.send(message);

    // message can no longer be used.
}

// send(value) transfers ownership of value into the channel and returns
// Result<(), Error>.


// receiving

fn channel_receive() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    std::thread::spawn(move fn() {
        tx.send(42);
    });

    received: Result<Int, Error> = rx.recv();
}

// recv():
//
// - Blocks until a value becomes available.
// - Returns Result<T, Error>; Error means that the channel is closed.


// receiver movement

fn move_receiver_to_thread() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    std::thread::spawn(move fn() {
        received: Result<Int, Error> = rx.recv();
    });

    tx.send(42);
}

// rx may move to another thread.


// invalid sender cloning

fn invalid_sender_clone() {
    (tx, rx): std::thread::Channel<Int> =
        std::thread::channel();

    second_sender: std::thread::Channel<Int> =
        std::thread::Channel::clone(&tx);
}

// Channel endpoints cannot be cloned.
// This is a compile-time error.


// ============================================================
// Error
// ============================================================

fn handle_sync_error() {
    mutex: Mutex<Int> = new Mutex<Int>(42);

    result: Result<Mutex<Int>, Error> = mutex.lock();
    if (result.is_error()) {
        println!("sync failure");
    }
}

// Error covers runtime synchronization failures,
// including:
//
// - Poisoned Mutex access
// - Poisoned RwLock access
// - Invalid runtime lock ownership
// - Invalid runtime unlocking
// - Runtime-detected lock reentry
// - Failed Weak upgrade
// - Receiving from a closed channel
//
// Statically provable misuse is rejected during compilation.


// ============================================================
// Shared-memory summary
// ============================================================

//
// T
//     Single ownership and ordinary single-thread use.
//
// Arc<T>
//     Multi-thread shared immutable access.
//
// Arc<Mutex<T>>
//     Multi-thread shared mutable access with one active
//     lock holder.
//
// Arc<RwLock<T>>
//     Multi-thread shared access optimized for many readers
//     and fewer writers.
//
// Arc<Atomic<T>>
//     Multi-thread shared lock-free atomic value access.
//
