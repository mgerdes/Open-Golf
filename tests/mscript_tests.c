#include "mscript/parser.h"

#include <assert.h>

void mscript_tests_run() {
    mscript_t *mscript = mscript_create("tests/scripts");
    mscript_program_t *mscript_program = mscript_get_program(mscript, "tests.mscript");
    mscript_vm_t *mscript_vm = mscript_vm_create(mscript_program);
    char *mscript_vm_stack = mscript_vm_get_stack(mscript_vm);

    {
        mscript_val_t args[2];
        args[0] = mscript_val_int(7);
        args[1] = mscript_val_int(15);
        mscript_vm_run(mscript_vm, "int_addition", 2, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 7 + 15);
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_int(7);
        args[1] = mscript_val_int(15);
        mscript_vm_run(mscript_vm, "int_subtraction", 2, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 7 - 15);
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_float(7.0f);
        args[1] = mscript_val_float(15.0f);
        mscript_vm_run(mscript_vm, "float_addition", 2, args);
        float result = *(float*) mscript_vm_stack;
        assert(result == 7.0f + 15.0f);
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_float(7.0f);
        args[1] = mscript_val_float(15.0f);
        mscript_vm_run(mscript_vm, "float_subtraction", 2, args);
        float result = *(float*) mscript_vm_stack;
        assert(result == 7.0f - 15.0f);
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec2(7.0f, 15.0f);
        args[1] = mscript_val_vec2(5.0f, 19.0f);
        mscript_vm_run(mscript_vm, "vec2_addition", 2, args);
        vec2 result = *(vec2*) mscript_vm_stack;
        assert((result.x == 7.0f + 5.0f) && (result.y == 15.0f + 19.0f));
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec2(7.0f, 15.0f);
        args[1] = mscript_val_vec2(5.0f, 19.0f);
        mscript_vm_run(mscript_vm, "vec2_subtraction", 2, args);
        vec2 result = *(vec2*) mscript_vm_stack;
        assert((result.x == 7.0f - 5.0f) && (result.y == 15.0f - 19.0f));
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec2(7.0f, 15.0f);
        args[1] = mscript_val_float(5.0f);
        mscript_vm_run(mscript_vm, "vec2_scale", 2, args);
        vec2 result = *(vec2*) mscript_vm_stack;
        assert((result.x == 7.0f * 5.0f) && (result.y == 15.0f * 5.0f));
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_float(7.0f);
        args[1] = mscript_val_float(15.0f);
        mscript_vm_run(mscript_vm, "vec2_create", 2, args);
        vec2 result = *(vec2*) mscript_vm_stack;
        assert(result.x == 7.0f && result.y == 15.0f);
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec3(7.0f, 15.0f, 23.0f);
        args[1] = mscript_val_vec3(5.0f, 19.0f, 17.0f);
        mscript_vm_run(mscript_vm, "vec3_addition", 2, args);
        vec3 result = *(vec3*) mscript_vm_stack;
        assert((result.x == 7.0f + 5.0f) && (result.y == 15.0f + 19.0f) && (result.z == 23.0f + 17.0f));
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec3(7.0f, 15.0f, 23.0f);
        args[1] = mscript_val_vec3(5.0f, 19.0f, 17.0f);
        mscript_vm_run(mscript_vm, "vec3_subtraction", 2, args);
        vec3 result = *(vec3*) mscript_vm_stack;
        assert((result.x == 7.0f - 5.0f) && (result.y == 15.0f - 19.0f) && (result.z == 23.0f - 17.0f));
    }

    {
        mscript_val_t args[2];
        args[0] = mscript_val_vec3(7.0f, 15.0f, 23.0f);
        args[1] = mscript_val_float(5.0f);
        mscript_vm_run(mscript_vm, "vec3_scale", 2, args);
        vec3 result = *(vec3*) mscript_vm_stack;
        assert((result.x == 7.0f * 5.0f) && (result.y == 15.0f * 5.0f) && (result.z == 23.0f * 5.0f));
    }

    {
        mscript_val_t args[3];
        args[0] = mscript_val_float(7.0f);
        args[1] = mscript_val_float(15.0f);
        args[2] = mscript_val_float(23.0f);
        mscript_vm_run(mscript_vm, "vec3_create", 3, args);
        vec3 result = *(vec3*) mscript_vm_stack;
        assert(result.x == 7.0f && result.y == 15.0f && result.z == 23.0f);
    }

    {
        mscript_val_t args[1];
        args[0] = mscript_val_int(10);
        mscript_vm_run(mscript_vm, "fib", 1, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 55);
    }

    {
        mscript_val_t args[1];
        args[0] = mscript_val_int(10);
        mscript_vm_run(mscript_vm, "array_1", 1, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 10);
    }

    {
        mscript_val_t args[1];
        args[0] = mscript_val_int(10);
        mscript_vm_run(mscript_vm, "array_2", 1, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 45);
    }

    {
        mscript_val_t args[1];
        args[0] = mscript_val_int(10);
        mscript_vm_run(mscript_vm, "global_1", 1, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 10);
    }

    {
        mscript_val_t args[1];
        args[0] = mscript_val_int(5);
        mscript_vm_run(mscript_vm, "global_1", 1, args);
        int result = *(int*) mscript_vm_stack;
        assert(result == 15);
    }
}

