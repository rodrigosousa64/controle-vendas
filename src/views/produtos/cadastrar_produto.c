#include "views.h"

/*
 * Endpoint: POST /api/products
 * Descrição: Valida os dados de entrada e cadastra um novo produto (Requisito 3 e 9).
 */
Response endpoint_cadastrar_produto(const Request *req) {
    Product produto = {0};

    /* Converte o corpo JSON recebido para a struct Product */
    if (!json_para_produto(req->body, &produto)) {
        return json_erro(400, "Informe código, nome, categoria, preço positivo em centavos e estoques inteiros entre 0 e 1.000.000.");
    }

    /* Insere através da camada Model */
    int status = model_inserir_produto(&produto);

    if (status == MODEL_DUPLICADO) {
        return json_erro(409, "Já existe um produto com este código. Use um código único.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Não foi possível cadastrar o produto.");
    }

    cJSON *json = produto_para_cjson(&produto);
    return json_resposta(201, json);
}
