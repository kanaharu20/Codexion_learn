*This project has been created as part of the 42 curriculum by hkanamit.*

# Codexion

## Description

Codexion is a concurrency simulation written in C with POSIX threads.

A number of coders sit around a circular co-working hub. Between every pair of
neighbours lies one USB dongle, so there are exactly as many dongles as coders.
A coder must hold **both** the dongle on their left and the one on their right to
compile. After compiling they release both dongles, debug, refactor, and
immediately try to compile again.

Two clocks run against them:

* **Burnout** — if a coder does not *start* a new compile within
  `time_to_burnout` milliseconds of the start of their previous compile (or of
  the start of the simulation), they burn out and the simulation stops.
* **Cooldown** — a released dongle stays unusable for `dongle_cooldown`
  milliseconds before anyone may take it again.

Each coder is a thread. Each dongle is a small state machine
(`D_FREE` / `D_TAKEN` / `D_COOLDOWN`) guarded by its own mutex and condition
variable, with a per-dongle priority queue that arbitrates between the two
coders that can possibly want it. A coder never holds one dongle while waiting
for the other: the pair is taken all-or-nothing. A separate monitor thread
watches every coder's deadline and stops the simulation the moment one is
missed.

The simulation ends when either

* a coder burns out (`<timestamp> <id> burned out` is printed), or
* every coder has compiled at least `number_of_compiles_required` times.

### Log format

```
<timestamp_in_ms> <coder_id> has taken a dongle
<timestamp_in_ms> <coder_id> is compiling
<timestamp_in_ms> <coder_id> is debugging
<timestamp_in_ms> <coder_id> is refactoring
<timestamp_in_ms> <coder_id> burned out
```

Timestamps are milliseconds elapsed since the start of the simulation.

The two `has taken a dongle` lines of one compile usually share a timestamp.
`try_take_pair` claims both dongles in a single critical section or neither
(see *Deadlock*), so there is no instant at which the coder holds only one.

## Instructions

### Build

```sh
make          # builds ./codexion
make clean    # removes obj/
make fclean   # removes obj/ and the binary
make re       # fclean + all
```

Compiled with `cc -Wall -Wextra -Werror -pthread`. Sources live in `src/`, the
single header in `incl/`, objects are produced into `obj/`.

### Run

```sh
./codexion number_of_coders time_to_burnout time_to_compile time_to_debug \
           time_to_refactor number_of_compiles_required dongle_cooldown scheduler
```

| Argument | Meaning |
|---|---|
| `number_of_coders` | number of coders, and of dongles (≥ 1) |
| `time_to_burnout` | ms; deadline measured from the start of the last compile |
| `time_to_compile` | ms spent compiling, holding two dongles |
| `time_to_debug` | ms spent debugging |
| `time_to_refactor` | ms spent refactoring |
| `number_of_compiles_required` | stop once every coder reached this count (≥ 1) |
| `dongle_cooldown` | ms a dongle stays unavailable after release |
| `scheduler` | exactly `fifo` or `edf` |

All eight arguments are mandatory. Each numeric argument must be a string of
decimal digits only, so negative numbers and non-integers are rejected before
parsing; the conversion also refuses values that would overflow `int`. Anything
invalid prints `Error` on stderr and exits with status 1.

Examples:

```sh
# feasible parameters: every coder reaches the required number of compiles
./codexion 5 800 200 200 0 10 0 fifo
./codexion 5 800 200 200 0 10 0 edf
./codexion 4 410 200 100 100 10 0 edf
./codexion 5 610 200 200 0 10 0 fifo

# not feasible: in a ring only floor(n / 2) coders can compile at once, and a
# dongle is usable once per (time_to_compile + dongle_cooldown), so giving every
# coder a turn takes ceil(n / floor(n / 2)) of those periods. time_to_burnout
# must exceed that. Here it is ceil(5 / 2) * 200 = 600 ms, and 460 < 600, so
# someone always burns out, whichever scheduler is used
./codexion 5 460 200 200 0 20 0 edf

# one coder, one dongle: the second dongle can never be taken,
# so this always ends in a burnout at time_to_burnout
./codexion 1 800 200 200 100 5 0 fifo
```

## Resources

* Blaise Barney, [*POSIX Threads Programming*](https://hpc-tutorials.llnl.gov/posix/)
  (Lawrence Livermore National Laboratory, UCRL-MI-133316) — the reference used
  for the Pthreads API itself: thread creation and joining, mutex variables and,
  most importantly for this project, the condition-variable chapter and its rule
  that a wait must always sit inside a `while` loop re-testing the predicate.
* Sachin Adlakha, [*POSIX Threads in Linux: A Complete Implementation
  Guide*](https://medium.com/@sachinadlakha7/posix-threads-in-linux-a-complete-implementation-guide-0ca9de562c45)
  — a worked walkthrough of the same primitives in complete programs, used to
  check the shape of the coder threads and of the monitor loop against a
  known-good implementation.

### How AI was used

AI (Claude) was used as a study aid and as a reviewer, not as a code generator:

* **Learning the primitives.** Before writing any of this project, small
  throwaway programs were built with AI assistance to see `pthread_create` /
  `pthread_join`, mutexes, condition variables and `pthread_cond_timedwait`
  working in isolation, together with notes on `gettimeofday` vs
  `clock_gettime` and on the unit conversions `pthread_cond_timedwait` needs
  (`struct timeval` in microseconds vs `struct timespec` in nanoseconds).
  Those experiments are not part of this repository.
* **Reviewing invariants.** AI was used to question the lock ordering, the
  placement of the stop check inside `log_lock`, the microsecond-vs-millisecond
  rounding in the monitor, and the arbitration rule in `can_be_passed_over`.
  Every conclusion was re-derived, measured against the running program and
  verified in the code before being applied.
* **Measuring.** The burn-in runs quoted under "Verification" below were driven
  by AI-written shell loops; the program itself, and every line under `src/`
  and `incl/`, was written and is fully understood by the author.

## Blocking cases handled

### Deadlock (Coffman's four conditions)

A dongle is a genuinely exclusive resource that must be held while compiling,
so *mutual exclusion* cannot be removed, and *no preemption* is kept as well: a
dongle is never taken back from a coder that holds it. The design attacks the
other two conditions.

**Hold and wait** is removed by `try_take_pair` in `src/dongle_pair.c`. A coder
never takes one dongle and then blocks on the other. It locks both dongles (in
ascending id order), and either both are available — in which case both are
marked `D_TAKEN` in the same critical section — or neither is taken and the
function returns the dongle that was missing, having released every lock it
took. `acquire_two_dongles` then calls `wait_for_dongle` on that dongle and
retries. `wait_for_dongle` re-takes that one lock, re-tests the same predicate,
and only sleeps if it still fails, so the test and the wait sit in one critical
section and no broadcast can slip between them.

This is not only a deadlock argument, it is what makes the simulation keep up
with its deadlines. If coders hold one dongle while waiting, every coder in the
ring ends up holding one, all dongles are held and only a single coder can
compile at a time. The number of coders that can compile simultaneously drops
from `n / 2` to one, and parameters that are comfortably feasible on paper start
producing burnouts.

**Circular wait** is removed by `order_by_id` in `src/dongle_acquire.c`: the two
dongle mutexes are always locked lowest id first, so the "A holds 1 waits for 2,
B holds 2 waits for 1" cycle cannot form between threads either.

### Starvation

Each dongle owns a priority queue (`t_heap`) of the coders waiting for it. A
dongle sits between exactly two neighbouring coders and no one else can ever
request it, so the waiter set is provably bounded at two entries. The priority
queue is therefore a fixed-size two-element array rather than a dynamically
sized binary heap: `heap_push` restores the ordering invariant with a single
comparison and swap in constant time, and `heap_top`, `heap_pop` and
`heap_remove` preserve it. It is a hand-written priority queue with the same
contract as a heap, sized to the only input it can ever receive.

A coder enqueues itself on **both** of its dongles for the whole attempt, so
whichever dongle frees first, its own request is already there to be compared
against the neighbour's.

Arbitration follows the `scheduler` argument. Both keys are expressed in
microseconds since the start of the simulation, so the two schedulers differ
only in what the number means:

* **`fifo`** — the key is the arrival time of the request, so requests are
  served in arrival order.
* **`edf`** — the key is `last_compile_start + time_to_burnout`, i.e. the
  coder's burnout deadline, so the coder closest to burning out is served first.

The key is sampled **once** per compile attempt in `snapshot_priority_key` and
reused for both dongles, so every dongle sees the same ordering and a coder
cannot lose its place between the first and the second acquisition.

**Tie-breaker.** `beats` in `src/heap.c` compares keys first and falls back to
the smaller `coder_id` when they are equal. Insertion order would not do: it
depends on which thread happened to reach the queue first, so two runs of the
same command could arbitrate differently. Ties are not a corner case under
`edf` — every coder starts with `last_compile_start = 0`, so at the beginning of
the simulation all deadlines are exactly equal.

**Passing over the head of the queue.** Strict "the head of the queue always
wins" is not enough on a ring, because the head may be a coder that is blocked
on its *other* dongle. It cannot use the dongle it is reserving, its neighbour
cannot have it either, and that chains around the hub until the whole
simulation runs one coder at a time. `can_be_passed_over`
(`src/coder_state.c`) allows a lower-priority coder to take a dongle over the
head of the queue only when **both** of these hold:

1. the head is currently blocked on a *different* dongle, so it cannot use this
   one right now, and
2. the head still has more slack before its own burnout deadline than the
   pass-over would cost it.

Condition 2 is what keeps the arbitration honest: as soon as a coder gets close
to its deadline, it can no longer be passed over by anyone, and it is served
strictly by priority. Condition 1 is what keeps the hub busy while everyone
still has room to spare.

The cost in condition 2 is computed by `pass_over_cost_us`
(`src/coder_state.c`):

    cost = t_to_compile + dongle_cooldown

That is exactly how long the dongle stays out of the head's reach. The passer
holds it for `t_to_compile`, releases it, and the mandatory cooldown runs before
anyone may take it again; only then can the head have it. Nothing else in the
simulation lengthens that gap, so nothing else belongs in the estimate.

The head's own `t_to_debug + t_to_refactor` is deliberately **not** in the cost.
While it is being passed over the head is blocked waiting for a dongle, not
debugging and not refactoring, so those numbers describe time it is not
spending. `dongle_cooldown` on the other hand matters a great deal: at
`5 1600 100 20 20 5 300` the dongle is gone for 400 ms after each release, four
times the compile time alone.

**This is a deliberate deviation from the literal wording of the subject**, and
the one place where this implementation does not follow it to the letter. The
subject defines `fifo` as serving "the coder whose request arrived first", but
it also requires that "no coder should be starved of dongles and burn out under
edf scheduling, provided the parameters are feasible". On a ring those two
demands conflict, and this implementation keeps the liveness guarantee, because
a burnout is an observable failure while a reordered grant is not. The conflict
is measured, not assumed: rebuilding with `can_be_passed_over` replaced by
`return (0)` -- strict order, everything else unchanged -- at the feasible set
`5 620 200 200 0 20 0 edf` gives **18 of 20** runs with a burnout, against
**0 of 20** for this design. The rule applies under both schedulers; the head
keeps its place in the queue and is served as soon as its own blocker frees.

Passing over is a **starvation** mechanism, not a deadlock mechanism: the
disabled build above starves coders into burning out, it never hangs. Deadlock
freedom comes from the previous section, and independently from the fact that
`(priority_key, coder_id)` is a total order -- the coder with the globally
smallest key heads *both* of its queues at once, so a circular wait would
require `key1 < key2 < ... < keyN < key1`.

`mark_waiters_unblocked` (`src/coder_state.c`) closes the gap between the two: the
moment a dongle is released, every coder that was waiting on it is marked as no
longer blocked, so its claim on its *other* dongle becomes untouchable in the
same critical section in which the dongle is freed, and not a wake-up later.

### Cooldown

`release_one_dongle` sets the state to `D_COOLDOWN`, records `release_time`, and
broadcasts. A waiter that sees `D_COOLDOWN` does not spin: it computes the exact
expiry with `cooldown_deadline` and sleeps on `pthread_cond_timedwait` until
that absolute time, then calls `refresh_dongle_state`, which flips `D_COOLDOWN`
back to `D_FREE` once the deadline has passed. The state is refreshed lazily,
always under `d->lock`, so no separate timer thread is needed.

### Precise burnout detection

`monitor_thread` polls every coder every 1 ms. The comparison is done in
**microseconds**, not milliseconds: rounding the deadline down to milliseconds
would move it up to 1 ms earlier and cause a false burnout. Reading
`last_compile_start_us` and `compile_count` is done under the coder's
`state_lock`, so the monitor never observes a half-updated coder.

A coder that has already reached `number_of_compiles_required` is excluded from
the deadline check, because it will never start another compile and would
otherwise be reported as burnt out.

### Sleeping accurately

`usleep(ms * 1000)` returns late by however long the scheduler feels like, and
that error is charged directly against `time_to_burnout`. `precise_sleep`
(`src/timing.c`) instead sleeps in short slices and re-reads the clock, dropping
to 50 µs slices for the last millisecond, so a phase ends within about a
millisecond of its nominal length however loaded the machine is. Each slice also
re-tests `is_stopped`, so a phase in progress is abandoned as soon as the
simulation ends rather than running to completion (see *Shutdown*).

### Log serialization and ordering

Every state line is printed while holding `log_lock`, so two messages can never
interleave. Two details matter beyond that:

* The **timestamp is read inside** `log_lock`, not before it. Reading the clock
  outside the lock lets a thread be overtaken between reading the time and
  printing it, which prints lines out of chronological order — visible with a
  few hundred coders.
* `log_state` re-checks the stop flag **inside** `log_lock`:

```c
pthread_mutex_lock(&shared->log_lock);
if (!is_stopped(shared))
{
    ts = elapsed_ms(shared);
    printf("%ld %d %s\n", ts, coder_id, msg);
}
pthread_mutex_unlock(&shared->log_lock);
```

Checking before taking the lock would leave a window in which a coder passes the
check, the monitor prints `burned out`, and the coder then prints a state line
after it. Doing the check inside the same critical section makes `burned out`
the last line of the output.

### Stopping on the last compile

The simulation is over as soon as every coder has compiled
`number_of_compiles_required` times, so `coder_thread` breaks out of its loop
right after that compile instead of running one more debug and refactor phase.
Otherwise the program keeps printing state lines after its own stop condition
has been met, and takes an extra `time_to_debug + time_to_refactor` to exit.

### Single coder

With one coder, `left == right`: only one dongle exists on the table and the
second acquisition can never succeed. This is handled explicitly rather than
deadlocking — `acquire_two_dongles` detects `second == first` and hands over to
`acquire_single`, which takes the one dongle, logs it, and parks in
`wait_until_stopped` until the monitor detects the inevitable burnout and wakes
it. The program then terminates normally.

### Shutdown

`set_stopped` alone is not enough: coders blocked in `pthread_cond_wait` would
never re-evaluate it. `wake_all_dongles` broadcasts on every dongle's condition
variable after the flag is set, so every sleeping coder wakes, sees
`is_stopped`, removes itself from both waiter queues, releases whatever it
holds, and returns. `main` can then join every thread and free all memory.

The same pair (`set_stopped` + `wake_all_dongles`) is used on the error paths in
`run` and `spawn_coders`, so a failed `pthread_create` also unwinds cleanly
instead of leaving threads blocked forever.

A broadcast only reaches coders that are *blocked*; one sleeping through a
phase would not see the flag until that phase ended, keeping the process alive
for up to `time_to_compile + time_to_debug + time_to_refactor` after the burnout
line was printed. Two checks close that window: `precise_sleep` re-tests
`is_stopped` on every slice, and `acquire_two_dongles` tests it before doing any
work. On `3 250 100 100 3000 5 0 fifo` this took the exit from 3438 ms down to
279 ms, with the burnout line at 250 ms either way.

## Thread synchronization mechanisms

### Threads

* `number_of_coders` coder threads (`coder_thread`), one per coder.
* One monitor thread (`monitor_thread`).
* The main thread creates them, joins the monitor first, then joins the coders,
  and finally destroys every mutex/condition variable and frees the two arrays.

There are no global variables. Everything shared lives in a single `t_shared`
allocated on `main`'s stack and reached through `coder->shared` and
`dongle->shared`.

### Primitives

| Primitive | Where | Protects |
|---|---|---|
| `pthread_mutex_t lock` | one per `t_dongle` | dongle state, `release_time`, waiter queue |
| `pthread_cond_t cond` | one per `t_dongle` | waiting for the dongle to become free or its cooldown to expire |
| `pthread_mutex_t state_lock` | one per `t_coder` | `last_compile_start_us`, `compile_count`, `blocked_on` |
| `pthread_mutex_t log_lock` | one, in `t_shared` | stdout, the timestamp, and the stop check that guards them |
| `pthread_mutex_t stop_lock` | one, in `t_shared` | the `stopped` flag |

### The custom event: the stop flag

The project needs a one-shot "the simulation is over" event that any thread can
observe. It is built from a plain `int stopped` plus `stop_lock`, exposed only
through `is_stopped` / `set_stopped`, so the flag is never read or written
outside its mutex — there is no unsynchronized shared variable anywhere in the
program.

Because a flag on its own cannot wake a blocked thread, it is paired with
`wake_all_dongles`, which broadcasts on every dongle condition variable. Setting
the flag and broadcasting is the "signal" half of the event; the `while` loops
in `acquire_two_dongles`, `acquire_single` and `wait_until_stopped` are the
"wait" half.

### Lock ordering

Three rules, and no path in the program breaks them:

```
1. dongle locks are taken in ascending dongle id  (f -> s)
2. a dongle lock may be taken before a coder's state_lock, never the reverse
3. stop_lock is innermost: it is never held while acquiring anything else
```

Rule 1 makes the two-dongle acquisition free of circular wait. Rule 2 lets
`try_take_pair` ask a neighbour whether it may be passed over, and lets
`release_one_dongle` clear the reservations of the coders queued on it, both
while holding the dongle. Rule 3 makes `is_stopped` safe to call from inside a
dongle critical section (`acquire_two_dongles`, `wait_until_stopped`) and from
inside the log critical section (`log_state`). `log_lock` and the dongle locks
are never held at the same time: every `log_state` call is made after the dongle
locks have been dropped.

### Preventing race conditions

* **Duplicated dongles.** The transition to `D_TAKEN` for *both* dongles and the
  removal from both waiter queues happen while both `d->lock` are held, in the
  same critical section as the test that allowed them. Two coders can never both
  conclude that the same dongle is free.
* **Spurious wakeups.** No wait is trusted to mean the predicate now holds. A
  wakeup only returns control to the `while` loop in `acquire_two_dongles`,
  which calls `try_take_pair` again and re-tests everything from scratch; a
  spurious or broadcast-induced wakeup therefore just costs one more attempt.
* **Torn coder state.** `last_compile_start_us` and `compile_count` are written
  by the coder and read by the monitor; `blocked_on` is written by its own coder
  and read by the neighbour that wants to pass it over. All three are only ever
  accessed under `state_lock`, including the coder's own loop condition
  (`is_done`), and the monitor copies what it needs and releases the lock before
  comparing, so it never holds `state_lock` while blocking.
* **Interleaved or late output.** See "Log serialization and ordering" above.

### Coder ↔ monitor communication

The two directions are deliberately asymmetric and both are one-way:

* **Coder → monitor:** the coder publishes `last_compile_start_us` and
  `compile_count` under `state_lock`. It never notifies the monitor; the monitor
  polls at 1 ms, which is what keeps burnout detection within the required
  10 ms.
* **Monitor → coders:** the monitor sets the stop flag, prints the burnout line,
  and broadcasts on every dongle. Coders observe the flag either at the top of a
  wait loop or right after releasing their dongles, and exit.

Neither side ever blocks waiting for the other, so the monitor can always meet
its deadline, and a coder can always terminate.

## Verification

Current results, on macOS 26.6 (arm64, Apple clang):

* `norminette 3.3.60` — 20 of 20 files OK, 0 errors.
* `cc -Wall -Wextra -Werror -pthread` — no warnings; `make && make` does not
  relink.
* `cc -fsanitize=thread` — 0 warnings across the completion path, cooldown +
  `edf`, a single coder, and the burnout path.
* `leaks --atExit` — 0 leaks on the completion, burnout and early-shutdown
  paths.
* Burnout latency: `2 200 1000 0 0 5 0 fifo` prints `burned out` at 200–201 ms
  over five runs, `1 500 100 100 100 5 0 fifo` at 500–501 ms — inside the 10 ms
  tolerance.
* Log invariants, checked mechanically over 11 configurations up to 200 coders:
  every `is compiling` is preceded by exactly two `has taken a dongle` lines for
  that coder, timestamps never go backwards, at most one `burned out` line with
  nothing after it, and on the completion path every coder reaches the required
  number of compiles.
* Liveness: 0 burnouts over 20 runs of `5 620 200 200 0 20 0`, 15 runs of
  `5 1600 100 20 20 5 300` and 20 runs of `5 700 200 100 100 10 0`, under both
  schedulers.
* Infeasible parameters still burn out on time: `1 800 …` at 800 ms,
  `5 460 200 200 0` at 460 ms, `4 310 200 100 100` at 310 ms.
* Argument validation: negatives, non-digits, decimals, a leading `+`, the empty
  string, values above `INT_MAX`, `0` for either count, an unknown or
  wrong-cased scheduler, and too few or too many arguments are all rejected with
  exit status 1.

An earlier revision was also checked under `valgrind --leak-check=full` and
`valgrind --tool=helgrind` on a 16-core Linux machine, both clean; those two
have not been re-run since the most recent changes.
