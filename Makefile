CC ?= cc
CPPFLAGS = -Iinclude -Ivendor/sqlite -Ivendor/cjson -Ivendor/civetweb/include -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS = -lm -lpthread -ldl
BUILD_DIR ?= build

# Todos os fontes organizados em MVT
SOURCES = \
    src/main.c \
    src/server/http.c \
    src/server/router.c \
    src/server/utils.c \
    src/models/database.c \
    src/models/produto_model.c \
    src/models/venda_model.c \
    src/views/produtos/listar_produtos.c \
    src/views/produtos/buscar_produto.c \
    src/views/produtos/cadastrar_produto.c \
    src/views/produtos/atualizar_produto.c \
    src/views/produtos/excluir_produto.c \
    src/views/vendas/listar_vendas.c \
    src/views/vendas/buscar_venda.c \
    src/views/vendas/criar_venda.c \
    src/views/vendas/cancelar_venda.c \
    src/views/sistema/health.c \
    src/views/sistema/encerrar.c

OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all run demo test clean sanitize package

all: $(BUILD_DIR)/vendas

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) data
	mkdir -p $(BUILD_DIR)/server $(BUILD_DIR)/models $(BUILD_DIR)/views/produtos $(BUILD_DIR)/views/vendas $(BUILD_DIR)/views/sistema

$(BUILD_DIR)/%.o: src/%.c include/app.h include/models.h include/http.h include/views.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sqlite3.o: vendor/sqlite/sqlite3.c vendor/sqlite/sqlite3.h | $(BUILD_DIR)
	$(CC) -std=c11 -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_DQS=0 -DSQLITE_OMIT_LOAD_EXTENSION -c $< -o $@

$(BUILD_DIR)/cJSON.o: vendor/cjson/cJSON.c vendor/cjson/cJSON.h | $(BUILD_DIR)
	$(CC) -std=c11 -O2 -c $< -o $@

$(BUILD_DIR)/civetweb.o: vendor/civetweb/src/civetweb.c vendor/civetweb/include/civetweb.h | $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Ivendor/civetweb/include -D_POSIX_C_SOURCE=200809L -DUSE_IPV6 -UUSE_IPV6 -DNO_SSL -c $< -o $@

$(BUILD_DIR)/vendas: $(OBJECTS) $(BUILD_DIR)/sqlite3.o $(BUILD_DIR)/cJSON.o $(BUILD_DIR)/civetweb.o
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

run: all
	./build/vendas

demo: all
	./build/vendas --demo

test: all
	python3 tests/test_api.py

sanitize:
	$(MAKE) BUILD_DIR=build/sanitized CFLAGS='-std=c11 -g -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
	ASAN_OPTIONS=detect_leaks=1 VENDA_BINARY=build/sanitized/vendas python3 tests/test_api.py

clean:
	rm -rf build/
