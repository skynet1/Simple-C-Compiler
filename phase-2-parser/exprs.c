#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "globals.h"
#include "print-tree.h"
#include "types.h"
#include "optimization-globals.h"
#include "handle-types.h"

bool is_test_op(char *op)
{
	return !strcmp("<", op) || !strcmp(">", op) || !strcmp("<=", op) || !strcmp(">=", op) || !strcmp("!=", op) || !strcmp("==", op);
}

#ifdef DEBUG
void print_expr(char *pre, struct expr_t *e)
{
	if (e==NULL) {
		fprintf(stderr, "(nil)");
		return;
	} 

	if (e->kind!=arg) {
		switch (e->kind) {
		case const_int:
			fprintf(stderr, "%d", e->attrs.cint_val);
			break;
		case bin_op:
			fprintf(stderr, "%s", e->attrs.bin_op);
			break;
		case var:
			fprintf(stderr, "%s", e->attrs.var->name);
			break;
		case pre_un_op:
			fprintf(stderr, "%s", e->attrs.un_op);
			break;
		case post_un_op:
			fprintf(stderr, "%s", e->attrs.un_op);
			break;
		case funccall:
			fprintf(stderr, "%s()", e->attrs.function->name);
			break;
		case const_str:
			fprintf(stderr, "string literal: %s", e->attrs.cstr_val);
			break;
		case const_float:
			fprintf(stderr, "float: %s", e->attrs.cfloat);
			break;
		case convert:
			if (e->type->body->core_type==_INT)
				fprintf(stderr, "conversion type: %s, type_size: %ld, pointer_depth: %ld, core_type: INT", e->type->name, get_type_size(e->type), e->type->pointer_depth);
			else
				fprintf(stderr, "conversion type: %s, type_size: %ld, pointer_depth: %ld, core_type: FLOAT", e->type->name, get_type_size(e->type), e->type->pointer_depth);
		}
		if (e->type->body==NULL) {
			fprintf(stderr, "\n");
			return;
		}
		if (e->type->body->core_type==_INT)
			fprintf(stderr, ", type: %s, type_size: %ld, pointer_depth: %ld, core_type: INT\n", e->type->name, get_type_size(e->type), e->type->pointer_depth);
		else
			fprintf(stderr, ", type: %s, type_size: %ld, pointer_depth: %ld, core_type: FLOAT\n", e->type->name, get_type_size(e->type), e->type->pointer_depth);
	}

	else if (e->kind==arg) {
		fprintf(stderr, "argument: \n", pre);
		char *new_pre=NULL;
		asprintf(&new_pre, "%s |", pre);
		print_tree((__printer_function_t) print_expr, e->attrs.argument, new_pre, offsetof(struct expr_t, left), offsetof(struct expr_t, right));
		free(new_pre);
	} 	
}
#endif

struct expr_t* convert_expr(struct expr_t *e, struct type_t *t)
{
	t->refcount++;
	struct expr_t *ret=malloc(sizeof(struct expr_t));
	ret->kind=convert;
	ret->type=t;
	ret->right=e;
	ret->left=NULL;
	return ret;
}

struct expr_t* const_int_expr(int i, struct type_t *t)
{
	struct expr_t *e=malloc(sizeof(struct expr_t));
	e->kind=const_int;
	e->left=e->right=NULL;
	e->attrs.cint_val=i;
	if (t==NULL) {
		e->type=get_type_by_name("int", _normal);
		e->type->refcount++;
	} else {
		e->type=t;
		t->refcount++;
	}
	return e;
}
void free_expr(struct expr_t *e)
{
	if (e==NULL)
		return;

	switch (e->kind) {
	case bin_op:
		free(e->attrs.bin_op);
		break;

	case pre_un_op:
	case post_un_op:
		free(e->attrs.un_op);
		break;

	case const_str:
		free(e->attrs.cstr_val);
		break;

	case arg:
		free_expr(e->attrs.argument);
		break;

	case var:
		free_var(e->attrs.var);
		break;
	
	}

	if (e->left!=NULL)
		free_expr(e->left);

	if (e->right!=NULL)
		free_expr(e->right);

	free_type(e->type);
	free(e);
}

struct expr_t* copy_expression(struct expr_t *e)
{
	if (e==NULL)
		return NULL;

	struct expr_t *ret=malloc(sizeof(struct expr_t));
	memcpy(ret, e, sizeof(struct expr_t));
	ret->left=copy_expression(e->left);
	ret->right=copy_expression(e->right);

	ret->type->refcount++;

	switch (ret->kind) {
	case bin_op:
		ret->attrs.bin_op=strdup(e->attrs.bin_op);
		break;
	case pre_un_op:
	case post_un_op:
		ret->attrs.un_op=strdup(e->attrs.un_op);
		break;

	case const_str:
		ret->attrs.cstr_val=strdup(e->attrs.cstr_val);
		break;

	case arg:
		ret->attrs.argument=copy_expression(e->attrs.argument);
		break;
	
	case var:
		ret->attrs.var->refcount++;
		break;
	case const_float:
		ret->attrs.cfloat=strdup(e->attrs.cfloat);
		break;

	}
	return ret;
}

bool evaluate_constant_expr(char *op, struct expr_t *a, struct expr_t *b, struct expr_t **e2)
{
	struct expr_t *e=*e2;
	if (a->kind==const_int && b->kind==const_int && evaluate_constants) {
		e->kind=const_int;
		e->left=NULL;
		e->right=NULL;
		long int c=a->attrs.cint_val, d=b->attrs.cint_val, f=0;
		if (!strcmp(op, "+"))
			f=c+d;
		else if (!strcmp(op, "-"))
			f=c-d;
		else if (!strcmp(op, "/")) 
			f=c/d;
			/* NOTE: This might cause problems in the future if
			it's cross-compiling, and the target architecture
			handles integer rounding differently */
		else if (!strcmp(op, "*"))
			f=c*d;
		else if (!strcmp(op, "=="))
			f=c==d;
		else if (!strcmp(op, "<"))
			f=c<d;
		else if (!strcmp(op, ">"))
			f=c>d;
		else if (!strcmp(op, "!="))
			f=c!=d;
		else if (!strcmp(op, ">="))
			f=c>=d;
		else if (!strcmp(op, "<="))
			f=c<=d;
		else if (!strcmp(op, "<<"))
			f=c<<d;
		else if (!strcmp(op, ">>"))
			f=c>>d;
		else if (!strcmp(op, "|"))
			f=c|d;
		else if (!strcmp(op, "&"))
			f=c&d;
		else if (!strcmp(op, "^"))
			f=c^d;
		else if (!strcmp(op, "||"))
			f=c||d;
		else if (!strcmp(op, "&&"))
			f=c&&d;
		else if (!strcmp(op, "%"))
			f=c%d;
		else if (!strcmp(op, ","))
			f=d;
	
		e->attrs.cint_val=f;
		return true;
	} else if (a->kind==const_int && b->kind!=const_int && a->attrs.cint_val==0 && optimize_dont_add_zero && !strcmp(op, "+")) {
		*e2=b;
		free_expr(a);
		return true;
	} else if (a->kind!=const_int && b->kind==const_int && b->attrs.cint_val==0 && optimize_dont_add_zero && !strcmp(op, "+")) {
		*e2=a;
		free_expr(b);
		return true;
	}
	return false;
}

bool is_constant_kind(struct expr_t *e)
{
	return e->kind==const_int || e->kind==const_float || e->kind==const_str;
}


struct expr_t* bin_expr(char *X, struct expr_t *Y, struct expr_t *Z)
{
	/* TODO: Make sure that integers added to pointers get multiplied by the size of the pointer base type */
	if (strlen(X)==2 && X[1]=='=' && X[0]!='='  && X[0]!='<' && X[0]!='>' && X[0]!='!' ) {
		struct expr_t *assignment=malloc(sizeof(struct expr_t));
		struct expr_t *or=malloc(sizeof(struct expr_t));
		assignment->kind=or->kind=bin_op;
		assignment->attrs.bin_op=strdup("=");
		assignment->type=Y->type;
		Y->type->refcount+=2;
		assignment->left=Y;
		assignment->right=or;

		or->attrs.bin_op=calloc(2, sizeof(char));
		or->attrs.bin_op[1]='\0';
		or->attrs.bin_op[0]=X[0];
		or->type=assignment->type;

		or->left=copy_expression(Y);
		or->right=Z;
		return assignment;
	}
	struct expr_t *e=malloc(sizeof(struct expr_t)); 
	struct expr_t *a=Y, *b=Z;
	parser_type_cmp(&a, &b);
	if (!is_test_op(X))
		e->type=b->type;
	else
		e->type=get_type_by_name("int", _normal);
	e->type->refcount++;
	if (!evaluate_constant_expr(X, a, b, &e)) {
		e->kind=bin_op;
		e->left=a;
		e->right=b;
		e->attrs.bin_op=strdup(X);
	}

	return e;
}

struct expr_t* make_prefix_or_postfix_op(char *op, struct expr_t *e, struct type_t *t, bool is_prefix)
{
	if (is_constant_kind(e) && strcmp("-", op)) {
		/*TODO: make the error message easier to read for programmers
		who aren't experts on how compilers work. */
		yyerror("can not do prefix operator on constant expression.");
		exit (1);
	}

	struct expr_t *new=malloc(sizeof(struct expr_t));
	if (is_prefix) {
		new->kind=pre_un_op;
		new->right=e;
		new->left=NULL;
	} else {
		new->kind=post_un_op;
		new->left=e;
		new->right=NULL;
	}
	new->attrs.un_op=strdup(op);
	new->type=t;
	t->refcount++;
	return new;
}