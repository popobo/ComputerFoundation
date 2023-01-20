#include <assert.h>
#include <isa.h>
#include "common.h"
#include "debug.h"
#include "expr.h"
#include "memory/paddr.h"
#include "watchpoint.h"

#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>

void cpu_exec(uint64_t);
int is_batch_mode();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(0);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static int cmd_p(char *args);

static int cmd_w(char *args);

static int cmd_d(char *args);

static int cmd_testp(char *args);

static int cmd_testp1(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute instructions by step", cmd_si },
  { "info", "Print Regiser state", cmd_info },
  { "x", "Scan Memory", cmd_x },
  { "p", "Get the value of expression", cmd_p },
  { "w", "Add a watchpoint", cmd_w },
  { "d", "Delete a watchpoint by no", cmd_d },
  { "testp", "test p", cmd_testp },
  { "testp1", "test p", cmd_testp1 },
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
	long steps = 0;
	
	steps = args != NULL ? strtol(args, NULL, 10) : 1;

	Log("steps: %ld", steps);
	
	if (steps <= 0) {
		steps = 1;
	}

	cpu_exec((uint64_t)steps);

	return 0;
}

static int cmd_info(char *args) {
	if (NULL == args || strlen(args) != 1) {
		Log("failed to info");
		return 0;
	}

	char opt = args[0];

    switch (opt) {
		case 'r': 
			isa_reg_display();
			break;
		case 'w': 
			info_wp();
			break;
    }		

	return 0;
}

static int cmd_x(char *args) {
	if (NULL == args) {
		Log("failed to x");
		return -1;
	}
	
	char *str = args;
	char *token = NULL;
	char *saveptr = NULL;
	long numberOfBytes = 0;
	uint32_t  memoryAddress = 0;

	Log("args:%s", args);
	
	for(int i = 0; ; i++, str = NULL) {
		token = strtok_r(str, " ", &saveptr);
		if (NULL == token) {
			break;
		}
		
		if (0 == i) {
			numberOfBytes = strtol(token, NULL, 10);
		}

		if (1 == i) {
			memoryAddress = (uint32_t)strtol(token, NULL, 16);
		}
	}

	Log("numberOfBytes:%ld, memoryAddress:0x%08x", numberOfBytes, memoryAddress);

	for (int i = 0; i < numberOfBytes; ++i) {
		word_t byte = paddr_read(memoryAddress + i, 1);
		printf("%x ", byte);
	}

	printf("\n");

	return 0;
}

static int cmd_p(char *args) {
	assert(args != NULL);

	Log("args: %s", args);

	bool isExprSuc = 0;

	word_t value = expr(args, &isExprSuc);	
	
	printf("%u\t\t0x%X\n", value, value);

	return 0;
}

static int cmd_w(char *args) {
	if (NULL == args) {
		return -1;
	}

	WP *nwp = NULL;
	bool new_wb_success = new_wp(&nwp);
	if (false == new_wb_success || NULL == nwp) {
		return -1;
	}

	bool success = true;
	word_t value = expr(args, &success);
	if (false == success) {
		printf("cmd expresson is is illegal\n");
		return -1;
	}

	nwp->last_expr_value = value;	
	memcpy(nwp->expresson, args, strlen(args) + 1);

	return 0;
}

static int cmd_d(char *args) {
	if (NULL == args) {
		return -1;
	}
	
	char *end_ptr = NULL;
	int no = (int)strtol(args, &end_ptr, 10);
	if (args == end_ptr) {
		Log("not watchpoint no found");
		return -1;
	}

	bool free_ret = free_wp_by_no(no);
	if (free_ret == false) {
		Log("failed to free watch point");
		return -1;
	}

	return 0;
}

static int cmd_testp(char *args) {
	if (NULL == args) {
		return -1;
	}
	
	char *file = strtok(args, " ");
	if (NULL == file) {
		file = args;
	}

	Log("test_file: %s", file);
	
	FILE *fp = fopen(file, "r");
	if (NULL == fp) {
		return -1;
	}
	
	FILE *result_fp = fopen("/tmp/compare_result", "w+");
	if (NULL == result_fp) {
		return -1;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read_len = 0;
	int index = 0;
	word_t expr_result = 0;
	word_t actual_result = 0;
	char buffer[65536] = {};
	while((read_len = getline(&line, &len, fp)) != -1) {
		if (index % 2 == 0) {
			line[read_len - 1] = '\0';
			memcpy(buffer, line, read_len);
			expr_result = expr(line, NULL);
		} else {
			line[read_len -1] = '\0';
			actual_result = (word_t)strtol(line, NULL, 10);
			Log("expr_result:%d, actual_result:%d", expr_result, actual_result);
			int deviation = actual_result - expr_result;
			if (deviation > 1 || deviation < -1) {
				fprintf(result_fp, "%s\n%d\n", buffer, actual_result);
			}
		}
		index++;
	}

	if (fp != NULL)
		fclose(fp);
	if (result_fp != NULL)
		fclose(result_fp);

	if (line != NULL)
		free(line);

	return 0;
}

static int cmd_testp1(char *args) {
	if (NULL == args) {
		return -1;
	}

	char *file = strtok(args, " ");
	if (NULL == file) {
		file = args;
	}

	Log("test_file: %s", file);
	
	FILE *fp = fopen(file, "r");
	if (NULL == fp) {
		return -1;
	}
	

	char *line = NULL;
	size_t len = 0;
	ssize_t read_len = 0;
	int index = 0;
	word_t expr_result = 0;
	word_t actual_result = 0;
	while((read_len = getline(&line, &len, fp)) != -1) {
		if (index % 2 == 0) {
			line[read_len - 1] = '\0';
			expr_result = expr(line, NULL);
		} else {
			line[read_len -1] = '\0';
			actual_result = (word_t)strtol(line, NULL, 10);
			Log("expr_result:%d, actual_result:%d", expr_result, actual_result);
		}
		index++;
	}

	if (fp != NULL)
		fclose(fp);

	if (line != NULL)
		free(line);

	return 0;
}

void ui_mainloop() {
  if (is_batch_mode()) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
