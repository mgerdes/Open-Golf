#include "golf/scripting.h"

#include <stdio.h>

#include "picoc/picoc.h"
#include "golf/file.h"
#include "golf/string.h"

void golf_scripting_init(void) {
    Picoc pc;
    PicocInitialise(&pc, 1024*1024);

    if (PicocPlatformSetExitPoint(&pc)) {
        PicocCleanup(&pc);
        printf("EXIT\n");
        return;
    }

    char *data;
    int data_len;
    if (golf_file_load_data("data/scripts/math.c", &data, &data_len)) {
        PicocParse(&pc, "nofile", data, data_len, true, true, false, false);
        golf_free(data);
    }
    if (golf_file_load_data("data/scripts/test.c", &data, &data_len)) {
        PicocParse(&pc, "nofile", data, data_len, true, true, false, false);
        golf_free(data);
    }

    if (VariableDefined(&pc, TableStrRegister(&pc, "generate"))) {
        struct Value *val = NULL;
        VariableGet(&pc, NULL, TableStrRegister(&pc, "generate"), &val);

        golf_string_t call;
        golf_string_initf(&call, "generate(");
        if (val->Typ->Base == TypeFunction) {
            struct FuncDef *def = &val->Val->FuncDef;
            for (int i = 0; i < def->NumParams; i++) {
                enum BaseType type = def->ParamType[i]->Base;
                switch (type) {
                    case TypeInt: {
                        golf_string_appendf(&call, "%d", i);
                        break;
                    }
                    case TypeFP: {
                        golf_string_appendf(&call, "%f", (float)i);
                        break;
                    }
                    case TypeStruct: {
                        if (strcmp(def->ParamType[i]->Identifier, "vec2") == 0) {
                            golf_string_appendf(&call, "V2(%f, %f)", (float)i, (float)i);
                        }
                        else if (strcmp(def->ParamType[i]->Identifier, "vec3") == 0) {
                            golf_string_appendf(&call, "V3(%f, %f, %f)", (float)i, (float)i, (float)i);
                        }
                        else {
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
                if (i + 1 < def->NumParams) {
                    golf_string_appendf(&call, ", ");
                }
            }
        }
        golf_string_appendf(&call, ");");
        printf("%s\n", call.cstr);
        PicocParse(&pc, "nofile", call.cstr, data_len, true, true, false, false);
    }
}
