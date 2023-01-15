#include "common.h"
#include "debug.h"
#include <assert.h>
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum {
  TK_NOTYPE = 256,
  TK_DIGIT,
  TK_EQ,
  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"[0-9]+", TK_DIGIT},  
  {"\\+", '+'},         // plus
  {"-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"==", TK_EQ},        // equal
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

#define TOKEN_STR_LEN 32

typedef struct token {
  int type;
  char str[TOKEN_STR_LEN];
} Token;

#define TOKEN_ARR_LEN 32

static Token tokens[TOKEN_ARR_LEN] = {};
static int nr_token  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
		
		if (nr_token >= TOKEN_ARR_LEN) {
			Log("Too many token in the expression");
			return false;
		}

        switch (rules[i].token_type) {
			case TK_NOTYPE:
				break;
			case TK_DIGIT:
				tokens[nr_token].type = TK_DIGIT;
				if (substr_len > TOKEN_STR_LEN) {
					Log("dight is too long");
					return false;
				}
				memcpy(tokens[nr_token].str, substr_start, substr_len);
				nr_token++;
				break;
			case '+':
				tokens[nr_token++].type = '+';
				break;
			case '-':
				tokens[nr_token++].type = '-';
				break;
			case '*':
				tokens[nr_token++].type = '*';
				break;
			case '/':
				tokens[nr_token++].type = '/';
				break;
			case '(':
				tokens[nr_token++].type = '(';
				break;
			case ')':
				tokens[nr_token++].type = ')';
				break;
			default:
				Log("token is not dealt, token%s\n", substr_start);
        }
		
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

	for (int i = 0; i < nr_token; ++i) {
		Log("tokens type: %d, tokens str: %s", tokens[i].type, tokens[i].str);
	}

  return true;
}

static bool check_parentheses(int front, int end) {
	int numberOfLB = 0;
	bool result = true;

	do {
		if (tokens[front].type != '(' || tokens[end].type != ')') {
			result = false;
			break;
		}
		
		for (int i = front; i < end + 1; ++i) {
			if (tokens[i].type == '(') {
				++numberOfLB;
			}
			else if (tokens[i].type == ')') {
				--numberOfLB;
			}

			
			if (numberOfLB == 0 && i != end) {
				result = false;
				// we wanna see whether this expression is illegal
				// so no break here
			}
			else if (numberOfLB < 0) {
				// In this case, the expression is illegal
				result = false;
				break;
			}
		}

		if (numberOfLB > 0) {	
			// In this case, the expression is illegal
			result = false;
			break;
		}
	} while(false);


	return result;
}

static int find_main_operator(int front, int end, bool *success) {
	bool is_in_par = false;
	int operators[TOKEN_ARR_LEN] = {};
	int index = 0;
	int main_op_index = 0;
	for (int i = front; i < end + 1; ++i) {
		if ('(' == tokens[i].type) {
			is_in_par = true;
		}
		else if (')' == tokens[i].type) {
			is_in_par = false;
		}
		else if ('+' == tokens[i].type ||
			     '-' == tokens[i].type || 
				 '*' == tokens[i].type ||
				 '/' == tokens[i].type) {
			if (true == is_in_par) {
				continue;
			}
			operators[index++] = i; 
		}
	}
	
	main_op_index = operators[0];
	for (int i = 0; i < index; ++i) {
		switch (tokens[operators[i]].type) {
		case '+':
		case '-':
			main_op_index = operators[i];
			break;
		case '*':
		case '/':
			if ('*' == tokens[main_op_index].type ||
				'/' == tokens[main_op_index].type) {
				main_op_index = operators[i];
			}
			break;
		default:
			assert(0);
		}
	}
	if (success != NULL) {
		*success = index > 0 ? true : false;
	}
	return main_op_index;
}

static int eval(int front, int end, bool *success) {
	Log("front:%d, end:%d", front, end);

	if (front > end) {
		return 0;
	}
	else if (front == end) {
		word_t value = 0;
		if (tokens[front].type == TK_DIGIT) {
			value = (word_t)strtol(tokens[front].str, NULL, 10);
		}
		return value;
	}
	else if (check_parentheses(front, end) == true) {
		return eval(front + 1, end - 1, success);
	}
	else {
		bool find_main_operator_success = false;
		int main_op_index = find_main_operator(front, end, &find_main_operator_success);
		word_t val1 = eval(front, main_op_index - 1, success);
		word_t val2 = eval(main_op_index + 1, end, success);

		switch (tokens[main_op_index].type) {
			case '+':
				return val1 + val2;
			case '-':
				return val1 - val2;
			case '*':
				return val1 * val2;
			case '/':
				return val1 / val2;
			default:
				assert(0);
		}
	}
	return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  int value =  eval(0, nr_token - 1, success);

  return value;
}
