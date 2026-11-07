#include "views.h"

/*
 * Endpoint: GET /api/products/:id
 * Descrição: Retorna os dados detalhados de um produto específico pelo ID.
 */
Response endpoint_buscar_produto(int64_t id) {
    Product produto = {0};

    int status = model_buscar_produto_por_id(id, &produto);
    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Produto não encontrado.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao buscar produto.");
    }

    cJSON *json = produto_para_cjson(&produto);
    return json_resposta(200, json);
}
