#include "views.h"
#include "app.h"

/*
 * Endpoint: GET /api/health
 * Descrição: Retorna status e versão do sistema para verificação de disponibilidade.
 */
Response endpoint_health(const Request *req) {
    (void)req;
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "status", "ok");
        cJSON_AddStringToObject(json, "version", APP_VERSION);
    }
    return json_resposta(200, json);
}
