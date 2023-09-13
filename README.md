# igasync - C++ Promise library focused on browser WebAssembly

**Current version: 0.2**

Changes from version 0.1:
- Specialization for void promises
- TaskList can return a promise for a task it invokes
- Promises consume a scheduler, and by default execute callbacks on the same thread as the resolving (or scheduling) thread
- Upgraded to C++20, and introduced concept guards and better template type deduction

> :warning: This project is under development, and not production-hardened

<!-- TOC -->
- [Overview](#overview)
  - [Motivation](#motivation)
  - [Concepts](#concepts)
- [WebAssembly Considerations](#webassembly-considerations)
- [Samples](#samples)
- [Thank you!](#thank-you)

## Overview

igasync is a C++20 library that defines a `Promise` class strongly motivated from the [JavaScript Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise), but designed with C++ applications in mind.

Supporting objects TaskList, PromiseCombiner, and ThreadPool are also included.

### Motivation

Writing C++ code for browser WebAssembly applications brings constraints that don't fit well with classic C++ asynchronous programming models - read the Emscripten [pthreads documentation](https://emscripten.org/docs/porting/pthreads.html) for more information:

* An application should work if threads aren't available (i.e. that `std::thread` doesn't exist)
  * A good browser WASM application provides builds with and without `std::thread`
  * The [large majority of users](https://caniuse.com/sharedarraybuffer) can use threads
* Synchronization primitives on the main thread are implemented as busy waits
  * Mutex locks, thread joins, condition variable waits
  * This is bad for performance, energy usage, and hardware utilization
  * Loosely contested mutexes are usually fine
  * Thread joins, condvar waits, future gets must be avoided
* Web IO is non-blocking and callback-based
  * Example: web version of `std::fstream` read is the [Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API/Using_Fetch), which is promise-based
  * Same goes for thread sleep (setTimeout), graphics API vsync blocking calls (requestAnimationFrame)

A good asynchronous programming model for the web includes the following features:

* Applications may be built with or without multithreading support
* Do not use blocking thread joins / future waits on the main thread

I have been using this library in my multi-threaded WebAssembly applications and have been happy with it, and of course I'd be thrilled if other people get good usage out of it.

### Concepts

#### Task Lists

`igasync` promises use the `igasync::TaskList` object to handle scheduling - these task lists are just dumb lists of tasks, but application developers may create task lists that carry logical meaning.

For simple game code, I tend to create a task list for main-thread tasks, and another for off-thread tasks. I pass the off-thread task list to a thread pool, and execute main-thread tasks in between frames.

I've also created task lists that have much more focused scope, such as:
* Main thread tasks that must be finished before the frame ends
* Any-thread tasks that must be finished this frame
* Tasks that will take a very long time to run, and are not urgent

The scope, quantity, and lifetimes of task lists are entirely up to application developers, though the general assumption of `igasync` objects is that once a task is added to a `igasync::TaskList`, it will eventually be executed.

#### Promises

`igasync::Promise` is strongly inspired from the [JavaScript Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise) type, but it uses C++ idioms instead of attempting to live by the JavaScript API. It addresses the common case of some background task producing data that should be consumed via zero or more callbacks that are interested in the result, either on the application or a background thread.

A common example of using `igasync::Promise`:

```c++
shared_ptr<Promise<Geometry>> geometry_promise = async_generate_geo(async_tasks);
geometry_promise->on_resolve(
  /* callback= */ [](const Geometry& geo) { set_geometry_data(geo); },
  /* task_list= */ main_thread_tasks);
```

A promise has one of three states:
* `Empty`: The promise has been created, but no data has been added.
* `Resolved`: The promise is holding data
* `Consumed`: The promise has been destroyed and will never again hold data

Promises are created in the `Empty` state, and an application developer calls `resolve(data)` to place it in the `Resolved` state.

Callbacks can be registered against a promise that is in the `Empty` or `Resolved` state, with `on_success(callback, task_list)`. Registered callbacks will be scheduled when the promise is resolved, or immediately if the promise is already resolved.

Normal callbacks pass a reference to the held promise data, but a consuming callback may also be registered with the `consume(callback, task_list)` method. Once a consuming callback is added, no additional callbacks may be registered. The data held by the promise will be sent to the callback with move semantics, and the promise will enter the `Consumed` state. **For safety, the promise is considered `Consumed` as soon as a consuming callback is registered.**

> Notice: keep promise callback registration as simple as possible, and avoid using `consume` unless there's a good reason to move ownership of the held data out of the promise (e.g. moving large 3D model geometry data to the GPU from a promise who's only job was to generate the geometry data on a background thread).

Promises can also be chained together, or combined via `igasync::PromiseCombiner`.

## WebAssembly Considerations

To fit the constraints of a possibly single-threaded platform that hates blocking the main thread, I've found the following advice to be helpful:

### Make task lists that run in thread pools, but can also run on the main thread

Create async task lists and have logic where you pull tasks from them on the main thread, especially if you know this is a single-threaded build.

Executing tasks until some deadline is reached is a good idea. Games often have CPU downtime while the GPU works and/or while waiting for monitor vsync, that's a great time to chip away at async tasks.

### Replace fork/join with schedule/execute_until

Some tasks must unavoidably be synchronously joined - for example, a game may wish to calculate animation state for 8 actors across whichever cores are free to take the work, but must unavoidably wait for them all to finish before yielding control back to the game process to render the frame.

```c++
// schedule tasks and add them to PromiseCombiner (instead of "fork")
auto frame_combiner = PromiseCombiner::Create();
auto animation_key =
    frame_combiner->add(update_all_animations_async(frame_async_task_list));
auto particles_key =
    frame_combiner->add(update_all_particles_async(frame_async_task_list));

bool is_finished = false;
frame_combiner->combine(
    [&is_finished](const auto&) { is_finished = true; },
    main_thread_task_list);

// Execute frame tasks until the combiner resolves
while (!is_finished) {
  main_thread_task_list->execute_task();
  frame_async_task_list->execute_task();
}

// NOTICE: if all task lists are empty and a worker thread is currently taking
//  care some task that will finally trigger the frame_combiner, the above while
//  loop is an inefficient busy-wait - buuuut that's what thread.join() and any
//  sort of blocking synchronization is from the main thread in browser WASM
//  code anyways, so it's fine.
// If main thread synchronization primitives are ever implemented, a more clever
//  solution will be in order.
```

## Samples

- [sample-read-file](samples/read-file): Interface with file system API via `std::ifstream` for native builds, and JavaScript `fetch` for web builds

To run samples natively, simply build the appropriate target. Make sure `IGASYNC_BUILD_EXAMPLES` is set.

### Running web targets

I use [emscripten](https://emscripten.org/) to build and run tests and samples. Download and install it there.

To set up a project binary directory with Emscripten, run the following:

```
mkdir out/web
cd out/web
emcmake cmake ../..
```

To build a sample (or unit tests with target `igasync_test` instead of `sample-read-file`), run the following

```
emmake make sample-read-file
```

To run the samples in a web browser though, you'll need to use the `simple_server.js` tool provided in order to
set the headers required to run multi-threaded code in a web environment.

> :warning: Threads are not guaranteed to be usable in all web browsers - some samples may simply not work because most binaries in this project are built under the assumption that threads are supported.

```
node ./simple_server.js
```

Navigate to `https://localhost:8000/` from your binary directory, and from there you can select HTML files, or navigate through
the output binary directory to find the appropriate samples.

## Thank you!

Open source projects used in this library:

* [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue) - thread-safe lock-free queue implementation that powers `igasync::TaskList`