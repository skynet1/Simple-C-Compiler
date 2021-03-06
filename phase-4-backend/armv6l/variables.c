#define IN_BACKEND
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generator/types.h"
#include "generator/generator.h"
#include "utilities/types.h"
#include "backend/registers.h"
#include "backend/errors.h"
#include "globals.h"
#include "backend/registers.h"
#include "types.h"

static int current_L=0;
static struct global_var {
	char *name;
	int num;
} **global_vars=NULL;
static int num_global_vars=0;
void backend_make_global_var(FILE *fd, struct var_t *v)
{
	fprintf(fd, ".comm %s, 4, 4\n", v->name);
	fprintf(fd, ".L%d:\n\t.word\t%s\n", current_L, v->name);
	num_global_vars++;
	global_vars=realloc(global_vars, num_global_vars*sizeof(struct global_var*));
	global_vars[num_global_vars-1]=malloc(sizeof(struct global_var));
	global_vars[num_global_vars-1]->name=strdup(v->name);
	global_vars[num_global_vars-1]->num=current_L;
	current_L++;

}

static void inc_by_int(FILE *fd, int i, char *dest, size_t size)
{
	fprintf(fd, "\tadd %s, %s, #%d\n", dest, dest, i);
}

void get_address(FILE *fd, struct expr_t *_var)
{
	struct reg_t *ret=get_ret_register(pointer_size, false);
	if (_var->kind==var) {
		fprintf(fd, "\tmov %s, fp\n", get_reg_name(ret, pointer_size));
		inc_by_int(fd, _var->attrs.var->offset, get_reg_name(ret, pointer_size), pointer_size);
	}
}

static inline void print_assign_var(FILE *fd, char *operator, struct reg_t *reg, struct var_t *var)
{
	fprintf(fd, "\t%s %s, var$%s$%d(%%rbp)\n", operator, reg_name(reg), var->name, var->scope_depth);
}

void assign_var(FILE *fd, struct reg_t *src, struct var_t *dest)
{
	if (dest->is_register) {
		fprintf(fd, "\tmov %s, %s\n", reg_name(src), reg_name(dest->reg));
		return;
	}
			
	if (src->use==FLOAT || src->use==FLOAT_RET) {
		char *name=reg_name(src);
		if (dest->scope_depth!=0) {
			fprintf(fd, "\tcvtsd2ss %s, %s\n", name, name);
			fprintf(fd, "\tmovss %s, %d(%%rbp)\n", name, dest->offset);
		} else {
			int x;
			for (x=0; x<num_global_vars; x++)
				if (!strcmp(global_vars[x]->name, dest->name))
					break;
			depth++;
			struct reg_t *r=get_free_register(fd, get_type_size(dest->type), depth, false);
			fprintf(fd, "\tldr %s, .L%d\n", reg_name(r), global_vars[x]->num);
			fprintf(fd, "\tstr %s, [%s]\n", reg_name(src), reg_name(r));
			free_register(fd, r);
			depth--;
		}
		return;
	}

	if (dest==NULL) {
		fprintf(stderr, "Internal error: a NULL variable pointer was passed to assign_var\n");
		exit(1);
	}

	size_t size=get_type_size(dest->type);
	if (dest->scope_depth!=0) {
		if (size==char_size)
			fprintf(fd, "\tstrb\tr0, [fp, #%zd]\n", dest->offset);
		else if (size==word_size)
			fprintf(fd, "\tstr\tr0, [fp, #%zd]\n", dest->offset);
		else
			error(__func__, size);
	} 
} 

static inline void print_read_var(FILE *fd, char *operator, char *reg, struct var_t *var)
{
	fprintf(fd, "\t%s var$%s$%d(%%rbp), %s\n", operator, var->name, var->scope_depth, reg);
}

void read_var(FILE *fd, struct var_t *v)
{
	get_ret_register(word_size, false)->is_signed=is_signed(v->type);
	if (v->is_register) {
		fprintf(fd, "\tmov %s, r0\n", reg_name(v->reg));
		return;
	}

	if (v->scope_depth!=0) {
		size_t size=get_type_size(v->type);
		if (size==char_size)
			fprintf(fd, "\tldrb\tr0, [fp, #%zd]\n", v->offset);
		else if (size==word_size)
			fprintf(fd, "\tldr\tr0, [fp, #%zd]\n", v->offset);
		else
			size_error(size);

	} 
}

void dereference(FILE *fd, struct reg_t *reg, size_t size)
{
	fprintf(fd, "\tldr r0, [%s]\n", reg_name(reg));
}

void assign_dereference(FILE *fd, struct reg_t *assign_from, struct reg_t *assign_to)
{

		fprintf(fd, "\tstr %s, [%s, #0]\n", reg_name(assign_from), reg_name(assign_to));
}
