#include "views.h"

/*
 * Endpoint: POST /api/sales/:id/cancel
 * Descrição: Cancela uma venda e estorna os produtos de volta ao estoque (Requisito 4).
 */
Response endpoint_cancelar_venda(int64_t id) {
    int status = model_cancelar_venda(id);

    if (status == MODEL_NAO_ENCONTRADO) {
        return json_erro(404, "Venda não encontrada.");
    }
    if (status == MODEL_CONFLITO) {
        return json_erro(409, "Esta venda já foi cancelada ou a devolução excederia o estoque máximo.");
    }
    if (status != MODEL_OK) {
        return json_erro(500, "Erro ao cancelar venda.");
    }

    /* Retorna o registro da venda atualizado como cancelado */
    Sale venda_cancelada = {0};
    model_buscar_venda_por_id(id, &venda_cancelada);
    cJSON *json = venda_para_cjson(&venda_cancelada);
    model_liberar_venda(&venda_cancelada);

    return json_resposta(200, json);
}
