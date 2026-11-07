#include "views.h"

/*
 * Endpoint: DELETE /api/products/:id
 * Descrição: Remove um produto do sistema ou alerta caso haja vendas vinculadas (Requisito 3).
 */
Response endpoint_excluir_produto(int64_t id) {
    int status = model_excluir_produto(id);

    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Produto não encontrado.");
    }
    if (status == MODEL_CONFLITO) {
        return json_erro(409, "Este produto possui vendas no histórico. Desative o produto para preservar os registros.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao excluir produto.");
    }

    cJSON *json = cJSON_CreateObject();
    if (json) cJSON_AddBoolToObject(json, "deleted", 1);
    return json_resposta(200, json);
}
