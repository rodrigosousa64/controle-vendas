#include "views.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper para verificar correspondência exata de método e caminho */
static int rota_exata(const Request *req, const char *metodo, const char *caminho) {
    return (strcmp(req->method, metodo) == 0 && strcmp(req->path, caminho) == 0);
}

/* Helper para extrair ID numérico de rotas no formato /api/recurso/:id */
static int extrair_id(const char *path, const char *prefixo, int64_t *out_id, const char **out_sufixo) {
    size_t len = strlen(prefixo);
    if (strncmp(path, prefixo, len) != 0) return 0;
    if (path[len] < '0' || path[len] > '9') return 0;

    errno = 0;
    char *end = NULL;
    long long valor = strtoll(path + len, &end, 10);
    if (errno || valor <= 0 || valor > 2147483647) return 0;

    *out_id = (int64_t)valor;
    *out_sufixo = end ? end : "";
    return 1;
}

/*
 * ROTEADOR CENTRAL DA APLICAÇÃO
 * Organizado de forma visual e intuitiva como um índice das Views em C.
 */
Response rotear_requisicao(const Request *req) {
    int64_t id = 0;
    const char *sufixo = "";

    /* === ROTAS DE SISTEMA === */
    if (rota_exata(req, "GET",  "/api/health"))    return endpoint_health(req);
    if (rota_exata(req, "POST", "/api/shutdown"))  return endpoint_shutdown(req);
    if (rota_exata(req, "GET",  "/api/reports"))   return endpoint_relatorios(req);

    /* === ROTAS DE PRODUTOS === */
    if (rota_exata(req, "GET",  "/api/products"))  return endpoint_listar_produtos(req);
    if (rota_exata(req, "POST", "/api/products"))  return endpoint_cadastrar_produto(req);

    if (extrair_id(req->path, "/api/products/", &id, &sufixo) && strlen(sufixo) == 0) {
        if (strcmp(req->method, "GET") == 0)    return endpoint_buscar_produto(id);
        if (strcmp(req->method, "PUT") == 0)    return endpoint_atualizar_produto(req, id);
        if (strcmp(req->method, "DELETE") == 0) return endpoint_excluir_produto(id);
        return json_erro(405, "Método não permitido para este produto.");
    }

    /* === ROTAS DE VENDAS === */
    if (rota_exata(req, "GET",  "/api/sales"))     return endpoint_listar_vendas(req);
    if (rota_exata(req, "POST", "/api/sales"))     return endpoint_criar_venda(req);

    if (extrair_id(req->path, "/api/sales/", &id, &sufixo)) {
        if (strlen(sufixo) == 0) {
            if (strcmp(req->method, "GET") == 0) return endpoint_buscar_venda(id);
            return json_erro(405, "Método não permitido para esta venda.");
        }
        if (strcmp(sufixo, "/cancel") == 0) {
            if (strcmp(req->method, "POST") == 0) return endpoint_cancelar_venda(id);
            return json_erro(405, "Utilize POST para cancelar uma venda.");
        }
    }

    /* Se o caminho pertence a /api/ mas não casou método */
    if (strncmp(req->path, "/api/", 5) == 0) {
        return json_erro(404, "Recurso ou rota da API não encontrada.");
    }

    return json_erro(404, "Página não encontrada.");
}
