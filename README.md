A collection of systems programming projects built in C, covering process scheduling, parallel I/O, inter-process communication, and core Unix utilities. Each project was implemented from scratch with an emphasis on correctness, concurrency, and low-level memory management.

---

## Projects

### 🐚 `wish` — Unix Shell
A fully functional Unix shell written in C with support for both interactive and batch modes.

**Key Features:**
- Built-in commands: `cd`, `path`, `exit`
- Parallel command execution using `&` operator
- Output redirection via `>`
- POSIX thread support for concurrent command dispatch
- Batch mode: reads and executes commands from a file

**Concepts Demonstrated:** process creation (`fork`/`exec`/`waitpid`), file descriptor manipulation, string parsing, POSIX threads

---

### ⚙️ `project-2-sched` — Process Scheduler Simulation
A CPU scheduler simulator implementing four classic scheduling algorithms with full I/O awareness.

**Scheduling Policies:**
| Policy | Description |
|--------|-------------|
| FIFO | First-In, First-Out — non-preemptive |
| SJF | Shortest Job First — non-preemptive |
| STCF | Shortest Time-to-Completion First — preemptive |
| RR | Round Robin — time-slice based preemption |

**Key Features:**
- Simulates CPU bursts and I/O wait cycles
- Tracks turnaround time, response time, and wait time per process
- Configurable time quantum for Round Robin
- Event-driven simulation loop

**Concepts Demonstrated:** scheduling algorithms, preemption, I/O-aware simulation, performance metrics

---

### ✂️ `psed` — Parallel Stream Editor
A high-performance parallel implementation of `sed`-style stream editing using memory-mapped I/O and a producer-consumer threading model.

**Key Features:**
- Memory-mapped file I/O (`mmap`) for zero-copy reads
- Producer-consumer architecture with a circular queue
- Multi-threaded line processing with POSIX mutex/condition variables
- Supports substitution patterns across large files
- Handles edge cases: partial lines, large buffers, concurrent writes

**Concepts Demonstrated:** `mmap`, producer-consumer synchronization, circular buffers, `pthread_mutex`, `pthread_cond`, race condition prevention

---

### 🔧 Unix Utilities — `wcat` · `wgrep` · `wsed`
Reimplementations of classic Unix text-processing utilities in C.

| Utility | Description |
|---------|-------------|
| `wcat` | Concatenates and prints files to stdout |
| `wgrep` | Searches for a pattern in files or stdin |
| `wsed` | Performs in-place string substitution on a file |

**Concepts Demonstrated:** file I/O, stdin/stdout handling, string manipulation in C, error handling

---

## Technical Stack

- **Language:** C (C99/C11)
- **Concurrency:** POSIX Threads (`pthreads`), mutexes, condition variables
- **Memory:** `mmap`, manual memory management, buffer management
- **OS Primitives:** `fork`, `exec`, `waitpid`, `pipe`, file descriptors
- **Environment:** Linux / Ubuntu, GCC, Makefile

---

## Build & Run

Each project has its own directory with a `Makefile`.

```bash
# Example: build and run wish
cd wish
make
./wish

# Example: build and run psed
cd psed
make
./psed input.txt 's/foo/bar/'
```

---

## Author

**Aldo Martell**
[aldo.martell@outlook.com](mailto:aldo.martell@outlook.com)
