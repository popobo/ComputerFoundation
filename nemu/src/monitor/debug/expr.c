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

static bool check_parentheses(int front, int end, bool *is_prarentthesises_legal) {
	int numberOfLB = 0;
	bool result = true;
	bool is_ever_in_parenthesised = false;
	if (is_prarentthesises_legal != NULL) {
		*is_prarentthesises_legal = true;
	}
	
	do {
		if (tokens[front].type != '(' || tokens[end].type != ')') {
			result = false;
			// need to tell whether this expression is illegal, so do not return
		}
		
		for (int i = front; i < end + 1; ++i) {
			if (tokens[i].type == '(') {
				++numberOfLB;
				is_ever_in_parenthesised = true;
			}
			else if (tokens[i].type == ')') {
				--numberOfLB;
			}
			
			if (is_ever_in_parenthesised && numberOfLB == 0 && i != end) {
				result = false;
				// we wanna see whether this expression is illegal
				// so no break here
			}
			else if (numberOfLB < 0) {
				// In this case, the expression is illegal
				result = false;
				if (is_prarentthesises_legal != NULL){
					*is_prarentthesises_legal = false;
				}
				break;
			}
		}

		if (numberOfLB > 0) {	
			// In this case, the expression is illegal
			result = false;
			if (is_prarentthesises_legal != NULL) {
				*is_prarentthesises_legal = false;
			}
			break;
		}
	} while(false);


	return result;
}


static int find_main_operator(int front, int end, bool *is_main_operator_found) {
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
	
	if (is_main_operator_found != NULL) {
		*is_main_operator_found = index > 0 ? true : false;
	}
	
	return main_op_index;
}

typedef struct eval_status {
	bool is_front_end_correct;
	bool is_prarentthesises_legal;
	bool is_main_operator_found;
} eval_status;

static word_t eval(int front, int end, eval_status * status) {
	Log("front:%d, end:%d", front, end);
	
	word_t result = 0;
	bool is_front_end_correct = true;
	bool is_prarentthesises_legal = true;
	bool is_main_operator_found = true;

	do {	
		if (front > end) {
			result = 0;
			is_front_end_correct = false;
		}
		else if (front == end) {
			if (tokens[front].type == TK_DIGIT) {
				result = (word_t)strtol(tokens[front].str, NULL, 10);
			}
			break;
		}
		else if (check_parentheses(front, end, &is_prarentthesises_legal) == true) {
			result = eval(front + 1, end - 1, status);
		}
		else {
			if (false == is_prarentthesises_legal) {
				result = 0;
				break;
			}
			int main_op_index = find_main_operator(front, end, &is_main_operator_found);
			word_t val1 = eval(front, main_op_index - 1, status);
			word_t val2 = eval(main_op_index + 1, end, status);

			switch (tokens[main_op_index].type) {
				case '+':
				    result = val1 + val2;
					break;
				case '-':
					result = val1 - val2;
					break;
				case '*':
					result = val1 * val2;
					break;
				case '/':
					result = val1 / val2;
					break;
				default:
					assert(0);
			}
		}

	} while(false);
		
	if (status != NULL) {
		status->is_front_end_correct = status->is_front_end_correct && is_front_end_correct;
		status->is_main_operator_found = status->is_main_operator_found && is_main_operator_found;
		status->is_prarentthesises_legal = status->is_prarentthesises_legal && is_prarentthesises_legal;
	}

	return result;
}

word_t expr(char *e, bool *success) {
	if (!make_token(e)) {
		*success = false;
		return 0;
	}
	
	eval_status status = {true, true, true};
	int value =  eval(0, nr_token - 1, &status);
	
	Log("is_front_end_correct:%d," 
		"is_main_operator_found:%d," 
		" is_prarentthesises_legal:%d", status.is_front_end_correct, status.is_main_operator_found, status.is_prarentthesises_legal);

	return value;
}
