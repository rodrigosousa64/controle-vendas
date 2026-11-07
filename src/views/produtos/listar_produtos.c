#include "views.h"
#include <stdlib.h>

/*
 * Endpoint: GET /api/products
 * Descrição: Lista todos os produtos cadastrados no sistema.
 */
Response endpoint_listar_produtos(const Request *req) {
    (void)req;
    Product *produtos = NULL;
    size_t total = 0;

    int status = model_listar_produtos(&produtos, &total);
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao consultar produtos no banco de dados.");
    }

    cJSON *json = produtos_para_cjson_array(produtos, total);
    free(produtos);

    return json_resposta(200, json);
}
