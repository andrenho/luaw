#include <cstdio>
#include <cstdlib>

#include <sstream>
#include <fstream>
#include <zlib.h>

#include "../luaw/luaw.hh"

void help(const char* program)
{
    printf("%s [-s] LUA_FILES...\n", program);
    printf("Generate Lua bytecode, compress it and generate C header.\n");
    printf("Can be loaded using luaw library (https://github.com/andrenho/luaw).\n");
    printf("Program arguments:   -s    Strip debugging information.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
        help(argv[0]);

    bool strip = false;
    std::string header_name;

    // 1. read lua files, concatenating them

    std::stringstream ss;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-s") {
            strip = true;
            continue;
        }

        if (header_name.empty()) {
            header_name = std::string(argv[i]);
            std::replace(header_name.begin(), header_name.end(), '.', '_');
            size_t pos = header_name.rfind('/');
            if (pos != std::string::npos)
                header_name = header_name.substr(pos + 1);
        }

        std::ifstream ifs(argv[i]);
        if (!ifs.good()) {
            fprintf(stderr, "Could not open file '%s'.\n", argv[i]);
            exit(EXIT_FAILURE);
        }
        ss << ifs.rdbuf() << "\n";
    }
    std::string code = ss.str();

    // 2. generate bytecode
    lua_State* L = luaw_newstate();

    luaw_do(L, "return string.dump", 1);

    if (luaL_loadbuffer(L, code.data(), code.size(), "bytecode") != LUA_OK) {
        fprintf(stderr, "error on source files: %s\n", lua_tostring(L, -1));
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

    // 4. generate headers

    printf("#define %s_len %zu\n", header_name.c_str(), compressed_len);
    printf("static const unsigned char %s_zbytecode[] = {\n", header_name.c_str());
    for (size_t i = 0; i < compressed_len; ++i) {
        printf("0x%02x, ", compressed[i]);
        if ((i+1) % 16 == 0)
            printf("\n");
    }
    printf("};\n");

    fprintf(stderr, "Bytecode compressed generated with %zu bytes (%zu bytes before compression).\n",
            compressed_len, bytecode_len);
}