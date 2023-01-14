#include <isa.h>
#include "common.h"
#include "debug.h"
#include "expr.h"
#include "memory/paddr.h"
#include "watchpoint.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

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

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute instructions by step", cmd_si},
  { "info", "Print Regiser state", cmd_info},
  { "x", "Scan Memory", cmd_x},
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
			break;
    }		

	return 0;
}

static int cmd_x(char *args) {
	if (NULL == args) {
		Log("failed to x");
		return 0;
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
