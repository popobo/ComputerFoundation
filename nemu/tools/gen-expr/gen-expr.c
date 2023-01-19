#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include "common.h"
#include "expr.h"

#define BUF_SIZE 65536

// this should be enough
static word_t buf_index = 0;
static word_t buf_for_test_index = 0;
static char buf[BUF_SIZE] = {};
static char buf_for_test[BUF_SIZE] = {};
static char code_buf[BUF_SIZE + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  double result = %s; "
"  result += 0.5; "
"  long temp = result;"
"  printf(\"%%ld\", temp); "
"  return 0; "
"}";
static int token_number = 0;

static int choose(int n) {
	return rand() % n;
}

static inline void gen_spaces() {
	int random_length = choose(3);
	for (int i = 0; i < random_length; ++i) {
		buf[buf_index++] = ' ';
		buf_for_test[buf_for_test_index++] = ' ';
	}
}

static inline void gen_rand_op() {
	if (buf_index > BUF_SIZE - 1) {
		return;
	}
	if (buf_for_test_index > BUF_SIZE - 1) {
		return;
	}
	gen_spaces();
	token_number++;
	switch (choose(4)) {
		case 0:
			buf[buf_index++] = '+';
			buf_for_test[buf_for_test_index++] = '+';
			break;
		case 1:
			buf[buf_index++] = '-';
			buf_for_test[buf_for_test_index++] = '-';
			break;
		case 2:
			buf[buf_index++] = '*';
			buf_for_test[buf_for_test_index++] = '*';
			break;
		case 3:
			buf[buf_index++] = '/';
			buf_for_test[buf_for_test_index++] = '/';
			break;
		default:
			break;
	}
	gen_spaces();
}

static inline void gen_num() {
	if (buf_index > BUF_SIZE - 1) {
		return;
	}
	if (buf_for_test_index > BUF_SIZE - 1) {
		return;
	}
	gen_spaces();
	token_number++;
	int random_length = choose(10);
	random_length = random_length == 0 ? 1 :random_length;
	for (int i = 0; i < random_length; ++i) {
		int random_number = choose(10);
		if (0 == i && 0 == random_number){
			buf[buf_index++] = 1 + '0';
			buf_for_test[buf_for_test_index++] = 1 + '0';
			continue;
		}
		buf[buf_index++] = random_number + '0';
		buf_for_test[buf_for_test_index++] = random_number + '0';
	}
	buf[buf_index++] = '.';
	buf[buf_index++] = '0';
	gen_spaces();
}

static inline void gen_c(char charact) {
	if (buf_index > BUF_SIZE - 1) {
		return;
	}
	gen_spaces();
	token_number++;
	buf[buf_index++] = charact;
	buf_for_test[buf_for_test_index++] = charact;
	gen_spaces();
}

static inline void gen_rand_expr() {
	int choose  = rand() % 3 ;
	switch (choose) {
		case 0: 
			gen_num();
			break;
		case 1:
			gen_c('(');
			gen_rand_expr();
			gen_c(')');
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
  int loop = 10;

  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }

  int i;
  for (i = 0; i < loop; i ++) {
	token_number = 0;
	buf_index = 0;
	memset(buf, 0, sizeof(buf)/sizeof(buf[0]));

	buf_for_test_index = 0;
	memset(buf_for_test, 0, sizeof(buf_for_test)/sizeof(buf_for_test[0]));
	
	gen_rand_expr();

	if (token_number > TOKEN_ARR_LEN || buf_index > BUF_SIZE - 1) {
		i--;
		continue;
	}

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

	//clear the file
	FILE *code_temp_fp = fopen("/tmp/.code_temp", "w+");
	if (code_temp_fp != NULL)
		fclose(code_temp_fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr 2> /tmp/.code_temp");
    if (ret != 0) continue;
	
	code_temp_fp = fopen("/tmp/.code_temp", "r+");
	size_t len = 0;
	ssize_t read_len = 0;
	char *line = NULL;
	char *warning = NULL;
	while((read_len = getline(&line, &len, code_temp_fp)) != -1) {
		warning = strstr(line, "-Wdiv-by-zero");
		if (warning != NULL) {
			break;
		}
		warning = strstr(line, "-Woverflow");
		if (warning != NULL) {
			printf("-Woverflow");
			break;
		}
	}

	
	if (line != NULL)
		free(line);

	if (warning != NULL) {
		printf("this is illegal expression because div by 0\n"
			   "expression %s\n", buf);
		if (code_temp_fp != NULL)
			fclose(code_temp_fp);
		//truncate file to zero
		code_temp_fp = fopen("/tmp/.code_temp", "w+");
		if (code_temp_fp != NULL)
			fclose(code_temp_fp);
		
		// this expression does not count
		--i;
		continue;
	} 
	else {
		if (code_temp_fp != NULL)
			fclose(code_temp_fp);
	}
	

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    long result;
    fscanf(fp, "%ld", &result);

    pclose(fp);
	
	if (result < 0) {
		--i;
		continue;
	}
	
	if (result > UINT32_MAX) {
		--i;
		continue;
	}

    //printf("\nvalue: %d\n expression: %s\n", result, buf);
	
	FILE *store_expressions_fp = fopen("/tmp/expressions", "a+");
	assert(store_expressions_fp != NULL);

	if (store_expressions_fp != NULL) {
		fprintf(store_expressions_fp, "%s\n%ld\n", buf_for_test, result);
		fclose(store_expressions_fp);
	}
  }

  return 0;
}
