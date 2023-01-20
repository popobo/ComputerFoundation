#include "watchpoint.h"
#include "common.h"
#include "expr.h"
#include <assert.h>

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

bool new_wp(WP **wp) {
	if (NULL == wp) {
		return false;
	}

	if (NULL == free_) {
		return false;
	}


	*wp = free_;
	if (head == NULL) {
		head = free_;
		free_ = free_->next;
		head->next = NULL;
	} else {
		// find the end of head
		WP *temp_head = head;
		while(temp_head->next != NULL) {
			temp_head = temp_head->next;
		}
		temp_head->next = free_;
		free_ = free_->next;
		temp_head->next->next = NULL;
	}
	return true;
}

bool free_wp(WP *wp) {
	if (NULL == wp) {
		return false;
	}
	
	if (NULL == head) {
		return false;
	}

	if (head == wp) {
		if (NULL == free_) {
			free_ = head;
			head = head->next;
			free_->next = NULL;
		} else {
			WP *temp_free = free_;
			while(temp_free->next != NULL) {
				temp_free = temp_free->next;
			}
			temp_free->next = head;
			head = head->next;
			temp_free->next->next = NULL;
		}
		memset(wp->expresson, 0, WP_EX_SIZE);
		return true;
	}

	WP *temp_head = head;
	while(temp_head->next != NULL) {
		if (temp_head->next == wp) {
			break;
		}
	}

	//target wp is not found
	if (NULL == temp_head->next) {
		return false;
	}
	
	if (NULL == free_) {
		free_ = temp_head->next;
		temp_head->next = temp_head->next->next;
	} else {
		WP *temp_free = free_;
		while(temp_free->next != NULL) {
			temp_free = temp_free->next;
		}
		temp_free->next = temp_head->next;
		temp_head->next = temp_head->next->next;
		temp_free->next->next = NULL;
	}

	memset(wp->expresson, 0, WP_EX_SIZE);
	return true;
}

bool free_wp_by_no(int no){
	if (no < 0 || no > sizeof(wp_pool)/sizeof(wp_pool[0]) - 1) {
		return false;
	}

	return free_wp(&wp_pool[no]);
}

void info_wp() {
	WP *temp_head = head;
	while(temp_head != NULL) {
		printf("NO:%d\t\texpression:%s\n", 
				temp_head->NO, temp_head->expresson);
		temp_head = temp_head->next;
	}
}

void traverse_wp() {
	WP *temp_head = head;
	word_t value = 0;
	while(temp_head != NULL) {
		value = expr(temp_head->expresson, NULL);
		if (value != temp_head->last_expr_value) {
			printf("NO:%d \t expresson:%s \t old value:%d\n"
				   "NO:%d \t expresson:%s \t new value:%d\n", 
				   temp_head->NO, temp_head->expresson, temp_head->last_expr_value, 
				   temp_head->NO, temp_head->expresson, value);
			temp_head->last_expr_value = value;
		}
		temp_head = temp_head->next;
	}
}

