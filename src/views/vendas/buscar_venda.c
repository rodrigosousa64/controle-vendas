#include "views.h"

/*
 * Endpoint: GET /api/sales/:id
 * Descrição: Retorna os dados completos de uma venda e seus itens para comprovante.
 */
Response endpoint_buscar_venda(int64_t id) {
    Sale venda = {0};

    int status = model_buscar_venda_por_id(id, &venda);
    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Venda não encontrada.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao buscar detalhes da venda.");
    }

    cJSON *json = venda_para_cjson(&venda);
    model_liberar_venda(&venda);

    return json_resposta(200, json);
}
