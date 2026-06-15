# FOS (FCIS Operating System) 2024 💻⚙️

## Overview
This repository contains the source code for FOS (FCIS Operating System), a 32-bit x86 instructional operating system. Developed as part of the Computer Science coursework at Ain Shams University, this project is designed to demonstrate core OS concepts, memory management, scheduling, and hardware interactions.

## Features & Implementation Details
The kernel is capable of initializing the CPU, handling hardware interrupts, and managing multi-tasking environments. Key features include:

* **Memory Management**:
    * Dynamic allocator for kernel and user heaps utilizing a First-Fit placement strategy.
    * Paging initialization and shared memory management.
    * Page Replacement Algorithms (configured to use FIFO by default).
    * **Custom Modifications:** Includes student-developed page allocator logic (`freePagesList_KH`) to explicitly track and allocate consecutive free pages within the kernel heap limit.
* **Multitasking & Scheduling**:
    * Programmable Interrupt Controller (PIC) setup to enable the clock (IRQ0), keyboard (IRQ1), and COM1 (IRQ4) interrupts.
    * Process context switching and environment queueing.
* **Interactive Shell**:
    * A built-in kernel command prompt used for testing dynamic allocators, page faults, and system commands.

## Toolchain & Prerequisites
To successfully build and run this OS, you must have a 32-bit x86 ELF cross-compiler toolchain installed.

* `i386-elf-gcc` (Targeting 32-bit x86 ELF)
* `i386-elf-ld`, `i386-elf-as`, `i386-elf-objdump`
* **Emulator**: Bochs (`bochs` command line tool)

> **Note on Modern GCC:** If you are porting or compiling this system with newer GCC versions, you may encounter linking issues with inline functions. You may need to use `static inline` instead of `inline`. The Makefile is currently pre-configured with `-fgnu89-inline` and `-O0` to help manage these backward compatibility problems.

## Build & Run Instructions
The build system relies on standard Makefiles designed to compile the kernel, bootloader, and user-space binaries.

1. **Compile the OS image:**
   ```bash
   make all
