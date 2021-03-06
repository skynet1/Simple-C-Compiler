type_with_stars: type | type_with_stars '*' {
	$$=increase_type_depth($1, 1);
};

possibly_blank_ident: IDENTIFIER | %empty {
	$$=NULL;
};
type: TYPE {
	if ($1==NULL) {
		yyerror("type not known\n");
		exit(1);
	}
	free_type(current_type);
	current_type=$1;
	current_type->refcount+=2;
	$$=$1;
} | STRUCT possibly_blank_ident '{' struct_var_declarations '}' {
	/* TODO: check to see if there's already a struct prototyped by this name, and use that instead if it exists. */
	struct type_t *type=malloc(sizeof(struct type_t));
	init_type(type);
	type->fake_kind=_struct;
	type->refcount=2;
	if ($2==NULL) {
		char *name;
		asprintf(&name, "<struct_anonymous%p>", &name);
		type->name=name;
	} else
		type->name=$2;
	type->num_arrays=0;
	type->body=malloc(sizeof(struct tbody_t));

	struct tbody_t *body=type->body;
	body->core_type=_INT;
	int y=0;
	struct statem_t *tmp=$4;
	for (; tmp!=NULL; tmp=tmp->right, y++) {}
	body->attrs.vars.num_vars=y;
	body->attrs.vars.vars=calloc(body->attrs.vars.num_vars, sizeof(struct var_t*));
	size_t size=0;
	body->attrs.vars.alignment=word_size;
	struct statem_t *current=$4;
	size_t alignment=body->attrs.vars.alignment;
	for (y=0; current!=NULL; current=current->right, y++) {
		assert(current->left->kind==declare);
		struct var_t *var=current->left->attrs.var;
		int s=get_type_size(var->type);
		if (s<alignment)
			size+=alignment;
		else
			size+=s;
		body->attrs.vars.vars[y]=var;
		var->refcount=1;

	}

	/*TODO: free struct_var_declarations */

	body->kind=_struct;
	body->size=size;
	add_type(type);
	$$=type;
	current_type=type;
} | UNION IDENTIFIER '{' struct_var_declarations '}' {
	struct type_t *type=malloc(sizeof(struct type_t));
	init_type(type);
	type->fake_kind=_union;
	type->refcount=2;
	type->num_arrays=0;
	type->name=strdup($2);
	struct tbody_t *body=type->body=malloc(sizeof(struct tbody_t));
	body->core_type=_INT;
	body->refcount=1;

	body->kind=_union;
	body->is_func_pointer=false;

	body->attrs.vars.num_vars=0;
	body->attrs.vars.vars=NULL;
	register int x;
	size_t max_size=0;
	struct var_t **v=body->attrs.vars.vars;
	int num_vars=body->attrs.vars.num_vars;
	struct statem_t *current=$4;
	for (; current!=NULL; current=current->right) {
		size_t s=get_type_size(current->left->attrs.var->type);
		num_vars++;
		v=realloc(v, num_vars*sizeof(struct var_t*));
		v[num_vars-1]=malloc(sizeof(struct var_t));

		v[num_vars-1]=current->left->attrs.var;
		v[num_vars-1]->refcount=1;
		if (s>=max_size)
			max_size=s;
	}
	type->body->attrs.vars.num_vars=num_vars;
	type->body->attrs.vars.vars=v;

	type->body->size=max_size;
	current_type=type;
	current_type->refcount++;
	add_type(type);
	free($2);
	$$=type;
} | STRUCT IDENTIFIER {
	free_type(current_type);
	$$=current_type=get_type_by_name($2, _struct);
	$$->refcount+=2;
	free($2);
} | ENUM '{' enum_elements '}' {
	free_type(current_type);
	int x;
	for (x=0; $3[x]!=NULL; x++) {}
	int num_enums=++x;
	struct type_t *type=malloc(sizeof(struct type_t));
	init_type(type);
	type->is_signed=false;
	type->refcount=2;

	struct tbody_t *bod=type->body=malloc(sizeof(struct tbody_t));
	bod->refcount=1;
	bod->kind=_enum;
	bod->is_func_pointer=false;
	bod->size=num_enums%byte_size>0 ? ((num_enums-(num_enums%byte_size))+byte_size)/byte_size : num_enums/byte_size;
	bod->core_type=_INT;

	asprintf(&(type->name), "<enum_%p>", $3);
	type->native_type=false;

	add_type(type);
	struct const_t **consts=calloc(num_enums, sizeof(struct const_t*));

	for (x=0; $3[x]!=NULL; x++) {
		struct expr_t *e=const_int_expr($3[x]->i >=0 ? $3[x]->i : 1 << x, type);
		add_constant($3[x]->name, scope_depth, e);
	}

	for (x=0; $3[x]!=NULL; x++)
		free($3[x]);
	free($3);
	current_type=type;
	type->refcount++;
	$$=type;
} | ENUM IDENTIFIER '{' enum_elements '}' {
	free_type(current_type);
	int x;
	for (x=0; $4[x]!=NULL; x++) {}
	int num_enums=++x;
	struct type_t *type=malloc(sizeof(struct type_t));
	init_type(type);
	type->fake_kind=_enum;
	type->is_signed=false;
	type->refcount=2;

	struct tbody_t *bod=type->body=malloc(sizeof(struct tbody_t));
	bod->refcount=1;
	bod->kind=_enum;
	bod->is_func_pointer=false;
	bod->size=num_enums%byte_size>0 ? ((num_enums-(num_enums%byte_size))+byte_size)/byte_size : num_enums/byte_size;
	bod->core_type=_INT;

	type->name=$2;
	type->native_type=false;

	add_type(type);
	struct const_t **consts=calloc(num_enums, sizeof(struct const_t*));

	for (x=0; $4[x]!=NULL; x++) {
		struct expr_t *e=const_int_expr($4[x]->i >=0 ? $4[x]->i : 1 << x, type);
		add_constant($4[x]->name, scope_depth, e);
	}

	for (x=0; $4[x]!=NULL; x++)
		free($4[x]);
	free($4);
	current_type=type;
	type->refcount++;
	$$=type;
} | ENUM IDENTIFIER {
	free_type(current_type);
	current_type=$$=get_type_by_name($2, _enum);
	current_type->refcount+=2;
	if ($$==NULL) {
		yyerror("Error: enumeration not known.");
		exit(1);
	}
} | LONG type {
	if (get_type_size($2)==int_size)
		$$=get_type_by_name("long", _normal);
} | LONG {
	$$=get_type_by_name("long", _normal);
} | UNSIGNED type {
	$$=malloc(sizeof(struct type_t));
	init_type($$);
	memcpy($$, $2, sizeof(struct type_t));
	asprintf(&($$->name), "unsigned %s", $2->name);
	$$->is_signed=false;
	$$->native_type=false;
	$$->refcount=1;
	$$->body->refcount++;
} | SIGNED type {
	$$=copy_type($2);
	free($$->name);
	asprintf(&($$->name), "signed %s", $2->name);
	$$->is_signed=true;
} | SHORT type {
	$$=copy_type($2);
	free($$->name);
	asprintf(&($$->name), "signed %s", $2->name);
	$$->body=copy_body($$->body);
	$$->body->size/=2;
} | CONST type {
	$$=copy_type($2);
	free($$->name);
	asprintf(&($$->name), "const %s", $2->name);
	$$->is_constant=true;
	$$->refcount++;
	current_type=$$;
};

enum_elements: enum_element {
	$$=calloc(2, sizeof(struct enum_element*));
	$$[0]=$1;
	$$[1]=NULL;
} | enum_elements ',' enum_element {
	/* TODO: finish adding enumerations. */
	$$=$1;
	int x;
	for (x=0; $1[x]!=NULL; x++) {
	}
	$$=realloc($$, (x+2)*sizeof(struct enum_element*));
	$$[x]=$3;
	$$[x+1]=NULL;
};

enum_element: IDENTIFIER {
	$$=malloc(sizeof(struct enum_element));
	$$->name=$1;
	$$->i=-1;
} | IDENTIFIER '=' noncomma_expression {
	if ($3->kind!=const_int) {
		yyerror("Only constant integers are allowed for enumeration value specifications.");
		exit(1);
	}
	$$=malloc(sizeof(struct enum_element));
	$$->name=$1;
	$$->i=$3->attrs.cint_val;
	free_expr($3);
}; 
