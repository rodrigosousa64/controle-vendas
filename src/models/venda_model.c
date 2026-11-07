#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper para liberar a memória alocada dinamicamente de uma venda */
void model_liberar_venda(Sale *venda) {
    if (venda && venda->items) {
        free(venda->items);
        venda->items = NULL;
        venda->item_count = 0;
    }
}

void model_liberar_vendas(Sale *vendas, size_t total) {
    if (!vendas) return;
    for (size_t i = 0; i < total; i++) {
        model_liberar_venda(&vendas[i]);
    }
    free(vendas);
}

/* Helper para carregar os itens pertencentes a uma venda */
static int carregar_itens_da_venda(sqlite3 *db, int64_t sale_id, SaleItem **out_items, size_t *out_count) {
    const char *sql = "SELECT id, sale_id, product_id, sku, name, quantity, unit_price_cents, total_cents "
                      "FROM sale_items WHERE sale_id = ? ORDER BY id;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int64(stmt, 1, sale_id);

    size_t capacidade = 8;
    size_t count = 0;
    SaleItem *items = malloc(capacidade * sizeof(SaleItem));
    if (!items) {
        sqlite3_finalize(stmt);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacidade) {
            capacidade *= 2;
            SaleItem *temp = realloc(items, capacidade * sizeof(SaleItem));
            if (!temp) {
                free(items);
                sqlite3_finalize(stmt);
                return 0;
            }
            items = temp;
        }
        SaleItem *item = &items[count];
        item->id = sqlite3_column_int64(stmt, 0);
        item->sale_id = sqlite3_column_int64(stmt, 1);
        item->product_id = sqlite3_column_int64(stmt, 2);
        snprintf(item->sku, sizeof(item->sku), "%s", (const char *)sqlite3_column_text(stmt, 3));
        snprintf(item->name, sizeof(item->name), "%s", (const char *)sqlite3_column_text(stmt, 4));
        item->quantity = sqlite3_column_int(stmt, 5);
        item->unit_price_cents = sqlite3_column_int64(stmt, 6);
        item->total_cents = sqlite3_column_int64(stmt, 7);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_items = items;
    *out_count = count;
    return 1;
}

/*
 * Lista todas as vendas cadastradas.
 */
int model_listar_vendas(Sale **vendas, size_t *total) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "SELECT id, customer, payment_method, subtotal_cents, discount_cents, "
                      "total_cents, status, notes, created_at, coalesce(cancelled_at, '') "
                      "FROM sales ORDER BY created_at DESC, id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    size_t capacidade = 16;
    size_t count = 0;
    Sale *lista = malloc(capacidade * sizeof(Sale));
    if (!lista) {
        sqlite3_finalize(stmt);
        return MODEL_ERRO_GERAL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacidade) {
            capacidade *= 2;
            Sale *temp = realloc(lista, capacidade * sizeof(Sale));
            if (!temp) {
                model_liberar_vendas(lista, count);
                sqlite3_finalize(stmt);
                return MODEL_ERRO_GERAL;
            }
            lista = temp;
        }
        Sale *v = &lista[count];
        memset(v, 0, sizeof(Sale));
        v->id = sqlite3_column_int64(stmt, 0);
        snprintf(v->customer, sizeof(v->customer), "%s", (const char *)sqlite3_column_text(stmt, 1));
        snprintf(v->payment_method, sizeof(v->payment_method), "%s", (const char *)sqlite3_column_text(stmt, 2));
        v->subtotal_cents = sqlite3_column_int64(stmt, 3);
        v->discount_cents = sqlite3_column_int64(stmt, 4);
        v->total_cents = sqlite3_column_int64(stmt, 5);
        snprintf(v->status, sizeof(v->status), "%s", (const char *)sqlite3_column_text(stmt, 6));
        snprintf(v->notes, sizeof(v->notes), "%s", (const char *)sqlite3_column_text(stmt, 7));
        snprintf(v->created_at, sizeof(v->created_at), "%s", (const char *)sqlite3_column_text(stmt, 8));
        snprintf(v->cancelled_at, sizeof(v->cancelled_at), "%s", (const char *)sqlite3_column_text(stmt, 9));
        
        /* Carrega os itens vinculados à venda */
        carregar_itens_da_venda(db, v->id, &v->items, &v->item_count);
        count++;
    }

    sqlite3_finalize(stmt);
    *vendas = lista;
    *total = count;
    return MODEL_OK;
}

/*
 * Busca os dados detalhados de uma venda com seus itens.
 */
int model_buscar_venda_por_id(int64_t id, Sale *venda) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    const char *sql = "SELECT id, customer, payment_method, subtotal_cents, discount_cents, "
                      "total_cents, status, notes, created_at, coalesce(cancelled_at, '') "
                      "FROM sales WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return MODEL_ERRO_GERAL;

    sqlite3_bind_int64(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return MODEL_NAO_ENCONTRADO;
    }

    memset(venda, 0, sizeof(Sale));
    venda->id = sqlite3_column_int64(stmt, 0);
    snprintf(venda->customer, sizeof(venda->customer), "%s", (const char *)sqlite3_column_text(stmt, 1));
    snprintf(venda->payment_method, sizeof(venda->payment_method), "%s", (const char *)sqlite3_column_text(stmt, 2));
    venda->subtotal_cents = sqlite3_column_int64(stmt, 3);
    venda->discount_cents = sqlite3_column_int64(stmt, 4);
    venda->total_cents = sqlite3_column_int64(stmt, 5);
    snprintf(venda->status, sizeof(venda->status), "%s", (const char *)sqlite3_column_text(stmt, 6));
    snprintf(venda->notes, sizeof(venda->notes), "%s", (const char *)sqlite3_column_text(stmt, 7));
    snprintf(venda->created_at, sizeof(venda->created_at), "%s", (const char *)sqlite3_column_text(stmt, 8));
    snprintf(venda->cancelled_at, sizeof(venda->cancelled_at), "%s", (const char *)sqlite3_column_text(stmt, 9));
    sqlite3_finalize(stmt);

    if (!carregar_itens_da_venda(db, venda->id, &venda->items, &venda->item_count)) {
        return MODEL_ERRO_GERAL;
    }

    return MODEL_OK;
}

/*
 * REQUISITO 4: Regra específica do tema - Registro de Venda com baixa de estoque.
 * Transação atômica completa sem uso de goto.
 */
int model_criar_venda(Sale *venda) {
    sqlite3 *db = db_get_handle();
    if (!db || !venda || venda->item_count == 0) return MODEL_ERRO_GERAL;

    /* Inicia transação exclusiva */
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    int64_t subtotal_calculado = 0;
    const char *sql_check = "SELECT sku, name, price_cents, stock, active FROM products WHERE id = ?;";

    /* 1. Valida todos os produtos e calcula totais */
    for (size_t i = 0; i < venda->item_count; i++) {
        SaleItem *item = &venda->items[i];
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ERRO_GERAL;
        }

        sqlite3_bind_int64(stmt, 1, item->product_id);
        int rc = sqlite3_step(stmt);

        if (rc == SQLITE_DONE) {
            /* Produto inexistente */
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_NAO_ENCONTRADO;
        }
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ERRO_GERAL;
        }

        snprintf(item->sku, sizeof(item->sku), "%s", (const char *)sqlite3_column_text(stmt, 0));
        snprintf(item->name, sizeof(item->name), "%s", (const char *)sqlite3_column_text(stmt, 1));
        item->unit_price_cents = sqlite3_column_int64(stmt, 2);
        int estoque_atual = sqlite3_column_int(stmt, 3);
        int ativo = sqlite3_column_int(stmt, 4);
        sqlite3_finalize(stmt);

        if (!ativo) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_CONFLITO; /* Produto desativado */
        }

        if (estoque_atual < item->quantity) {
            /* REQUISITO: O programa deverá impedir uma saída maior que a quantidade disponível */
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ESTOQUE_INSUF;
        }

        item->total_cents = item->unit_price_cents * item->quantity;
        subtotal_calculado += item->total_cents;
    }

    if (venda->discount_cents > subtotal_calculado) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_PARAMETRO_INVALIDO;
    }

    venda->subtotal_cents = subtotal_calculado;
    venda->total_cents = subtotal_calculado - venda->discount_cents;

    /* 2. Grava o cabeçalho da venda */
    const char *sql_sale = "INSERT INTO sales (customer, payment_method, subtotal_cents, discount_cents, total_cents, notes) "
                           "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt_sale = NULL;
    if (sqlite3_prepare_v2(db, sql_sale, -1, &stmt_sale, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }

    sqlite3_bind_text(stmt_sale, 1, venda->customer, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_sale, 2, venda->payment_method, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt_sale, 3, venda->subtotal_cents);
    sqlite3_bind_int64(stmt_sale, 4, venda->discount_cents);
    sqlite3_bind_int64(stmt_sale, 5, venda->total_cents);
    sqlite3_bind_text(stmt_sale, 6, venda->notes, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt_sale) != SQLITE_DONE) {
        sqlite3_finalize(stmt_sale);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_finalize(stmt_sale);

    venda->id = sqlite3_last_insert_rowid(db);

    /* 3. Grava cada item e realiza a baixa do estoque */
    const char *sql_item = "INSERT INTO sale_items (sale_id, product_id, sku, name, quantity, unit_price_cents, total_cents) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?);";
    const char *sql_stock = "UPDATE products SET stock = stock - ?, "
                            "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ','now') "
                            "WHERE id = ? AND stock >= ?;";

    for (size_t i = 0; i < venda->item_count; i++) {
        SaleItem *item = &venda->items[i];
        item->sale_id = venda->id;

        sqlite3_stmt *stmt_it = NULL;
        if (sqlite3_prepare_v2(db, sql_item, -1, &stmt_it, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ERRO_GERAL;
        }
        sqlite3_bind_int64(stmt_it, 1, venda->id);
        sqlite3_bind_int64(stmt_it, 2, item->product_id);
        sqlite3_bind_text(stmt_it, 3, item->sku, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt_it, 4, item->name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt_it, 5, item->quantity);
        sqlite3_bind_int64(stmt_it, 6, item->unit_price_cents);
        sqlite3_bind_int64(stmt_it, 7, item->total_cents);

        int rc = sqlite3_step(stmt_it);
        sqlite3_finalize(stmt_it);
        if (rc != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ERRO_GERAL;
        }

        /* Baixa no estoque */
        sqlite3_stmt *stmt_stk = NULL;
        if (sqlite3_prepare_v2(db, sql_stock, -1, &stmt_stk, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ERRO_GERAL;
        }
        sqlite3_bind_int(stmt_stk, 1, item->quantity);
        sqlite3_bind_int64(stmt_stk, 2, item->product_id);
        sqlite3_bind_int(stmt_stk, 3, item->quantity);

        rc = sqlite3_step(stmt_stk);
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt_stk);

        if (rc != SQLITE_DONE || changes != 1) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return MODEL_ESTOQUE_INSUF;
        }
    }

    /* Confirma transação */
    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }

    return MODEL_OK;
}

/*
 * REQUISITO 4: Regra específica do tema - Cancelamento de venda com devolução de estoque.
 */
int model_cancelar_venda(int64_t id) {
    sqlite3 *db = db_get_handle();
    if (!db) return MODEL_ERRO_GERAL;

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        return MODEL_ERRO_GERAL;
    }

    /* 1. Checa status da venda */
    const char *sql_check = "SELECT status FROM sales WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_NAO_ENCONTRADO;
    }
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }

    const char *status = (const char *)sqlite3_column_text(stmt, 0);
    if (strcmp(status, "completed") != 0) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_CONFLITO; /* Já cancelada */
    }
    sqlite3_finalize(stmt);

    /* 2. Verifica se a reposição ultrapassa o limite de 1.000.000 */
    const char *sql_overflow = "SELECT 1 FROM sale_items i JOIN products p ON p.id=i.product_id "
                              "WHERE i.sale_id=? AND p.stock + i.quantity > 1000000;";
    if (sqlite3_prepare_v2(db, sql_overflow, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_CONFLITO;
    }
    sqlite3_finalize(stmt);

    /* 3. Devolve os itens ao estoque */
    const char *sql_restore =
        "UPDATE products SET stock = stock + (SELECT quantity FROM sale_items WHERE sale_id=?1 AND product_id=products.id), "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ','now') "
        "WHERE id IN (SELECT product_id FROM sale_items WHERE sale_id=?1);";
    if (sqlite3_prepare_v2(db, sql_restore, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_finalize(stmt);

    /* 4. Atualiza status da venda para cancelled */
    const char *sql_cancel = "UPDATE sales SET status='cancelled', "
                             "cancelled_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?;";
    if (sqlite3_prepare_v2(db, sql_cancel, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }
    sqlite3_finalize(stmt);

    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return MODEL_ERRO_GERAL;
    }

    return MODEL_OK;
}
