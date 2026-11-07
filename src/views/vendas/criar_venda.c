#include "views.h"
#include <stdlib.h>

/*
 * Endpoint: POST /api/sales
 * Descrição: Registra uma nova venda, calculando valores e efetuando a baixa de estoque.
 * REQUISITOS ATENDIDOS:
 * - Requisito 4: Regra de negócio (baixa de estoque impedindo saída maior que o disponível).
 * - Requisito 7: Alocação dinâmica de memória com calloc, verificação e liberação com free.
 * - Requisito 9: Validação de entradas e tratamento de falhas.
 */
Response endpoint_criar_venda(const Request *req) {
    Sale venda = {0};

    /* Faz o parse do JSON e aloca o vetor dinâmico de itens usando calloc */
    if (!json_para_venda_rascunho(req->body, &venda)) {
        return json_erro(400, "Informe produtos válidos, quantidades inteiras positivas e pagamento aceito.");
    }

    /* Processa a transação e a baixa no estoque */
    int status = model_criar_venda(&venda);

    /* Sempre libera a memória do rascunho de itens antes de responder */
    free(venda.items);
    venda.items = NULL;

    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Um dos produtos informados não existe.");
    }
    if (status == MODEL_CONFLITO) {
        return json_erro(409, "Produto inativo não pode ser vendido.");
    }
    if (status == MODEL_ESTOQUE_INSUF) {
        return json_erro(409, "Estoque insuficiente para a quantidade solicitada.");
    }
    if (status == MODEL_PARAMETRO_INVALIDO) {
        return json_erro(400, "O desconto não pode ser maior que o subtotal da venda.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao registrar venda no banco de dados.");
    }

    /* Recarrega a venda completa com itens e snapshot histórico para retorno */
    Sale venda_criada = {0};
    model_buscar_venda_por_id(venda.id, &venda_criada);
    cJSON *json = venda_para_cjson(&venda_criada);
    model_liberar_venda(&venda_criada);

    return json_resposta(201, json);
}
