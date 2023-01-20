#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include <common.h>

#define WP_EX_SIZE 1024

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expresson[WP_EX_SIZE];

  word_t last_expr_value;
} WP;

bool new_wp(WP **wp);

bool free_wp(WP *wp);

bool free_wp_by_no(int no);

void info_wp();

void traverse_wp();

#endif
