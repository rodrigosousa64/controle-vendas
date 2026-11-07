#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper interno para preencher uma struct Product a partir de uma linha do SQLite */
static void preencher_produto_stmt(sqlite3_stmt *stmt, Product *p) {
    p->id = sqlite3_column_int64(stmt, 0);
    snprintf(p->sku, sizeof(p->sku), "%s", (const char *)sqlite3_column_text(stmt, 1));
    snprintf(p->name, sizeof(p->name), "%s", (const char *)sqlite3_column_text(stmt, 2));
    snprintf(p->category, sizeof(p->category), "%s", (const char *)sqlite3_column_text(stmt, 3));
    p->price_cents = sqlite3_column_int64(stmt, 4);
    p->stock = sqlite3_column_int(stmt, 5);
    p->min_stock = sqlite3_column_int(stmt, 6);
    p->active = sqlite3_column_int(stmt, 7);
    const char *notes = (const char *)sqlite3_column_text(stmt, 8);
    snprintf(p->notes, sizeof(p->notes), "%s", notes ? notes : "");
}

/*
 * Lista todos os produtos cadastrados ordenados pelo nome.
 * REQUISITO 7: Alocação dinâmica de memória com realloc/malloc e verificação de ponteiro.
 */
int model_listar_produtos(Product **produtos, size_t *total) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "SELECT id, sku, name, category, price_cents, stock, min_stock, active, notes "
                      "FROM products ORDER BY name COLLATE NOCASE, id;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    size_t capacidade = 16;
    size_t count = 0;
    Product *lista = malloc(capacidade * sizeof(Product));
    if (!lista) {
        sqlite3_finalize(stmt);
        return MODEL_ERRO_GERAL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacidade) {
            capacidade *= 2;
            Product *temp = realloc(lista, capacidade * sizeof(Product));
            if (!temp) {
                free(lista);
                sqlite3_finalize(stmt);
                return MODEL_ERRO_GERAL;
            }
            lista = temp;
        }
        preencher_produto_stmt(stmt, &lista[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *produtos = lista;
    *total = count;
    return MODEL_OK;
}

/*
 * Busca um único produto pelo seu identificador primário.
 */
int model_buscar_produto_por_id(int64_t id, Product *produto) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "SELECT id, sku, name, category, price_cents, stock, min_stock, active, notes "
                      "FROM products WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        preencher_produto_stmt(stmt, produto);
        sqlite3_finalize(stmt);
        return MODEL_OK;
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? MODEL_NAO_ENCONTRADO : MODEL_ERRO_GERAL;
}

/*
 * Insere um novo produto no banco de dados.
 * Trata SKU duplicado retornando MODEL_DUPLICADO (Requisito 9).
 */
int model_inserir_produto(Product *produto) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "INSERT INTO products (sku, name, category, price_cents, stock, min_stock, active, notes) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    sqlite3_bind_text(stmt, 1, produto->sku, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, produto->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, produto->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, produto->price_cents);
    sqlite3_bind_int(stmt, 5, produto->stock);
    sqlite3_bind_int(stmt, 6, produto->min_stock);
    sqlite3_bind_int(stmt, 7, produto->active);
    sqlite3_bind_text(stmt, 8, produto->notes, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return MODEL_DUPLICADO;
    }
    if (rc != SQLITE_DONE) {
        return MODEL_ERRO_GERAL;
    }

    produto->id = sqlite3_last_insert_rowid(db);
    return MODEL_OK;
}

/*
 * Atualiza os dados de um produto existente.
 */
int model_atualizar_produto(int64_t id, const Product *produto) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "UPDATE products SET sku=?, name=?, category=?, price_cents=?, "
                      "stock=?, min_stock=?, active=?, notes=?, "
                      "updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    sqlite3_bind_text(stmt, 1, produto->sku, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, produto->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, produto->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, produto->price_cents);
    sqlite3_bind_int(stmt, 5, produto->stock);
    sqlite3_bind_int(stmt, 6, produto->min_stock);
    sqlite3_bind_int(stmt, 7, produto->active);
    sqlite3_bind_text(stmt, 8, produto->notes, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, id);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) return MODEL_DUPLICADO;
    if (rc != SQLITE_DONE) return MODEL_ERRO_GERAL;
    if (changes == 0) return MODEL_NAO_ENCONTRADO;

    return MODEL_OK;
}

/*
 * Exclui um produto do banco de dados.
 * Caso o produto possua vendas associadas no histórico, o banco rejeita
 * via chave estrangeira e retornamos MODEL_CONFLITO.
 */
int model_excluir_produto(int64_t id) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "DELETE FROM products WHERE id=?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return MODEL_CONFLITO; /* Possui itens de venda vinculados */
    }
    if (rc != SQLITE_DONE) {
        return MODEL_ERRO_GERAL;
    }
    if (changes == 0) {
        return MODEL_NAO_ENCONTRADO;
    }

    return MODEL_OK;
}
