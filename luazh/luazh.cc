#include <cstdio>
#include <cstdlib>

#include <sstream>
#include <fstream>
#include <zlib.h>

#include "../luaw/luaw.hh"

void help(const char* program)
{
    printf("%s HEADER_NAME [-s] LUA_FILES...\n", program);
    printf("Generate Lua bytecode, compress it and generate C header.\n");
    printf("Can be loaded using luaw library (https://github.com/andrenho/luaw).\n");
    printf("Program arguments:   -s    Strip debugging information.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
        help(argv[0]);

    bool strip = false;

    printf("struct LuaCompressedBytecode { unsigned long c, u; const char* f; unsigned char* data; } %s[] = {\n", argv[1]);


    // 1. read lua files, concatenating them

    std::stringstream ss;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-s") {
            strip = true;
            continue;
        }

        std::ifstream ifs(argv[i]);
        if (!ifs.good()) {
            fprintf(stderr, "Could not open file '%s'.\n", argv[i]);
            exit(EXIT_FAILURE);
        }
        ss << ifs.rdbuf();
        std::string code = ss.str();

        // 2. generate bytecode
        lua_State* L = luaw_newstate();

        luaw_do(L, "return string.dump", 1);

        if (luaL_loadbuffer(L, code.data(), code.size(), argv[i]) != LUA_OK) {
            fprintf(stderr, "error on '%s': %s\n", argv[i], lua_tostring(L, -1));
            exit(EXIT_FAILURE);
        }

        luaw_push(L, strip);
        lua_call(L, 2, 1);

        size_t bytecode_len = luaw_len(L, -1);
        uint8_t* bytecode = (uint8_t *) lua_tolstring(L, -1, &bytecode_len);

        // test bytecode
#if 0
        luaL_loadbuffer(L, (char const *) bytecode, bytecode_len, "test");
        lua_call(L, 0, 0);
#endif

        // 3. compress bytecode

        uLongf compressed_len = compressBound(bytecode_len);
        auto compressed = (Bytef *) malloc(bytecode_len);
        compress2(compressed, &compressed_len, bytecode, bytecode_len, Z_BEST_COMPRESSION);

        // 4. generate file in header

        printf("  { %zu, %zu, \"%s\", (unsigned char []) { ", compressed_len, bytecode_len, argv[i]);
        for (size_t j = 0; j < compressed_len; ++j) {
            printf("0x%02x, ", compressed[j]);
        }
        printf("} },\n");

    }

    // 4. generate headers

    printf("  { 0, 0, nullptr, {} }\n");
    printf("};\n");
}