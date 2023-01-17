#include "common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include "expr.h"

// this should be enough
static word_t buf_index = 0;
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static inline void gen_rand_op() {
	int choose = rand() % 4;
	switch (choose) {
		case 0:
			buf[buf_index++] = '+';
			break;
		case 1:
			buf[buf_index++] = '-';
			break;
		case 2:
			buf[buf_index++] = '*';
			break;
		case 3:
			buf[buf_index++] = '/';
			break;
		default:
			break;
	}
}

static inline void gen_num() {
	int rander_number = rand() % 10;
	buf[buf_index++] = '0' + rander_number;
}

static inline void gen_rand_expr() {
	int choose  = rand() % 3 ;
	switch (choose) {
		case 0:
			gen_num();
			break;
		case 1:
			buf[buf_index++] = '(';
			gen_rand_expr();
			buf[buf_index++] = ')';
			break;
		case 2: 
			gen_rand_expr();
			gen_rand_op();
			gen_rand_expr();
			break;
		default:
			break;
	}
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  int read_value = 0;
  int read_index = 0;
  char tokenArray[TOKEN_STR_LEN] = {};	

  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

	while((read_value = fgetc(fp)) != EOF){
		tokenArray[read_index] = (char)read_value;
	}

	//printf("%s\n", tokenArray);
	bool success = true;
	word_t eval_result = expr(tokenArray, &success);	
	printf("eval_result:%d\n", eval_result);

    int result;
    fscanf(fp, "%d", &result);

    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
