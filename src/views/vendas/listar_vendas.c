#include "views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Endpoint: GET /api/sales
 * Descrição: Retorna o histórico de vendas cadastradas, com suporte a filtros ?from=&to=&search=.
 */
Response endpoint_listar_vendas(const Request *req) {
    char from[32] = "";
    char to[32] = "";
    char search[128] = "";

    query_param(req->query, "from", from, sizeof(from));
    query_param(req->query, "to", to, sizeof(to));
    query_param(req->query, "search", search, sizeof(search));

    Sale *vendas = NULL;
    size_t total = 0;

    int status = model_listar_vendas(&vendas, &total);
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao consultar vendas no banco de dados.");
    }

    cJSON *json = cJSON_CreateArray();
    if (!json) {
        model_liberar_vendas(vendas, total);
        return json_erro(500, "Memória insuficiente.");
    }

    for (size_t i = 0; i < total; i++) {
        const Sale *s = &vendas[i];

        /* Filtro de data */
        if (from[0] && strncmp(s->created_at, from, 10) < 0) continue;
        if (to[0] && strncmp(s->created_at, to, 10) > 0) continue;

        /* Filtro de busca (cliente ou ID) */
        if (search[0]) {
            char id_str[32];
            snprintf(id_str, sizeof(id_str), "%lld", (long long)s->id);
            if (!strcasestr(s->customer, search) && strcmp(id_str, search) != 0) {
                continue;
            }
        }

        cJSON *item = venda_para_cjson(s);
        if (item) cJSON_AddItemToArray(json, item);
    }
    model_liberar_vendas(vendas, total);

    return json_resposta(200, json);
}
