#include "../interpreter.h"

void MsvcSetupFunc(Picoc *pc)
{    
}

void CTest (struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs) 
{
    printf("test(%d)\n", Param[0]->Val->Integer);
    Param[0]->Val->Integer = 1234;
}

void CLineNo (struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs) 
{
    ReturnValue->Val->Integer = Parser->Line;
}

/* list of all library functions and their prototypes */
struct LibraryFunction MsvcFunctions[] =
{
    { CTest,        "void Test(int);" },
    { CLineNo,      "int LineNo();" },
    { NULL,         NULL }
};

void PlatformLibraryInit(Picoc *pc)
{
    IncludeRegister(pc, "picoc_msvc.h", &MsvcSetupFunc, &MsvcFunctions[0], NULL);
}

