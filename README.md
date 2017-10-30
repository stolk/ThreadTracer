# ThreadTracer

## Summary

Lightweight inline profiler that measures wall-time, cpu-time and premptive context switches for threads.

## Features

ThreadTracer is an inline profiler that is special in the following ways:
* Fully supports multi threaded applications.
* Will never cause your thread to go to sleep because of profiling.
* Will not miss events.
* Will detect if threads were context-switched by scheduler, preemptively or voluntarily.
* Computes duty-cycle for each scope: not just how long it ran, but also how much of that time, it was running on a core.
* Small light weight system, written in C. Just one header and one small implementation file.

## Limitations
* Doesn't show a live profile, but creates a report after the run, viewable with Google Chrome.
* For 64bit Linux only.
* Currently does not support asynchronous events that start on one thread, and finish on another.

