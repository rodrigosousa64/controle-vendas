#ifndef VENDA_VIEWS_H
#define VENDA_VIEWS_H

#include "http.h"

/*
 * CAMADA VIEW (1 ARQUIVO .c PARA CADA ENDPOINT)
 * Cada função abaixo reside em um arquivo .c próprio, com responsabilidade única.
 */

/* Views de Produtos (src/views/produtos/) */
Response endpoint_listar_produtos(const Request *req);
Response endpoint_buscar_produto(int64_t id);
Response endpoint_cadastrar_produto(const Request *req);
Response endpoint_atualizar_produto(const Request *req, int64_t id);
Response endpoint_excluir_produto(int64_t id);

/* Views de Vendas (src/views/vendas/) */
Response endpoint_listar_vendas(const Request *req);
Response endpoint_buscar_venda(int64_t id);
Response endpoint_criar_venda(const Request *req);
Response endpoint_cancelar_venda(int64_t id);

/* Views de Sistema (src/views/sistema/) */
Response endpoint_health(const Request *req);
Response endpoint_shutdown(const Request *req);
Response endpoint_relatorios(const Request *req);

/* Roteador Central (src/server/router.c) */
Response rotear_requisicao(const Request *req);

#endif /* VENDA_VIEWS_H */
