#include "views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Endpoint: GET /api/reports
 * Descrição: Retorna o resumo consolidado de vendas no período para o frontend.
 */
Response endpoint_relatorios(const Request *req) {
    char from[32] = "1900-01-01";
    char to[32] = "9999-12-31";

    query_param(req->query, "from", from, sizeof(from));
    query_param(req->query, "to", to, sizeof(to));

    sqlite3 *db = db_get_handle();
    if (!db) {
        return json_erro(500, "Erro ao conectar ao banco de dados.");
    }

    const char *sql = "SELECT count(*), coalesce(sum(total_cents), 0) "
                      "FROM sales WHERE status = 'completed' "
                      "AND date(created_at, '-3 hours') BETWEEN ?1 AND ?2;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return json_erro(500, "Erro na consulta de relatório.");
    }

    sqlite3_bind_text(stmt, 1, from, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, to, -1, SQLITE_TRANSIENT);

    int64_t count = 0;
    int64_t total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
        total = sqlite3_column_int64(stmt, 1);
    }
    sqlite3_finalize(stmt);

    cJSON *json = cJSON_CreateObject();
    if (!json) return json_erro(500, "Memória insuficiente.");

    cJSON_AddStringToObject(json, "from", from);
    cJSON_AddStringToObject(json, "to", to);

    cJSON *summary = cJSON_CreateObject();
    if (summary) {
        cJSON_AddNumberToObject(summary, "sale_count", (double)count);
        cJSON_AddNumberToObject(summary, "total_cents", (double)total);
        cJSON_AddItemToObject(json, "summary", summary);
    }

    return json_resposta(200, json);
}
