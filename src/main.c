#include "app.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int port = 8080;
    int demo = 0;
    const char *host = "127.0.0.1";
    const char *db_path = "data/vendas.db";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--demo") == 0) {
            demo = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            puts("Uso: ./build/vendas [--port 8080] [--db data/vendas.db] [--demo]\n"
                 "Abra http://127.0.0.1:8080 no navegador.\n"
                 "--demo preenche dados de demonstração caso o banco esteja vazio.");
            return 0;
        } else if (i + 1 < argc && strcmp(argv[i], "--db") == 0) {
            db_path = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--host") == 0) {
            host = argv[++i];
        } else if (i + 1 < argc && strcmp(argv[i], "--port") == 0) {
            char *end = NULL;
            errno = 0;
            long val = strtol(argv[++i], &end, 10);
            if (errno || *end || val < 1024 || val > 65535) {
                fputs("Porta inválida. Escolha entre 1024 e 65535.\n", stderr);
                return 1;
            }
            port = (int)val;
        } else if (i + 1 < argc && strcmp(argv[i], "--web") == 0) {
            /* Mantido para compatibilidade se passado */
            i++;
        } else {
            fprintf(stderr, "Opção inválida: %s. Consulte --help.\n", argv[i]);
            return 1;
        }
    }

    /* REQUISITO 8: Abertura e persistência do banco de dados */
    if (!db_open(db_path)) {
        return 1;
    }

    if (demo && !db_seed()) {
        db_close();
        return 1;
    }

    int result = http_run(host, port);
    db_close();
    return result;
}
