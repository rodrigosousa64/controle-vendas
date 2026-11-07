#include "views.h"

/*
 * Endpoint: GET /api/sales
 * Descrição: Retorna o histórico de vendas cadastradas com seus respectivos itens.
 */
Response endpoint_listar_vendas(const Request *req) {
    (void)req;
    Sale *vendas = NULL;
    size_t total = 0;

    int status = model_listar_vendas(&vendas, &total);
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao consultar vendas no banco de dados.");
    }

    cJSON *json = vendas_para_cjson_array(vendas, total);
    model_liberar_vendas(vendas, total);

    return json_resposta(200, json);
}
