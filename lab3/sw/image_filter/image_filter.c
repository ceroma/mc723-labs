#include <stdio.h>
#include <stdlib.h>

#define NUM_PROC 8
#define MATRIX_SIZE 10

volatile int proc_counter = 0;
volatile int proc_finished = 0;

volatile int input[MATRIX_SIZE][MATRIX_SIZE] = {
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

/**
 * Apply the mean filter on a 3x3 window.
 */
int mean_filter(int tl, int tc, int tr,
                int ml, int mc, int mr,
                int bl, int bc, int br) {
  return (tl + tc + tr + ml + mc + mr + bl + bc + br) / 9;
}

/**
 * Write the result matrix to file.
 */
void write_output(const char *file) {
  int i, j;
  FILE *fp = fopen(file, "w");

  for (i = 0; i < MATRIX_SIZE; i++) {
    for (j = 0; j < MATRIX_SIZE; j++) {
      fprintf(fp, "%d ", output[i][j]);
    }
    fprintf(fp, "\n");
  }

  fclose(fp);
}

/**
 * Read a 10x10 matrix from file.
 */
void read_input(const char *file) {
  int i, j;
  FILE *fp = fopen(file, "r");

  for (i = 0; i < MATRIX_SIZE; i++) {
    for (j = 0; j < MATRIX_SIZE; j++) {
      fscanf(fp, "%d", &input[i][j]);
    }
  }

  fclose(fp);
}

int main(int argc, char *argv[]){
  int j, pn;

  // Sanitize arguments
  if (argc < 3) {
    printf("Usage: ./image_filter input_file output_file\n");
    exit(0);
  }

  // Get process number for running process
  acquire_lock();
  pn = proc_counter;
  proc_counter++;
  release_lock();

  // p0 will read the input file
  if (pn == 0) {
    read_input(argv[1]);
  }

  // Make sure input was read before cores start working
  synch();
  
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
  if (pn == 0) {
    write_output(argv[2]);
  }

  exit(0); // To avoid cross-compiler exit routine
  return 0; // Never executed, just for compatibility
}
