#define IN_BACKEND
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generator/generator-types.h"
#include "generator/generator.h"
#include "handle-types.h"
#include "backend/registers.h"
#include "globals.h"
#include "backend/registers.h"
#include "types.h"

static inline void error(char *name, size_t size)
{
	fprintf(stderr, "Internal Error: size %ld passed to function %s, no handler found.\n", size, name);
}

void backend_make_global_var(FILE *fd, struct var_t *v)
{
	fprintf(fd, "\t.comm %s, %d, %d\n", v->name, get_type_size(v->type), get_type_size(v->type));

}

static void inc_by_int(FILE *fd, int i, char *dest, size_t size)
{
	if (size==char_size)
		fprintf(fd, "\taddb $%d, %s\n", i, dest);
	else if (size==word_size)
		fprintf(fd, "\taddl $%d, %s\n", i, dest);
	else if (size==pointer_size)
		fprintf(fd, "\taddq $%d, %s\n", i, dest);
}

void get_address(FILE *fd, struct expr_t *_var)
{
	struct reg_t *ret=get_ret_register(pointer_size);
	if (_var->kind==var) {
		fprintf(fd, "\tmovq %%rbp, %s\n", get_reg_name(ret, pointer_size));
		inc_by_int(fd, _var->attrs.var->offset, get_reg_name(ret, pointer_size), pointer_size);
	}
}

static inline void print_assign_var(FILE *fd, char *operator, struct reg_t *reg, struct var_t *var)
{
	fprintf(fd, "\t%s %s, var$%s$%d(%%rbp)\n", operator, reg_name(reg), var->name, var->scope_depth);
}
void assign_var(FILE *fd, struct reg_t *src, struct var_t *dest)
{
	if (dest==NULL) {
		fprintf(stderr, "Internal error: a NULL variable pointer was passed to assign_var\n");
		exit(1);
	}

	size_t size=get_type_size(dest->type);
	if (dest->scope_depth!=0) {
		fprintf(fd, "\t.equ var$%s$%d, %ld\n", dest->name, dest->scope_depth, dest->offset);
		if (size==char_size)
			print_assign_var(fd, "movb", src, dest);
		else if (size==word_size)
			print_assign_var(fd, "movl", src, dest);
		else if (size==pointer_size)
			print_assign_var(fd, "movq", src, dest);
		else
			error("assign_var", size);
	} else {
		if (size==word_size)
			fprintf(fd, "\tmovl %s, %s(%%rip)\n", reg_name(src), dest->name);
	}
} 

static inline void print_read_var(FILE *fd, char *operator, char *reg, struct var_t *var)
{
	fprintf(fd, "\t%s var$%s$%d(%%rbp), %s\n", operator, var->name, var->scope_depth, reg);
}

void read_var(FILE *fd, struct var_t *v)
{
	fprintf(fd, "\t.equ var$%s$%d, %d\n", v->name, v->scope_depth, v->offset);
	if (v->scope_depth!=0) {
		size_t size=get_type_size(v->type);
		if (size==char_size)
			print_read_var(fd, "movb", "%al", v);
		else if (size==word_size)
			print_read_var(fd, "movl", "%eax", v);
		else if (size==pointer_size)
			print_read_var(fd, "movq", "%rax", v);
		else
			error("read_var", size);
	} else {
		if (get_type_size(v->type)==word_size) {
			fprintf(fd, "\tmovl %s(%%rip), %%eax\n", v->name);
		}
	}
}

void dereference(FILE *fd, struct reg_t *reg, size_t size)
{
	fprintf(fd, "\tmovq (%s), %%rax\n", reg_name(reg));
}

void assign_dereference(FILE *fd, struct reg_t *assign_from, struct reg_t *assign_to)
{

	if (assign_from->size==char_size)
		fprintf(fd, "\tmovb %s, (%s)\n", reg_name(assign_from), reg_name(assign_to));
	else if (assign_from->size==word_size)
		fprintf(fd, "\tmovl %s, (%s)\n", reg_name(assign_from), reg_name(assign_to));
	else if (assign_from->size==pointer_size)
		fprintf(fd, "\tmovq %s, (%s)\n", reg_name(assign_from), reg_name(assign_to));
	else {
		fprintf(stderr, "Internal error: %ld size passed to assign_dereference. No size handler found.\n", assign_from->size);
		exit(1);
	}
}
