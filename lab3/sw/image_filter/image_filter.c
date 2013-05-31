#include <stdio.h>
#include <stdlib.h>

#define NUM_PROC 8
#define MATRIX_SIZE 10

volatile int proc_counter = 0;
volatile int proc_finished = 0;

volatile int input[MATRIX_SIZE][MATRIX_SIZE] = {
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
  {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
  {4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
  {1, 2, 3, 4, 5, 6, 7, 8, 9, 0},
  {0, 9, 8, 7, 6, 5, 4, 3, 2, 1},
  {6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
  {7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
  {8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
};

volatile int output[MATRIX_SIZE][MATRIX_SIZE] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

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
 * Apply the mean filter on a 3x3 window.
 */
int mean_filter(int tl, int tc, int tr,
                int ml, int mc, int mr,
                int bl, int bc, int br) {
  return (tl + tc + tr + ml + mc + mr + bl + bc + br) / 9;
}

/**
 * Print the output matrix.
 */
void print_output() {
  int i, j;

  for (i = 0; i < MATRIX_SIZE; i++) {
    for (j = 0; j < MATRIX_SIZE; j++) {
      printf("%d\t", output[i][j]);
    }
    printf("\n");
  }
}

int main(int argc, char *argv[]){
  int j, pn;

  // Get process number for running process
  acquire_lock();
  pn = proc_counter;
  proc_counter++;
  release_lock();

  // Each core will apply the filter to one row
  for (j = 1; j < MATRIX_SIZE - 1; j++) {
    output[pn+1][j] = mean_filter(
      input[pn][j-1],   input[pn][j],   input[pn][j+1],
      input[pn+1][j-1], input[pn+1][j], input[pn+1][j+1],
      input[pn+2][j-1], input[pn+2][j], input[pn+2][j+1]
    );
  }

  // Mark core has finished its work
  acquire_lock();
  proc_finished++;
  release_lock();

  // p0 will wait for everyone and print the result
  while (pn == 0 && proc_finished < NUM_PROC);
  if (pn == 0) print_output();

  exit(0); // To avoid cross-compiler exit routine
  return 0; // Never executed, just for compatibility
}
