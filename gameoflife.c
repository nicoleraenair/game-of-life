/*
 * This program implements Conway's Game of Life in the command line.
 * Usage: ./gol infile.txt
 */

#define _XOPEN_SOURCE 600 /* Or higher */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "pthread_barrier.h"
#include <math.h>

#define NUM_THREADS 12 //number of threads to be spawned
#define PARTITION 0 //0 = row-wise partitioning, 1 = column-wise partitioning
#define OUTPUT_MODE 1 //0 = no board printer, 1 = board printed

static int total_live = 0;
pthread_barrier_t barrier1, barrier2;
pthread_mutex_t mutex;

/* struct representing game of life state and relevant attributes */
struct gol_data {
   int rows;
   int cols;
   int iters; // number of simulation rounds
   int* array;
   int* next_board;
   int output_mode;
   int id;
   int grid_allocation; //0: row-wise, 1: column-wise
   int num_threads;
   int print_partition;
};

/* Function Prototypes */
void *play_gol(void *args);
int initialize_board(struct gol_data *data, char *argv[]);
void print_board(struct gol_data *data, int round);
int* get_value_at(int i, int j, struct gol_data *data);
int make_new_array(int i, int j, int* array);
int cell_state(int x, int y, struct gol_data *data);
void partition(struct gol_data *data, int *start_row, int *start_col, int *end_row, int *end_col);

int main(int argc, char *argv[]) {

  int ret, i, ntids;
  pthread_t *tids;
  struct gol_data *tid_args;
  struct gol_data data;
  double secs;
  struct timeval start_time, stop_time;

  if (argc < 2) {
    printf("usage: ./gol infile.txt");
    exit(1);
  }

  ret = initialize_board(&data, argv);
  if(ret != 0) {
    printf("Error initiating board from file %s", argv[1]);
    exit(1);
  }

  if(data.output_mode){
    print_board(&data, 0);
  }

  ret = gettimeofday(&start_time, NULL);

  ntids = data.num_threads;
  pthread_barrier_init(&barrier1, NULL, ntids);
  pthread_barrier_init(&barrier2, NULL, ntids);
  pthread_mutex_init(&mutex, NULL);

  tids = (pthread_t *)malloc(sizeof(pthread_t) * ntids);
  if (!tids) {
    perror("malloc pthread_t array");
    exit(1);
  }

  /* an array of structs */
  tid_args = (struct gol_data *)malloc(sizeof(struct gol_data) * ntids);
  if (!tid_args) {
    perror("malloc tid_args array");
    exit(1);
  }

  for (i = 0; i < ntids; i++) {
    data.id = i;
    tid_args[i] = data;
    ret = pthread_create(&tids[i], 0, play_gol, &tid_args[i]);
    if (ret) {
      perror("Error pthread_create\n");
      exit(1);
    }
  }

  for (i = 0; i < ntids; i++) {
    pthread_join(tids[i], 0);
  }

  if(data.output_mode) {
    if(system("clear")) { perror("clear"); exit(1); }
    print_board(&data, data.iters);
  }

  ret = gettimeofday(&stop_time, NULL);
  secs = (double)(stop_time.tv_sec - start_time.tv_sec)
       + (double)(stop_time.tv_usec - start_time.tv_usec)/pow(10.0, 6.0);

  fprintf(stdout, "Time to run: %0.3f seconds\n", secs);
  fprintf(stdout, "%d live cells after %d rounds\n\n", total_live, data.iters);

  pthread_barrier_destroy(&barrier1);
  pthread_barrier_destroy(&barrier2);
  pthread_mutex_destroy(&mutex);
  free(data.next_board);
  free(data.array);
  free(tid_args);
  free(tids);
  return 0;
}

int initialize_board(struct gol_data *data, char *argv[]) {

  FILE *infile;
  int ret, numcoords, r, c, i, j;
  int* correct_index;

  infile = fopen(argv[1], "r");
  if (infile == NULL) {
    printf("Error: file open %s\n", argv[1]);
    exit(1);
  }

  ret = fscanf(infile, "%d", &data -> rows);
  if(ret != 1) {
    return 1;
    }

  ret = fscanf(infile, "%d", &data -> cols);
  if(ret != 1) {
    return 1;
    }

  ret = fscanf(infile, "%d", &data -> iters);
  if(ret != 1) {
    return 1;
    }

  data -> num_threads = NUM_THREADS;

  data -> grid_allocation = PARTITION;

  data -> output_mode = OUTPUT_MODE;


  data -> array = malloc(sizeof(int)*data -> rows * data -> cols);
  if (!data -> array) {
    perror("malloc array");
    exit(1);
  }
  ret = fscanf(infile, "%d", &numcoords);
   if(ret != 1) {
    return 1;
    }

  data -> next_board = malloc(sizeof(int)*data -> rows * data -> cols);
  if (!data -> next_board) {
    perror("malloc array");
    exit(1);
  }
  for(i = 0; i < data -> rows; i ++){
    for(j = 0; j < data -> cols; j ++){
      data -> array[i * data -> cols + j] = 0;
    }
  }

  for(i=0; i < numcoords; i++){
    ret = fscanf(infile, "%d %d", &r, &c);
    if(ret != 2)
    {
      return 1;
    }
    correct_index = get_value_at(r, c, data);
    if (correct_index == NULL)
    {
      return 1;
    }
    *correct_index = 1;
  }

  total_live = numcoords;

  fclose(infile);
  return 0;
}

void *play_gol(void *args) {

  struct gol_data *data;
  int newCell, start_row, start_col, end_row, end_col;
  int liveCells;
  int* ptr;

  data = (struct gol_data *)args;
  partition(data, &start_row, &start_col, &end_row, &end_col);

  for(int i = 0; i < data -> iters; i++){
    liveCells = 0;
    total_live = 0;
    pthread_barrier_wait(&barrier1);
    for(int j = start_row; j <= end_row; j++){
      for(int k = start_col; k <= end_col; k++){
        newCell = cell_state(j, k, data);
        data -> next_board[j * data -> cols + k] = newCell;
        liveCells += newCell;
      }
    }
    ptr = data -> array;
    data -> array = data -> next_board;
    data -> next_board = ptr;

    pthread_mutex_lock(&mutex);
    total_live += liveCells;
    pthread_mutex_unlock(&mutex);

    pthread_barrier_wait(&barrier1);

    if(data -> output_mode == 1 && data -> id == 0) {
      system("clear");
      print_board(data, i+1);
      usleep(200000);
    }
    pthread_barrier_wait(&barrier1);
  }

  if(data -> print_partition){
    printf("tid  %d: rows:  %d:%d (%d) cols:  %d:%d (%d)\n",
      data->id, start_row, end_row, (end_row - start_row + 1),
      start_col, end_col, (end_col - start_col + 1));
  }
 return NULL;
}

void print_board(struct gol_data *data, int round) {

    int i, j;

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {
            if(*get_value_at(i, j, data) == 1){
                fprintf(stderr, " ★");
            }
            else{
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Round: %d\n", round);
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}

int* get_value_at(int i, int j, struct gol_data *data){

  if(i < data -> rows && j < data -> cols)
  {
    return &data -> array[i * data -> cols + j];
  }
  return NULL;
}

/* Returns the new state of a given cell based
 * on its neighbors.
 *   x: row number of cell
 *   y: column of cell
 *   data: gol game specific data
 */
int cell_state(int x, int y, struct gol_data *data){
  int liveCells = 0;
  int rows = data -> rows;
  int cols = data -> cols;
  for(int i = x-1; i <= x+1; i++){
    for(int j = y-1; j <= y + 1; j++){
      if(*get_value_at((i+rows)%rows, (j+cols)%cols, data) == 1){
        liveCells += 1;
      }
    }
  }
  liveCells -= *get_value_at(x, y, data);
  if (liveCells <= 1 || liveCells >= 4) {
    return 0;
  }
  if (liveCells == 3){
    return 1;
  }
  return *get_value_at(x, y, data);
}


/*
 * Calculates row- or column-wise  partitioning information for a gol board
 * and for a specified thread
 *    data: pointer the the gol_data struct containing the board to be
 *          partitioned
 *    start_row: pointer to value of start row of partition (to be modified)
 *    end_row: pointer to value of end row of partition (to be modified)
 *    start_col: pointer to value of start column of partition (to be modified)
 *    end_col: pointer to value of end column of partition (to be modified)
 */
void partition(struct gol_data *data, int *start_row, int *start_col,
    int *end_row, int *end_col){
  int num_big_parts, start, end, n, i, big_part_size;

  if(data -> grid_allocation){
    n = data -> cols;
  }
  else{
    n = data -> rows;
  }

  num_big_parts = n % data -> num_threads;
  big_part_size = (n - num_big_parts)/data -> num_threads + 1;
  start = 0;
  for(i = 0; i < data -> id; i++){
    if(i < num_big_parts){
      start += big_part_size;
    }
    else{
      start += big_part_size - 1;
    }
  }

  if(data -> id < num_big_parts){
    end = start + big_part_size - 1;
  }
  else{
    end = start + big_part_size - 2;
  }

  if(data -> grid_allocation){
    *start_row = 0;
    *end_row = data -> rows - 1;
    *start_col = start;
    *end_col = end;
  }
  else{
    *start_col = 0;
    *end_col = data -> cols - 1;
    *start_row = start;
    *end_row = end;
  }
}
