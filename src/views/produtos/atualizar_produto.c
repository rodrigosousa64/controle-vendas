#include "views.h"

/*
 * Endpoint: PUT /api/products/:id
 * Descrição: Atualiza as informações de um produto existente (Requisito 3).
 */
Response endpoint_atualizar_produto(const Request *req, int64_t id) {
    Product produto = {0};

    if (!json_para_produto(req->body, &produto)) {
        return json_erro(400, "Dados do produto inválidos ou limites dos campos ultrapassados.");
    }

    int status = model_atualizar_produto(id, &produto);

    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Produto não encontrado.");
    }
    if (status == MODEL_DUPLICADO) {
        return json_erro(409, "Já existe outro produto com este código. Use um código único.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao atualizar produto.");
    }

    /* Retorna os dados atualizados com o ID */
    produto.id = id;
    cJSON *json = produto_para_cjson(&produto);
    return json_resposta(200, json);
}
