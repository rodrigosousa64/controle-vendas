#include "views.h"
#include <stdlib.h>
#include <string.h>

/*
 * Endpoint: GET /api/products
 * Descrição: Lista todos os produtos cadastrados no sistema, com suporte a filtro ?status=active/inactive.
 */
Response endpoint_listar_produtos(const Request *req) {
    char status_filtro[32] = "";
    query_param(req->query, "status", status_filtro, sizeof(status_filtro));

    Product *produtos = NULL;
    size_t total = 0;

    int status = model_listar_produtos(&produtos, &total);
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao consultar produtos no banco de dados.");
    }

    /* Se solicitou filtro de status */
    cJSON *json = cJSON_CreateArray();
    if (!json) {
        free(produtos);
        return json_erro(500, "Memória insuficiente.");
    }

    for (size_t i = 0; i < total; i++) {
        if (!strcmp(status_filtro, "active") && !produtos[i].active) continue;
        if (!strcmp(status_filtro, "inactive") && produtos[i].active) continue;

        cJSON *item = produto_para_cjson(&produtos[i]);
        if (item) cJSON_AddItemToArray(json, item);
    }
    free(produtos);

    return json_resposta(200, json);
}
