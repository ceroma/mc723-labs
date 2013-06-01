#include <stdio.h>
#include <stdlib.h>

#define FILTER_TL_ADDRESS 0x700000
#define FILTER_TC_ADDRESS 0x700004
#define FILTER_TR_ADDRESS 0x700008
#define FILTER_ML_ADDRESS 0x70000C
#define FILTER_MC_ADDRESS 0x700010
#define FILTER_MR_ADDRESS 0x700014
#define FILTER_BL_ADDRESS 0x700018
#define FILTER_BC_ADDRESS 0x70001C
#define FILTER_BR_ADDRESS 0x700020
#define FILTER_RESULT_ADDRESS 0x700024

#define NUM_PROC 8
#define MATRIX_SIZE 15
#define MIN(a, b) (a < b ? a : b)

volatile int proc_counter = 0;
volatile int proc_finished = 0;

volatile int input[MATRIX_SIZE][MATRIX_SIZE] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

volatile int output[MATRIX_SIZE][MATRIX_SIZE] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
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
  int *filter_address;

  filter_address = (int *) FILTER_TL_ADDRESS;
  *filter_address = tl;

  filter_address = (int *) FILTER_TC_ADDRESS;
  *filter_address = tc;

  filter_address = (int *) FILTER_TR_ADDRESS;
  *filter_address = tr;

  filter_address = (int *) FILTER_ML_ADDRESS;
  *filter_address = ml;

  filter_address = (int *) FILTER_MC_ADDRESS;
  *filter_address = mc;

  filter_address = (int *) FILTER_MR_ADDRESS;
  *filter_address = mr;

  filter_address = (int *) FILTER_BL_ADDRESS;
  *filter_address = bl;

  filter_address = (int *) FILTER_BC_ADDRESS;
  *filter_address = bc;

  filter_address = (int *) FILTER_BR_ADDRESS;
  *filter_address = br;

  filter_address = (int *) FILTER_RESULT_ADDRESS;
  return *filter_address;
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
  int i, i_max, num_lines;

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
  
  // Calculate matrix offset for this core
  num_lines = (MATRIX_SIZE - 2) / NUM_PROC;
  num_lines += ((MATRIX_SIZE - 2) % NUM_PROC > pn) ? 1 : 0;
  i = 1 + pn * ((MATRIX_SIZE - 2) / NUM_PROC);
  i += MIN(pn, (MATRIX_SIZE - 2) % NUM_PROC);
  i_max = i + num_lines;

  // Each core will apply the filter to one row
  for (i = i; i < i_max; i++) {
    for (j = 1; j < MATRIX_SIZE - 1; j++) {
      acquire_lock();
      output[i][j] = mean_filter(
        input[i-1][j-1], input[i-1][j], input[i-1][j+1],
        input[i  ][j-1], input[i  ][j], input[i  ][j+1],
        input[i+1][j-1], input[i+1][j], input[i+1][j+1]
      );
      release_lock();
    }
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
