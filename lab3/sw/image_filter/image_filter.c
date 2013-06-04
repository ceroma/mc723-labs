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
#define FILTER_TYPE_ADDRESS 0x700024
#define FILTER_RESULT_ADDRESS 0x700028

#define FILTER_TYPE_MEAN  0
#define FILTER_TYPE_SOBEL 1

#define NUM_PROC 8
#define NUM_MALLOC_RETRIES 30
#define MIN(a, b) (a < b ? a : b)

volatile int proc_order = 0;
volatile int proc_counter = 0;

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
 * Apply the selected filter on a 3x3 window.
 */
int apply_filter(int type,
                 int tl, int tc, int tr,
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

  filter_address = (int *) FILTER_TYPE_ADDRESS;
  *filter_address = type;

  filter_address = (int *) FILTER_RESULT_ADDRESS;
  return *filter_address;
}

/**
 * Apply the mean filter on a 3x3 window.
 */
int mean_filter(int tl, int tc, int tr,
                int ml, int mc, int mr,
                int bl, int bc, int br) {
  return apply_filter(FILTER_TYPE_MEAN, tl, tc, tr, ml, mc, mr, bl, bc, br);
}

/**
 * Apply the sobel filter on a 3x3 window.
 */
int sobel_filter(int tl, int tc, int tr,
                int ml, int mc, int mr,
                int bl, int bc, int br) {
  return apply_filter(FILTER_TYPE_SOBEL, tl, tc, tr, ml, mc, mr, bl, bc, br);
}

/**
 * Utility function to map matrix indexes to a flat arary.
 *
 * @param row the row index in matrix[row][column]
 * @param column the column index in matrix[row][column]
 * @param C the total number of columns
 */
int map(int row, int column, int C) {
  return row * C + column;
}

/**
 * Utility function to retry malloc a few times in case of failure.
 *
 * @param size the number of integer positions needed
 */
int *try_malloc(int size) {
  int retries = 0;
  int *mem = NULL;

  while (!mem && retries++ < NUM_MALLOC_RETRIES) {
    mem = (int *)malloc(size * sizeof(int));
  }

  if (!mem) {
    printf("Error: couldn't allocate enough memory!\n");
    exit(1);
  }

  return mem;
}

/**
 * Write the result matrix to file.
 *
 * @param file name of the file to write to
 * @param pn identifier of the running core
 * @param output matrix that will be written to file
 * @param r number of rows in the output matrix
 * @param R number of rows in the original input matrix
 * @param C number of columns in the original input matrix
 */
void write_output(char *file, int pn, int *output, int r, int R, int C) {
  int i, j;
  FILE *fp;

  // First core writes size and a row of zeros
  if (pn == 0) {
    fp = fopen(file, "w");
    fprintf(fp, "%d %d\n", R, C);
    for (i = 0; i < C; i++) {
      fprintf(fp, "0 ");
    }
    fprintf(fp, "\n");
  } else {
    fp = fopen(file, "a");
  }

  for (i = 0; i < r; i++) {
    for (j = 0; j < C; j++) {
      fprintf(fp, "%d ", output[map(i, j, C)]);
    }
    fprintf(fp, "\n");
  }

  // Last core writes a row of zeros
  if (pn == NUM_PROC - 1) {
    for (i = 0; i < C; i++) {
      fprintf(fp, "0 ");
    }
    fprintf(fp, "\n");
  }

  fclose(fp);
}

/**
 * Read r+2 rows of a RxC matrix from file. The number r is the ammount of rows
 * this core will produce, but since an output row i also depends on rows i-1
 * and i+1 of the input matrix, r+2 rows are read.
 *
 * @param file name of the file to read the input from
 * @param pn identifier of the running core
 * @param input will have the values of the (r+2)xC matrix read from file
 * @param r will be set to the number of rows this core will work on
 * @param R will be set to the number of rows in the original input matrix
 * @param C will be set to the number of columns in the original input matrix
 */
void read_input(char *file, int pn, int **input, int *r, int *R, int *C) {
  int i, j, k, first_row;
  FILE *fp = fopen(file, "r");

  // Read matrix size
  fscanf(fp, "%d %d", R, C);
  
  // Calculate matrix offset for this core
  *r = (*R - 2) / NUM_PROC;
  *r += ((*R - 2) % NUM_PROC > pn) ? 1 : 0;
  first_row = 1 + pn * ((*R - 2) / NUM_PROC);
  first_row += MIN(pn, (*R - 2) % NUM_PROC);

  // Go to right offset in file
  for (i = 0; i < first_row - 1; i++) {
    for (j = 0; j < *C; j++) {
      fscanf(fp, "%d", &k);
    }
  }

  // Read input for this core
  *input = try_malloc((*r + 2) * (*C));
  for (i = 0; i < (*r + 2); i++) {
    for (j = 0; j < *C; j++) {
      fscanf(fp, "%d", *input + map(i, j, *C));
    }
  }

  fclose(fp);
}

int main(int argc, char *argv[]){
  int pn, i, j;
  int r, R, C, *input, *output;

  // Sanitize arguments
  if (argc < 3) {
    printf("Usage: ./image_filter input_file output_file\n");
    exit(0);
  }

  // Get process number for running process and read input
  acquire_lock();
  pn = proc_counter;
  proc_counter++;
  read_input(argv[1], pn, &input, &r, &R, &C);
  release_lock();

  // Each core will apply the filter to a subset of rows
  acquire_lock();
  output = try_malloc(r * C);
  memset(output, 0, r * C * sizeof(int));
  release_lock();
  for (i = 1; i <= r; i++) {
    for (j = 1; j < C - 1; j++) {
      acquire_lock();
      output[map(i-1, j, C)] = sobel_filter(
        input[map(i-1, j-1, C)], input[map(i-1, j, C)], input[map(i-1, j+1, C)],
        input[map(i  , j-1, C)], input[map(i  , j, C)], input[map(i  , j+1, C)],
        input[map(i+1, j-1, C)], input[map(i+1, j, C)], input[map(i+1, j+1, C)]
      );
      release_lock();
    }
  }

  // Wait to write output in the correct order
  while (1) {
    acquire_lock();
    i = proc_order;
    release_lock();
    if (i == pn) break;
  }

  // Write output and mark that core has finished
  write_output(argv[2], pn, output, r, R, C);
  acquire_lock();
  proc_order++;
  release_lock();

  // Free, free, free!
  free(input);
  free(output);

  exit(0); // To avoid cross-compiler exit routine
  return 0; // Never executed, just for compatibility
}
