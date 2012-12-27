/* Syntax tree -> 32-bit x86 assembly (GAS syntax) generator 
 * (it's my first time [quickly] doing x86 so it's probably far
 * from optimal). The essence of this module is juggling with
 * registers and coping with permitted parameter-types. 
 *		=>	Many improvements possible.	<=
 */

/* NOTE: this code generator uses the standard C boolean
 * idiom of 0 = false, nonzero = true, unlike the BPFVM one.
 */

#include "tree.h"
#include "tokens.h"
#include "codegen_x86.h"
#include <string.h>
#include <stdio.h>

char symtab[256][32] = {""};
char ts_used[TEMP_REGISTERS];
char tm_used[TEMP_MEM];
int syms = 0;
int temp_register = 0;
int swap = 0;

int stack_size;
int intl_label = 0; /* internal label numbering */

extern void fail(char*);
extern void compiler_fail(char *message, token_t *token,
	int in_line, int in_chr);

void print_code() {
	;
}

void new_temp_reg() {
	int i;
	for (i = 0; i < TEMP_REGISTERS; ++i)
		ts_used[i] = 0;
}

char* get_temp_reg() {
	int i;
	for (i = 0; i < TEMP_REGISTERS; ++i)
		if (!ts_used[i]) {
			ts_used[i] = 1;
			return temp_reg[i];
		}
	fail("out of registers");
}

void free_temp_reg(char *reg) {
	int i;

	for (i = 0; i < TEMP_REGISTERS; ++i)
		if (!strcmp(reg, temp_reg[i]))
			ts_used[i] = 0;
}

void new_temp_mem() {
	int i;
	for (i = 0; i < TEMP_MEM; ++i)
		tm_used[i] = 0;
}

char* get_temp_mem() {
	int i;
	for (i = 0; i < TEMP_MEM; ++i)
		if (!tm_used[i]) {
			tm_used[i] = 1;
			return temp_mem[i];
		}
	fail("out of temporary memory");
}

char *symstack(int id) {
	static char buf[128];
	if (!id)
		sprintf(buf, "(%%ebp)");
	else
		sprintf(buf, "-%d(%%ebp)", id * 4);
	return buf;
}

/* Extract the raw string from a token */
char* get_tok_str(token_t t)
{
	static char buf[1024];
	strncpy(buf, t.start, t.len);
	buf[t.len] = 0;
	return buf;
}

/* Check if a symbol is already defined */
int sym_check(token_t* tok)
{
	int i;
	char *s = get_tok_str(*tok);
	char buf[128];

	for (i = 0; i < 256; i++)
		if (!strcmp(symtab[i], s)) {
			sprintf(buf, "symbol `%s' defined twice", s);
			compiler_fail(buf, tok, 0, 0);
		}
	return 0;
}

/*
 * Allocate a permanent storage register without
 * giving it a symbol name. Useful for special 
 * registers created by the compiler. 
 */
int nameless_perm_storage()
{
	return syms++;
}

/*
 * Create a new symbol, obtaining its permanent
 * storage address.
 */ 
int sym_add(token_t *tok)
{
	char *s = get_tok_str(*tok);
	strcpy(symtab[syms], s);
	return syms++; 
}

/* Lookup storage address of a symbol */
int sym_lookup(token_t* tok)
{
	char buf[1024];
	char *s = get_tok_str(*tok);
	int i = 0;

	for (i = 0; i < syms; i++)
		if (!strcmp(symtab[i], s))
			return i;
	sprintf(buf, "unknown symbol `%s'", s);
	compiler_fail(buf, tok, 0, 0);
}

/* Tree type -> arithmetic routine */
char* arith_op(int ty)
{
	switch (ty) {
		case ADD:
			return "addl";
		case SUB:
			return "subl";
		case MULT:
			return "imull";
		case DIV:
			return "idivl";
		default:
			return 0;
	}
}

/* Starting point of the codegen */
void run_codegen(exp_tree_t *tree)
{
	extern void setup_symbols(exp_tree_t* tree);
	char *buf2;
	int i;

	/* setup the identifier symbol table and make stack
	 * space for all the variables in the program */
	setup_symbols(tree);

	for (i = 0; i < TEMP_MEM; ++i) {
		buf2 = malloc(64);
		strcpy(buf2, symstack(nameless_perm_storage()));
		temp_mem[i] = buf2;
	}

	printf(".section .rodata\n");
	printf("format: .string \"%%d\\n\"\n");
	printf(".section .text\n");
	printf(".globl main\n\n");
	printf(".type main, @function\n");
	printf("main:\n");
	printf("# set up stack space\n");
	printf("pushl %%ebp\n");
	printf("movl %%esp, %%ebp\n");
	printf("subl $%d, %%esp\n", syms * 4);
	printf("\n\n# >>> compiled code <<<\n"); 

	codegen(tree);

	printf("\n# clean up stack\n");
	printf("addl $%d, %%esp\n", syms * 4);
	printf("movl %%ebp, %%esp\n");
	printf("popl %%ebp\n");
	printf("\n# exit(0)\n");
	printf("pushl $0\ncall exit\n\n");
	printf(".type echo, @function\n");
	printf("echo:\n");
    printf("pushl $0\n");
    printf("pushl 8(%%esp)\n");
    printf("pushl $format\n");
    printf("call printf\n");
    printf("addl $12, %%esp  # get rid of the printf args\n");
    printf("ret\n");
}

void setup_symbols(exp_tree_t *tree)
{
	int i, sto;

	/* variable declaration, with optional assignment */
	if (tree->head_type == INT_DECL) {
		/* create the storage and symbol */
		if (!sym_check(tree->child[0]->tok))
			(void)sym_add(tree->child[0]->tok);
		/* preserve assignment, else discard tree */
		if (tree->child_count == 2)
			(*tree).head_type = ASGN;
		else
			(*tree).head_type = NULL_TREE;
	}

	/* array declaration */
	if (tree->head_type == ARRAY_DECL) {
		/* make a symbol named after the array
		 * at its index 0 */
		if (!sym_check(tree->child[0]->tok))
			(void)sym_add(tree->child[0]->tok);
		/* make nameless storage for the subsequent
		 * indices -- when we deal with an expression
		 * such as array[index] we only need to know
		 * the starting point of the array and the
		 * value "index" evaluates to */
		sto = atoi(get_tok_str(*(tree->child[1]->tok)));
		for (i = 0; i < sto - 1; i++)
			(void)nameless_perm_storage();
		/* discard tree */
		*tree = null_tree;
	}

	for (i = 0; i < tree->child_count; ++i)
		if (tree->child[i]->head_type == BLOCK
		||	tree->child[i]->head_type == IF
		||	tree->child[i]->head_type == WHILE
		||	tree->child[i]->head_type == INT_DECL
		||	tree->child[i]->head_type == ARRAY_DECL)
			setup_symbols(tree->child[i]);
}

/*
 * The core codegen routine. It returns
 * the temporary register at which the runtime
 * evaluation value of the tree is stored
 * (if the tree is an expression
 * with a value)
 */
char* codegen(exp_tree_t* tree)
{
	char *sto, *sto2, *sto3;
	char *str, *str2;
	char *name;
	int i;
	int sym;
	char *oper;
	char *arith;
	token_t one = { TOK_INTEGER, "1", 1, 0, 0 };
	exp_tree_t one_tree = new_exp_tree(NUMBER, &one);
	exp_tree_t fake_tree;
	exp_tree_t fake_tree_2;
	extern char* cheap_relational(exp_tree_t* tree, char *oppcheck);
	int lab1, lab2;

	if (tree->head_type == BLOCK
		|| tree->head_type == IF
		|| tree->head_type == WHILE
		|| tree->head_type == BPF_INSTR) {
		/* clear temporary memory and registers */
		new_temp_mem();
		new_temp_reg();
	}

	/* relational operators */
	if (tree->head_type == LT)
		return cheap_relational(tree, "jge");
	if (tree->head_type == LTE)
		return cheap_relational(tree, "jg");
	if (tree->head_type == GT)
		return cheap_relational(tree, "jle");
	if (tree->head_type == GTE)
		return cheap_relational(tree, "jl");
	if (tree->head_type == EQL)
		return cheap_relational(tree, "jne");
	if (tree->head_type == NEQL)
		return cheap_relational(tree, "je");

	/* special routines */
	if (tree->head_type == BPF_INSTR) {
		name = get_tok_str(*(tree->tok));
		if (!strcmp(name, "echo")) {
			if (tree->child[0]->head_type == NUMBER) {
				/* optimized code for number operand */
				printf("pushl $%s\n",
					get_tok_str(*(tree->child[0]->tok)));
			} else if (tree->child[0]->head_type == VARIABLE) {
				/* optimized code for variable operand */
				sto = symstack(sym_lookup(tree->child[0]->tok));
				printf("pushl %s\n", sto);
			} else {
				/* general case */
				sto = codegen(tree->child[0]);
				printf("pushl %s\n", sto);
				free_temp_reg(sto);
			}
			printf("call echo\n");
			return NULL;
		}
	}
	
	/* block */
	if (tree->head_type == BLOCK) {
		/* codegen expressions in block */
		for (i = 0; i < tree->child_count; i++) {
			if (tree->child[i]->head_type == ASGN) {
				/* clear temporary memory and registers */
				new_temp_mem();
				new_temp_reg();
			}
			codegen(tree->child[i]);
		}
		return NULL;
	}

	/* pre-increment, pre-decrement of variable lvalue */
	if ((tree->head_type == INC
		|| tree->head_type == DEC)
		&& tree->child[0]->head_type == VARIABLE) {
		sym = sym_lookup(tree->child[0]->tok);
		printf("%s %s\n", tree->head_type == INC ?
			"incl" : "decl", symstack(sym));
		return symstack(sym);
	}

	/* pre-increment, pre-decrement of array lvalue */
	if ((tree->head_type == INC
		|| tree->head_type == DEC)
		&& tree->child[0]->head_type == ARRAY) {
		/* head address */
		sym = sym_lookup(tree->child[0]->child[0]->tok);
		/* index expression */
		str = codegen(tree->child[0]->child[1]);
		/* build pointer */
		sto = get_temp_reg();
		printf("movl %s, %s\n",
			str, sto);
		printf("imull $4, %s\n",
			sto);
		printf("addl %%ebp, %s\n",
			sto);
		printf("addl $%d, %s\n",
			4 * sym, sto);
		/* write the final move */
		sto2 = get_temp_reg();
		printf("incl (%s)\n", sto);
		printf("movl (%s), %s\n", sto, sto2);
		free_temp_reg(sto);
		return sto2;
	}

	/* post-increment, post-decrement of variable lvalue */
	if ((tree->head_type == POST_INC
		|| tree->head_type == POST_DEC)
		&& tree->child[0]->head_type == VARIABLE) {
		sym = sym_lookup(tree->child[0]->tok);
		sto = get_temp_reg();
		/* store the variable's value to temp
		 * storage then bump it and return
		 * the temp storage */
		printf("movl %s, %s\n", symstack(sym), sto);
		printf("%s %s\n",
			tree->head_type == POST_INC ? "incl" : "decl",
			symstack(sym));
		return sto;
	}

	/* post-decrement, post-decrement of array lvalue */
	if ((tree->head_type == POST_INC
		|| tree->head_type == POST_DEC)
		&& tree->child[0]->head_type == ARRAY) {
		/* given bob[haha]++,
		 * codegen bob[haha], keep its value,
		 * codegen ++bob[haha]; 
		 * and give back the kept value */
		sto = codegen(tree->child[0]);
		fake_tree = new_exp_tree(tree->head_type == 
			POST_INC ? INC : DEC, NULL);
		add_child(&fake_tree, tree->child[0]);
		codegen(&fake_tree);
		return sto;
	}

	/* simple variable assignment */
	if (tree->head_type == ASGN && tree->child_count == 2
		&& tree->child[0]->head_type == VARIABLE) {
		sym = sym_lookup(tree->child[0]->tok);
		if (tree->child[1]->head_type == NUMBER) {
			/* optimized code for number operand */
			printf("movl $%s, %s\n",
				get_tok_str(*(tree->child[1]->tok)), symstack(sym));
			return symstack(sym);
		} else if (tree->child[1]->head_type == VARIABLE) {
			/* optimized code for variable operand */
			sto = symstack(sym_lookup(tree->child[1]->tok));
			sto2 = get_temp_reg();
			printf("movl %s, %s\n", sto, sto2);
			printf("movl %s, %s\n", sto2, symstack(sym));
			return symstack(sym);
		} else {
			/* general case */
			sto = codegen(tree->child[1]);
			sto2 = get_temp_reg();
			printf("movl %s, %s\n", sto, sto2);
			printf("movl %s, %s\n", sto2, symstack(sym));
			free_temp_reg(sto2);
			return symstack(sym);
		}
	}

	/* array assignment */
	if (tree->head_type == ASGN && tree->child_count == 2
		&& tree->child[0]->head_type == ARRAY) {
		/* head address */
		sym = sym_lookup(tree->child[0]->child[0]->tok);
		/* index expression */
		str = codegen(tree->child[0]->child[1]);
		/* right operand */
		str2 = codegen(tree->child[1]);
		sto3 = get_temp_reg();
		printf("movl %s, %s\n", str2, sto3);
		/* build pointer */
		sto2 = get_temp_reg();
		printf("movl %s, %s\n",
			str, sto2);
		printf("imull $4, %s\n",
			sto2);
		printf("addl %%ebp, %s\n",
			sto2);
		printf("addl $%d, %s\n",
			4 * sym, sto2);
		/* write the final move */
		sto = get_temp_mem();
		printf("movl %s, (%s)\n",
			sto3, sto2);
		free_temp_reg(sto2);
		free_temp_reg(sto3);
		return str2;
	}

	/* array retrieval */
	if (tree->head_type == ARRAY && tree->child_count == 2) {
		/* head address */
		sym = sym_lookup(tree->child[0]->tok);
		/* index expression */
		str = codegen(tree->child[1]);
		/* build pointer */
		sto2 = get_temp_reg();
		printf("movl %s, %s\n",
			str, sto2);
		printf("imull $4, %s\n",
			sto2);
		printf("addl %%ebp, %s\n",
			sto2);
		printf("addl $%d, %s\n",
			4 * sym, sto2);
		/* write the final move */
		sto = get_temp_reg();
		printf("movl (%s), %s\n",
			sto2, sto);
		free_temp_reg(sto2);
		return sto;
	}

	/* number */
	if (tree->head_type == NUMBER) {
		sto = get_temp_reg();
		printf("movl $%s, %s\n", 
			get_tok_str(*(tree->tok)),
			sto);
		return sto;
	}

	/* variable */
	if (tree->head_type == VARIABLE) {
		sto = get_temp_reg();
		sym = sym_lookup(tree->tok);
		printf("movl %s, %s\n", 
			symstack(sym), sto);
		return sto;
	}

	/* arithmetic */
	if ((arith = arith_op(tree->head_type)) && tree->child_count) {
		/* (with optimized code for number and variable operands
		 * that avoids wasting temporary registes) */
		sto = get_temp_reg();
		sto2 = get_temp_mem();
		for (i = 0; i < tree->child_count; i++) {
			oper = i ? arith : "movl";
			if (tree->child[i]->head_type == NUMBER) {
				printf("%s $%s, %s\n", 
					oper, get_tok_str(*(tree->child[i]->tok)), sto);
			} else if(tree->child[i]->head_type == VARIABLE) {
				sym = sym_lookup(tree->child[i]->tok);
				printf("%s %s, %s\n", oper, symstack(sym), sto);
			} else {
				str = codegen(tree->child[i]);
				printf("%s %s, %s\n", oper, str, sto);
				free_temp_reg(str);
			}
		}
		printf("movl %s, %s\n", sto, sto2);
		free_temp_reg(sto);
		return sto2;
	}

	/* if */
	if (tree->head_type == IF) {
		lab1 = intl_label++;
		lab2 = intl_label++;
		/* codegen the conditional */
		sto = codegen(tree->child[0]);
		/* branch if the conditional is false */
		str = get_temp_reg();
		str2 = get_temp_reg();
		printf("movl %s, %s\n", sto, str);
		printf("movl $0, %s\n", str2);
		printf("cmpl %s, %s\n", str, str2);
		free_temp_reg(sto);
		free_temp_reg(str);
		free_temp_reg(str2);
		printf("je IL%d\n",	lab1);
		/* codegen "true" block */
		codegen(tree->child[1]);
		/* jump over else block if there
		 * is one */
		if (tree->child_count == 3)
			printf("jmp IL%d\n", lab2);
		printf("IL%d: \n", lab1);
		/* code the else block, if any */
		if (tree->child_count == 3)
			codegen(tree->child[2]);
		printf("IL%d: \n", lab2);
		return NULL;
	}

	/* while */
	if (tree->head_type == WHILE) {
		lab1 = intl_label++;
		lab2 = intl_label++;
		/* codegen the conditional */
		printf("IL%d: \n", lab1);
		sto = codegen(tree->child[0]);
		/* branch if the conditional is false */
		str = get_temp_reg();
		str2 = get_temp_reg();
		printf("movl %s, %s\n", sto, str);
		printf("movl $0, %s\n", str2);
		printf("cmpl %s, %s\n", str, str2);
		free_temp_reg(sto);
		free_temp_reg(str);
		free_temp_reg(str2);
		printf("je IL%d\n",	lab2);
		/* codegen the block */
		codegen(tree->child[1]);
		/* jump back to the conditional */
		printf("jmp IL%d\n", lab1);
		printf("IL%d: \n", lab2);
		return NULL;
	}

	/* negative sign */
	if (tree->head_type == NEGATIVE) {
		sto = get_temp_mem();
		sto2 = get_temp_reg();
		str = codegen(tree->child[0]);
		printf("movl $0, %s\n", sto2);
		printf("subl %s, %s\n", str, sto2);
		printf("movl %s, %s\n", sto2, sto);
		free_temp_reg(str);
		return sto;
	}

	/* goto */
	if (tree->head_type == GOTO) {
		printf("jmp %s\n", get_tok_str(*(tree->tok)));
		return NULL;
	}

	/* label */
	if (tree->head_type == LABEL) {
		printf("%s:\n", get_tok_str(*(tree->tok)));
		return NULL;
	}

	/* discarded tree */
	if (tree->head_type == NULL_TREE)
		return NULL;

	fprintf(stderr, "Sorry, I can't yet codegen this tree: \n");
	fflush(stderr);
	printout_tree(*tree);
	fputc('\n', stderr);
	exit(1);
}

char* cheap_relational(exp_tree_t* tree, char *oppcheck)
{
		char *sto, *sto2, *sto3;
		char *str, *str2;
		sto3 = get_temp_reg();
		printf("movl $0, %s\n", sto3);
		sto = codegen(tree->child[0]);
		str = get_temp_reg();
		printf("movl %s, %s\n", sto, str);
		free_temp_reg(sto);
		sto2 = codegen(tree->child[1]);
		str2 = get_temp_reg();
		printf("movl %s, %s\n", sto2, str2);
		free_temp_reg(sto2);
		printf("cmpl %s, %s\n", str2, str);
		free_temp_reg(str);
		free_temp_reg(str2);
		printf("%s IL%d\n", oppcheck, intl_label);
		printf("movl $1, %s\n", sto3);
		printf("IL%d: \n", intl_label++);
		free_temp_reg(sto);
		free_temp_reg(sto2);
		return sto3;
}

