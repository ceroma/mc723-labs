#include <stdio.h>
#include <stdlib.h>

#define NUM_PROC 8
#define INITIAL_NUMS 11500

volatile int proc_counter = 0;
volatile unsigned int sum[NUM_PROC];

volatile int arrived = 0, ready = 0;
volatile int *lock_ptr = (volatile int *) 0x600000;

/**
 * Acquire the lock by reading the lock's address. The read will return 0 if the
 * lock was granted.
 */
void acquire_lock() {
  while (*lock_ptr);
}

/**
 * Release the lock by writting the value 0 to the lock's address.
 */
void release_lock() {
  *lock_ptr = 0;
}

/**
 * A simple synchronizing barrier. All process need to reach this point before
 * all of them can resume execution.
 */
void synch() {
  acquire_lock();
  arrived++;
  if (arrived == NUM_PROC) ready = 0;
  release_lock();

  while (arrived < NUM_PROC);

  acquire_lock();
  ready++;
  if (ready == NUM_PROC) arrived = 0;
  release_lock();

  while (ready < NUM_PROC);
}

int main(int argc, char *argv[]){
  int i, pn, half;

  // Get process number for running process
  acquire_lock();
  pn = proc_counter;
  proc_counter++;
  release_lock();

  // Calculate initial sum
  sum[pn] = 0;
  for (i = INITIAL_NUMS * pn; i < INITIAL_NUMS * (pn + 1); i++) {
    sum[pn] = sum[pn] + i;
  }

  half = NUM_PROC;
  do {
    // Synchronizing barrier
    synch();

    // When half is odd, p0 gets missing element
    if (half % 2 != 0 && pn == 0) {
      sum[0] = sum[0] + sum[half-1];
    }

    // Divide and conquer
    half = half / 2;
    if (pn < half) {
      sum[pn] = sum[pn] + sum[pn + half];
    }
  } while (half > 1);

  // p0 has the final result
  if (pn == 0) {
    printf("Result: %u\n", sum[0]);
  }

  exit(0); // To avoid cross-compiler exit routine
  return 0; // Never executed, just for compatibility
}
