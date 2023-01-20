#include "common.h"
#include "debug.h"
#include <assert.h>
#include <isa.h>
#include "expr.h"
#include "isa/riscv32.h"
#include "memory/paddr.h"

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
  TK_HEX,
  TK_REG_NAME,
  TK_EQ,
  TK_NO_EQ,
  TK_AND,
  TK_NEGATIVE,
  TK_DEREF,
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
  {"0x[0-9|a-f]+", TK_HEX},
  {"[0-9]+", TK_DIGIT},
  {"\\$[\\$|a-z|0-9]+", TK_REG_NAME},
  {"\\+", '+'},         // plus
  {"-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"==", TK_EQ},        // equal
  {"!=", TK_NO_EQ},
  {"&&", TK_AND},
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

typedef struct token {
  int type;
  char str[TOKEN_STR_LEN];
} Token;

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
			case TK_HEX:
			case TK_REG_NAME:
				tokens[nr_token].type = rules[i].token_type;
				if (substr_len > TOKEN_STR_LEN) {
					Log("too long");
					return false;
				}
				memcpy(tokens[nr_token].str, substr_start, substr_len);
				nr_token++;
				break;
			
			case '+':
			case '-':
			case '*':
			case '/':
			case '(':
			case ')':
			case TK_EQ:
			case TK_NO_EQ:
			case TK_AND:
				tokens[nr_token++].type = rules[i].token_type;
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


static int find_main_operator(int front, int end) {
	int number_of_lb = 0;
	int operators[TOKEN_ARR_LEN] = {};
	int index = 0;
	int main_op_index = 0;
	for (int i = front; i < end + 1; ++i) {
		switch (tokens[i].type) {
			case '(':
				++number_of_lb;
				break;
			case ')':
				--number_of_lb;
				break;
			case '+':
			case '-':
			case '*':
			case '/':
			case TK_EQ:
			case TK_NO_EQ:
			case TK_AND:
				if (number_of_lb > 0) {
					continue;
				}
				operators[index++] = i; 
				break;
		}
	}

	if (0 == index) {
		return -1;
	}
	
	main_op_index = operators[0];
	for (int i = 0; i < index; ++i) {
		int main_op_type = tokens[main_op_index].type;
		int i_op_type = tokens[operators[i]].type;
		switch (i_op_type) {
		case '+':
		case '-': 
			if (main_op_type != TK_EQ ||
				main_op_type != TK_NO_EQ ||
				main_op_type != TK_AND) {
				
				main_op_index = operators[i];
			
			}
			break;
		case '*':
		case '/':
			if (main_op_type != TK_EQ ||
				main_op_type != TK_NO_EQ ||
				main_op_type != TK_AND ||
				main_op_type != '+' ||
				main_op_type != '-') {
				
				main_op_index = operators[i];
			}
			break;
		case TK_EQ:
		case TK_NO_EQ:
			if (main_op_type != TK_AND) {
				main_op_index = operators[i];
			}	
		
			break;
		case TK_AND:
			main_op_index = operators[i];
			break;
		default:
			assert(0);
		}
	}
	
	return main_op_index;
}

static double eval_get_value(int index, bool *is_value_legal) {
	double result = 0.0;
	bool is_val_leg = true;

	switch (tokens[index].type) {
		case TK_DIGIT: {
			char *end_ptr = NULL;
			result = strtol(tokens[index].str, &end_ptr, 10);
			if (end_ptr == tokens[index].str)
				is_val_leg = false;
			break;
		}
		case TK_HEX: {
			char *end_ptr = NULL;
			result = strtol(tokens[index].str, &end_ptr, 16);
			if (end_ptr == tokens[index].str)
				is_val_leg = false;
			break;
		}
		case TK_REG_NAME:
			result = isa_reg_str2val(tokens[index].str + 1, &is_val_leg);
			break;
		default:
			is_val_leg = false;
			break;
	}

	if (NULL != is_value_legal)
		*is_value_legal = is_val_leg;

	return result;
}

static double eval_without_operator(int front, int end, bool *is_value_legal) {
	Log("front:%d, end:%d", front, end);
	double result = 0.0;

	if (TK_DIGIT == tokens[end].type ||
		TK_REG_NAME == tokens[end].type ||
		TK_HEX == tokens[end].type) {
		
		result = eval_get_value(end, is_value_legal);
		if (NULL != is_value_legal &&
			false == is_value_legal) {
			return result;
		}

		for(int i = end - 1; i > -1; --i) {
			if (TK_NEGATIVE == tokens[i].type) {
				result = - result;
			} else if (TK_DEREF == tokens[i].type) {
				result = (double)paddr_read(result, 1);
			}
		}
	}	

	return result;
}

typedef struct eval_status {
	bool is_prarentthesises_legal;
	bool is_value_legal;
} eval_status;

static double eval(int front, int end, eval_status * status) {
	Log("front:%d, end:%d", front, end);
	
	double result = 0;
	bool is_prarentthesises_legal = true;
	bool is_value_legal = true;

	do {	
		if (front > end) {
			result = 0;
		}
		else if (front == end) {
			result = eval_get_value(end, &is_value_legal);
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
			int main_op_index = find_main_operator(front, end);
			// deal with unary operatpr
			if (-1 == main_op_index) {
				result = eval_without_operator(front, end, &is_value_legal);
				break;
			}
				
			double val1 = eval(front, main_op_index - 1, status);
			double val2 = eval(main_op_index + 1, end, status);

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
					if (val2 != 0) {
						result = val1 / val2;
					}
					break;
				case TK_EQ:
					result = (word_t)val1 == (word_t)val2;
					break;
				case TK_NO_EQ:
					result = (word_t)val1 != (word_t)val2;
					break;
				case TK_AND:
					// add word_t to make sure bool is correct
					result = (word_t)val1 && (word_t)val2;
					break;
				default:
					is_value_legal = false;
			}
		}

	} while(false);
		
	if (status != NULL) {
		status->is_prarentthesises_legal = status->is_prarentthesises_legal && is_prarentthesises_legal;
		status->is_value_legal = status->is_value_legal && is_value_legal; 
	}

	return result;
}

word_t expr(char *e, bool *success) {
	if (!make_token(e)) {
		if (success != NULL)
			*success = false;
		return 0;
	}

	for (int i = 0; i < nr_token; ++i) {
		if (('-' == tokens[i].type || '*' == tokens[i].type) && 
			(i == 0 || 
			 tokens[i - 1].type == '+' || 
			 tokens[i - 1].type == '-' ||
			 tokens[i - 1].type == '*' ||
			 tokens[i - 1].type == '/' ||
			 tokens[i - 1].type == TK_EQ ||
			 tokens[i - 1].type == TK_NO_EQ ||
			 tokens[i - 1].type == TK_AND ||
			 tokens[i - 1].type == '(' ||
			 tokens[i - 1].type == TK_NEGATIVE ||
			 tokens[i - 1].type == TK_DEREF)) {
			if ('-' == tokens[i].type) {
				tokens[i].type = TK_NEGATIVE;
			} else if ('*' == tokens[i].type) {
				tokens[i].type = TK_DEREF;
			}
		}
		Log("%d, %s", tokens[i].type, tokens[i].str);
	}

	eval_status status = {true, true};
	double value =  eval(0, nr_token - 1, &status);
	printf("%lf", value);
	memset(tokens, 0, sizeof(tokens));

	Log( "is_value_legal:%d," 
		" is_prarentthesises_legal:%d, "
		" value:%lf", 
		status.is_value_legal, 
		status.is_prarentthesises_legal,
		value);

	if (value > UINT32_MAX) {
		return 0;
	}

	value = value > 0 ? value + 0.5 : value - 0.5; // for round
	
	return (word_t)value;
}
