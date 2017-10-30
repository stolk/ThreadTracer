# ThreadTracer

## Summary

Lightweight inline profiler that measures wall-time, cpu-time and premptive context switches for threads.

## Features

ThreadTracer is an inline profiler that is special in the following ways:
* Fully supports multi threaded applications.
* Will never cause your thread to go to sleep because of profiling.
* Will not miss events.
* Will detect if threads were context-switched by scheduler, preemptively or voluntarily.
* Computes duty-cycle for each scope: not just how long it ran, but also how much of that time, it was scheduled on a core.
* Small light weight system, written in C. Just one header and one small implementation file.
* Zero dependencies.

## Limitations
* Doesn't show a live profile, but creates a report after the run, viewable with Google Chrome.
* For 64bit Linux only.
* Currently does not support asynchronous events that start on one thread, and finish on another.

## Usage


```
#include <threadtracer.h>

// Each thread that will be generating profiling events must make itself known to the system.
// If you sign in with threadid -1, the threadid of calling thread will be used.

tt_signin( -1, "mainthread" );

// C Programs need to wrap sections of code with a begin and end macro.

TT_BEGIN( "simulation" );
simulate( dt );
TT_END( "simulation" );

// C++ can also use a scoped tag. Event automatically ends when it goes out of scope.
void draw_all(void)
{
	TT_SCOPE( "draw" );
	draw_world();
	if ( !overlay )
		return;
	draw_info();
}

// When you are done profiling, typically at program end, or earlier, you can generate the profile report.

tt_report( "threadtracer.json" );
```

## Building

For 64bit Linux only.
Just add threadtracer.c to your project, and compile your sources with ```-D_GNU_SOURCE``` flag so that RUSAGE_THREAD support is available for the getrusage() call.

## Viewing the report

Start the Google Chrome browser, and in the URL bar, type ```chrome://tracing``` and then load the genererated threadtracer.json file.

![screenshot](https://pbs.twimg.com/media/DNWrAKnVQAMaSiw.png)

Note that for the highlighted task, the detail view show that the thread got interrupted once preemptively, which causes it to run on a CPU core for only 48% of the time that the task took to complete.

