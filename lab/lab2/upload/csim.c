/*
 * @Author: [Zhuohao Lee]
 * @Date: 2022-05-17 00:20:00
 * @LastEditors: [Zhuohao Lee]
 * @LastEditTime: 2022-05-31 16:29:04
 * @FilePath:
 * /undefined/Users/edith_lzh/Desktop/大三下/Architecture/github/lab/lab2/upload/csim.c
 * @Description: edith_lzh@sjtu.edu.cn
 * Copyright (c) 2022 by Zhuohao Lee, All Rights Reserved.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cachelab.h"

// parameters definition
int S;                 // num of sets
int E;                 // num of ways == lines per set
int B;                 // block size
int index_s;           // store the index of sets when allocates
int index_E;           // store the index of lines in a set.
int index_b;           // store the index of blocks in memory
int current_time;      // manupulate LRU replacement
char input_file[100];  // input file generated by others
FILE* file_pointer;    // manupulate input_file

// result printed
int hits;
int misses;
int evictions;

// primary data structure
typedef struct {
  int valid;  // valid bit, 0 is invalid, 1 is valid
  int tag;    // tag bit
  int time;   // LRU time, manupulate LRU
} cache_line;
// abstract of cache
//*   *cache specifies each sets, **cache specifies each block   *//
cache_line** cache;

/* functions declare */
// initialize cache
void init_cache();
// parse operations from input file
void operation_parse();
// free cache
void free_cache();
// update cache
void update_cache(unsigned int address);
// update time
void update_time();
// manual
void man_help();

//*******  main funxtion *********//

int main(int argc, char** argv) {
  // initialize index
  index_b = -1;
  index_E = -1;
  index_s = -1;

  int params;  // restore
  // parse command line
  while ((params = getopt(argc, argv, "s:E:b:t:hv")) != -1) {
    switch (params) {
      case 'h':
        man_help();
        break;
      case 's':
        index_s = atoi(optarg);
        break;
      case 'E':
        index_E = atoi(optarg);
        break;
      case 'b':
        index_b = atoi(optarg);
        break;
      case 't':
        strcpy(input_file, optarg);
        break;
      case 'v':
      default:
        printf("csim command error: illegal command input\n");
        man_help();  // default print manual
        break;
    }
    /* code */
  }

  // legal check
  if ((index_b <= 0) || (index_E <= 0) || (index_s <= 0)) {
    return -1;
  }

  // parse file
  file_pointer = fopen(input_file, "r");  // open file with read permission
  if (file_pointer == NULL) {
    printf("File Error: Can't Open File\n");
    return -1;
  }

  // result init
  hits = 0;
  misses = 0;
  evictions = 0;

  // init time
  current_time = 0;

  // init
  B = 1 << index_b;  // B = 2^index_b
  S = 1 << index_s;  // S = 2^index_s
  E = index_E;       // ways just are all you need

  // let's begin
  init_cache();                           // initialize cache
  operation_parse();                      // parse instructions in input file
  free_cache();                           // free cache real memory
  fclose(file_pointer);                   // free file memory
  printSummary(hits, misses, evictions);  // report result
  return 0;
}

//******** end of main ********//

void man_help() {
  printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
  printf("Options:\n");
  printf("-h         Print this help maunual.\n");
  printf("-v         Optional verbose flag.\n");
  printf("-s <num>   Number of set index bits.\n");
  printf("-E <num>   Number of lines per set.\n");
  printf("-b <num>   Number of block offset bits.\n");
  printf("-t <file>  Trace file.\n\n\n");
  printf("Examples:\n");
  printf("linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
  printf("linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

void init_cache() {
  // malloc for a new cache
  //* for a cache at the top, it has a memory size of S*cache_line
  cache = (cache_line**)malloc(sizeof(cache_line*) * S);

  // clear all zeros
  for (int i = 0; i < S; ++i) {
    //* for each set, it has a memory size of E
    cache[i] = (cache_line*)malloc(sizeof(cache_line) * E);
    for (int j = 0; j < E; ++j) {
      cache[i][j].valid = 0;
      cache[i][j].tag = -1;
      cache[i][j].time = -1;
      /* code */
    }
  }
}

void operation_parse() {
  unsigned int address;  // address, 2nd argument
  int size;  // size, the number of bytes accessed by the operation, 3rd
             // argument
  char operation;
  char command;  // manupulate, -v, -t, -h

  while (fscanf(file_pointer, "%c %xu%c%xu ", &operation, &address, &command,
                &size) > 0) {
    switch (operation) {
      case 'I':  // I: just continue
        continue;
      case 'L':  // L
        update_cache(address);
        break;
      case 'M':  // M
        update_cache(address);
      case 'S':  // S
        update_cache(address);
        break;
      default:
        break;
    }
    update_time();
  }
}

void free_cache() {
  // iteration release mem
  for (int i = 0; i < S; ++i) {
    free(cache[i]);
  }
  free(cache);  // 2-level pointer
}

// main implementation in this part
void update_cache(unsigned int address) {
  // sparse tag value
  // address: |tag|set|offset|
  int tag_spec =
      (address >> (index_b + index_s));  // address right shift for
                                         // index_b+index_s sparse set value
  int set_spec =
      (address >> index_b) &
      ((0xFFFFFFFF) >> (64 - index_s));  // it should be |set|, bit-wise AND
                                         // could sparse |set| from origin

  // look up in a set
  for (int i = 0; i < E; ++i) {
    /* hit: tag is matched && valid == 1 */
    if ((cache[set_spec][i].tag == tag_spec) && cache[set_spec][i].valid == 1) {
      hits++;  // hit plus 1
      cache[set_spec][i].time =
          current_time;  // update the visit time to do LRU
      return;
    }
  }

  for (int i = 0; i < E; ++i) {
    // invalid --- miss
    if (cache[set_spec][i].valid == 0) {
      cache[set_spec][i].valid = 1;  // turn to valid
      cache[set_spec][i].tag = tag_spec;
      cache[set_spec][i].time = current_time;
      misses++;
      return;
    }
  }

  // valid but not match --- eviction
  evictions++;
  misses++;
  //******* LRU replacement *******//
  int min_time = 10000;  // simulate \infty
  int ir;                // replaced block
  for (int i = 0; i < E; ++i) {
    if (cache[set_spec][i].time < min_time) {
      min_time = cache[set_spec][i].time;  // update time
      ir = i;                              // update ir
      /* code */
    }
  }
  // after LRU, ir is what we're gonna replace
  cache[set_spec][ir].tag = tag_spec;
  cache[set_spec][ir].time = current_time;
}

void update_time() { current_time++; }