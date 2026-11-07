#include "views.h"

/*
 * Endpoint: POST /api/shutdown
 * Descrição: Encerra o servidor de forma limpa, preservando os dados em disco (Requisito 1).
 */
Response endpoint_shutdown(const Request *req) {
    (void)req;
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "message", "Servidor encerrado. Dados preservados com sucesso.");
    }
    Response resp = json_resposta(200, json);
    if (resp.body) {
        http_stop();
    }
    return resp;
}
