#ifndef VENDA_HTTP_H
#define VENDA_HTTP_H

#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "models.h"

#define MAX_BODY 65536

/* Representação de uma Requisição HTTP recebida */
typedef struct {
    char method[16];        /* GET, POST, PUT, DELETE */
    char path[512];         /* /api/products, etc. */
    char query[2048];       /* search=foo&status=active */
    char *body;             /* Corpo em JSON (se houver) */
    size_t body_len;
} Request;

/* Resposta HTTP a ser devolvida ao cliente */
typedef struct {
    int status;             /* 200, 201, 400, 404, 500... */
    char *body;             /* Texto JSON alocado dinamicamente */
    const char *content_type;
} Response;

/* Funções construtoras de resposta HTTP */
Response json_resposta(int status, cJSON *json);
Response json_erro(int status, const char *mensagem);

/* Conversores de Struct <-> cJSON (mantêm as Views limpas) */
int json_para_produto(const char *json_str, Product *out);
cJSON *produto_para_cjson(const Product *p);
cJSON *produtos_para_cjson_array(const Product *produtos, size_t total);

int json_para_venda_rascunho(const char *json_str, Sale *out);
cJSON *venda_para_cjson(const Sale *s);
cJSON *vendas_para_cjson_array(const Sale *vendas, size_t total);

/* Utilitários gerais */
int query_param(const char *query, const char *key, char *out, size_t capacity);

/* Inicialização e controle do servidor HTTP */
int http_run(const char *host, int port);
void http_stop(void);

#endif /* VENDA_HTTP_H */
