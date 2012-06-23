#include <stdlib.h>
#include <stdbool.h>
#include "../../dbg.h"
#include "../../mem.h"

#include "node.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

//==============================================================================
//
// Functions
//
//==============================================================================

// Creates an AST node for a function.
//
// name        - The name of the function.
// return_type - The data type that the function returns.
// args        - The arguments of the function.
// arg_count   - The number of arguments the function has.
// body        - The contents of the function.
// ret         - A pointer to where the ast node will be returned.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_create(bstring name, bstring return_type,
                            struct eql_ast_node **args, unsigned int arg_count,
                            struct eql_ast_node *body,
                            struct eql_ast_node **ret)
{
    eql_ast_node *node = malloc(sizeof(eql_ast_node)); check_mem(node);
    node->type = EQL_AST_TYPE_FUNCTION;
    node->parent = NULL;
    node->function.name = bstrcpy(name);
    if(name) check_mem(node->function.name);
    node->function.return_type = bstrcpy(return_type);
    if(return_type) check_mem(node->function.return_type);

    // Copy arguments.
    if(arg_count > 0) {
        size_t sz = sizeof(eql_ast_node*) * arg_count;
        node->function.args = malloc(sz);
        check_mem(node->function.args);
        
        unsigned int i;
        for(i=0; i<arg_count; i++) {
            node->function.args[i] = args[i];
            args[i]->parent = node;
        }
    }
    else {
        node->function.args = NULL;
    }
    node->function.arg_count = arg_count;
    
    // Assign function body.
    node->function.body = body;
    if(body != NULL) {
        body->parent = node;
    }

    *ret = node;
    return 0;

error:
    eql_ast_node_free(node);
    (*ret) = NULL;
    return -1;
}

// Frees a variable declaration AST node from memory.
//
// node - The AST node to free.
void eql_ast_function_free(struct eql_ast_node *node)
{
    if(node->function.name) bdestroy(node->function.name);
    node->function.name = NULL;

    if(node->function.return_type) bdestroy(node->function.return_type);
    node->function.return_type = NULL;
    
    if(node->function.arg_count > 0) {
        unsigned int i;
        for(i=0; i<node->function.arg_count; i++) {
            eql_ast_node_free(node->function.args[i]);
            node->function.args[i] = NULL;
        }
        free(node->function.args);
        node->function.arg_count = 0;
    }

    if(node->function.body) eql_ast_node_free(node->function.body);
    node->function.body = NULL;
}


//--------------------------------------
// Codegen
//--------------------------------------

// Recursively generates LLVM code for the function AST node.
//
// node    - The node to generate an LLVM value for.
// module  - The compilation unit this node is a part of.
// value   - A pointer to where the LLVM value should be returned.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_codegen(eql_ast_node *node, eql_module *module,
                             LLVMValueRef *value)
{
    int rc;
    unsigned int i;

    // Find the class this function belongs to, if any.
    eql_ast_node *class_ast = NULL;
    rc = eql_ast_function_get_class(node, &class_ast);
    check(rc == 0, "Unable to retrieve parent class for function");

    // Function name should be prepended with the class name if this is a method.
    bool is_method = (class_ast != NULL);
    bstring function_name;
    if(is_method) {
        check(blength(class_ast->class.name) > 0, "Class name required for method");
        function_name = bformat("%s___%s", bdata(class_ast->class.name), bdata(node->function.name));
        check_mem(function_name);
    }
    else {
        function_name = bstrcpy(node->function.name);
        check_mem(function_name);
    }

    // Create a list of function argument types.
    eql_ast_node *arg;
    unsigned int arg_count = node->function.arg_count;
    LLVMTypeRef *params = malloc(sizeof(LLVMTypeRef) * arg_count);

    // Create arguments.
    for(i=0; i<arg_count; i++) {
        arg = node->function.args[i];
        rc = eql_module_get_type_ref(module, arg->farg.var_decl->var_decl.type, NULL, &params[i]);
        check(rc == 0, "Unable to determine function argument type");
    }

    // Determine return type.
    LLVMTypeRef return_type;
    rc = eql_module_get_type_ref(module, node->function.return_type, NULL, &return_type);
    check(rc == 0, "Unable to determine function return type");

    // Create function type.
    LLVMTypeRef funcType = LLVMFunctionType(return_type, params, arg_count, false);
    check(funcType != NULL, "Unable to create function type");

    // Create function.
    LLVMValueRef func = LLVMAddFunction(module->llvm_module, bdata(function_name), funcType);
    check(func != NULL, "Unable to create function");
    
    // Store the current function on the module.
    module->llvm_function = func;
    rc = eql_module_push_scope(module, node);
    check(rc == 0, "Unable to add function scope");

    // Assign names to function arguments.
    for(i=0; i<arg_count; i++) {
        arg = node->function.args[i];
        LLVMValueRef param = LLVMGetParam(func, i);
        LLVMSetValueName(param, bdata(arg->farg.var_decl->var_decl.name));
    }

    // Generate body.
    LLVMValueRef body;
    rc = eql_ast_node_codegen(node->function.body, module, &body);
    check(rc == 0, "Unable to generate function body");
    
    // Dump before verification.
    // LLVMDumpValue(func);
    
    // Verify function.
    rc = LLVMVerifyFunction(func, LLVMPrintMessageAction);
    check(rc != 1, "Invalid function");

    // Unset the current function.
    rc = eql_module_pop_scope(module, node);
    check(rc == 0, "Unable to remove function scope");
    module->llvm_function = NULL;

    // Return function as a value.
    *value = func;
    
    bdestroy(function_name);
    return 0;

error:
    bdestroy(function_name);

    // Unset the current function.
    module->llvm_function = NULL;
    if(func) LLVMDeleteFunction(func);
    *value = NULL;
    return -1;
}

// Generates the allocas for the function arguments. This has to be called
// from the block since that is where the entry block is created.
//
// node    - The function node.
// module  - The compilation unit this node is a part of.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_codegen_args(eql_ast_node *node, eql_module *module)
{
    int rc;
    unsigned int i;
    
    check(node != NULL, "Node required");
    check(node->type == EQL_AST_TYPE_FUNCTION, "Node type expected to be 'function'");
    check(module != NULL, "Module required");

    LLVMBuilderRef builder = module->compiler->llvm_builder;

    // Codegen allocas.
    LLVMValueRef *values = malloc(sizeof(LLVMValueRef) * node->function.arg_count);
    check_mem(values);
    
    for(i=0; i<node->function.arg_count; i++) {
        rc = eql_ast_node_codegen(node->function.args[i], module, &values[i]);
        check(rc == 0, "Unable to determine function argument type");
    }
    
    // Codegen store instructions.
    for(i=0; i<node->function.arg_count; i++) {
        LLVMValueRef build_value = LLVMBuildStore(builder, LLVMGetParam(module->llvm_function, i), values[i]);
        check(build_value != NULL, "Unable to create build instruction");
    }
    
    free(values);

    return 0;
    
error:
    if(values) free(values);
    return -1;
}

//--------------------------------------
// Misc
//--------------------------------------

// Retrieves the class that this function belongs to (if it is a method).
// Otherwise it returns NULL as the class.
//
// node      - The function AST node.
// class_ast - A pointer to where the class AST node should be returned to.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_get_class(eql_ast_node *node, eql_ast_node **class_ast)
{
    check(node != NULL, "Node required");
    check(node->type == EQL_AST_TYPE_FUNCTION, "Node type must be 'function'");
    check(class_ast != NULL, "Class return pointer must not be null");
    
    *class_ast = NULL;

    // Check if there is a parent method.
    if(node->parent != NULL && node->parent->type == EQL_AST_TYPE_METHOD) {
        eql_ast_node *method = node->parent;

        // Check if the method has a class.
        if(method->parent != NULL && method->parent->type == EQL_AST_TYPE_CLASS) {
            *class_ast = method->parent;
        }
    }
    
    return 0;
    
error:
    *class_ast = NULL;
    return -1;
}


// Updates the return type of the function based on the last return statement
// of the function. This is used for implicit functions like the main function
// of a module.
//
// node - The function ast node to generate a type for.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_generate_return_type(eql_ast_node *node)
{
    int rc;
    bstring type;
    
    check(node != NULL, "Function required");
    check(node->type == EQL_AST_TYPE_FUNCTION, "Node type must be 'function'");
    
    // If function has no body then its return type is void.
    eql_ast_node *body = node->function.body;
    if(body == NULL) {
        type = bfromcstr("void");
    }
    // Otherwise find the last return statement and determine its type.
    else {
        eql_ast_node *freturn = NULL;
        
        // Loop over all returns and save the last one.
        unsigned int i;
        for(i=0; i<body->block.expr_count; i++) {
            if(body->block.exprs[i]->type == EQL_AST_TYPE_FRETURN) {
                freturn = body->block.exprs[i];
            }
        }
        
        // If there is no return statement or it's a void return then the type
        // is void.
        if(freturn == NULL || freturn->freturn.value == NULL) {
            type = bfromcstr("void");
        }
        // Otherwise check the last return value to determine its type.
        else {
            rc = eql_ast_node_get_type(freturn->freturn.value, &type);
            check(rc == 0, "Unable to determine return type");
        }
    }
    
    // Assign type to return type.
    node->function.return_type = bstrcpy(type);
    
    return 0;
    
error:
    bdestroy(type);
    return -1;
}

// Searches for variable declarations within the function's argument list.
//
// node     - The node to search within.
// name     - The name of the variable to search for.
// var_decl - A pointer to where the variable declaration should be returned to.
//
// Returns 0 if successful, otherwise returns -1.
int eql_ast_function_get_var_decl(eql_ast_node *node, bstring name,
                                  eql_ast_node **var_decl)
{
    unsigned int i;
    
    check(node != NULL, "Node required");
    check(node->type == EQL_AST_TYPE_FUNCTION, "Node type must be 'function'");

    // Search argument list for variable declaration.
    *var_decl = NULL;
    for(i=0; i<node->function.arg_count; i++) {
        if(biseq(node->function.args[i]->farg.var_decl->var_decl.name, name)) {
            *var_decl = node->function.args[i]->farg.var_decl;
            break;
        }
    }

    return 0;
    
error:
    *var_decl = NULL;
    return -1;    
}


//--------------------------------------
// Debugging
//--------------------------------------

// Append the contents of the AST node to the string.
// 
// node - The node to dump.
// ret  - A pointer to the bstring to concatenate to.
//
// Return 0 if successful, otherwise returns -1.s
int eql_ast_function_dump(eql_ast_node *node, bstring ret)
{
    int rc;
    check(node != NULL, "Node required");
    check(ret != NULL, "String required");

    // Append dump.
    bstring str = bformat("<function name='%s' return-type='%s'>\n", bdata(node->function.name), bdata(node->function.return_type));
    check_mem(str);
    check(bconcat(ret, str) == BSTR_OK, "Unable to append dump");

    // Recursively dump children
    unsigned int i;
    for(i=0; i<node->function.arg_count; i++) {
        rc = eql_ast_node_dump(node->function.args[i], ret);
        check(rc == 0, "Unable to dump function argument");
    }
    /*
    if(node->function.body != NULL) {
        rc = eql_ast_node_dump(node->function.body, ret);
        check(rc == 0, "Unable to dump function body");
    }
    */

    return 0;

error:
    if(str != NULL) bdestroy(str);
    return -1;
}
