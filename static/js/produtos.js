'use strict';

let allProducts = [];

async function carregarProdutos() {
  try {
    allProducts = await api('/api/products');
    renderTabela();
  } catch (err) {
    toast('Erro ao buscar produtos: ' + err.message, true);
  }
}

function renderTabela() {
  const tbody = document.getElementById('products-tbody');
  const busca = normalize(document.getElementById('product-search').value);
  const statusFiltro = document.getElementById('filter-status').value;

  const filtrados = allProducts.filter(p => {
    // Filtro por texto (nome, sku ou categoria)
    const matchBusca = !busca ||
      normalize(p.name).includes(busca) ||
      normalize(p.sku).includes(busca) ||
      normalize(p.category).includes(busca);

    // Filtro por situação
    let matchStatus = true;
    if (statusFiltro === 'active') matchStatus = p.active;
    else if (statusFiltro === 'inactive') matchStatus = !p.active;
    else if (statusFiltro === 'low_stock') matchStatus = p.active && p.stock <= p.min_stock;

    return matchBusca && matchStatus;
  });

  if (filtrados.length === 0) {
    tbody.innerHTML = '<tr><td colspan="7" class="empty-state">Nenhum produto encontrado.</td></tr>';
    return;
  }

  tbody.innerHTML = filtrados.map(p => {
    let situacao = 'Ativo';
    let badgeClass = 'active';
    if (!p.active) {
      situacao = 'Inativo';
      badgeClass = 'inactive';
    } else if (p.stock === 0) {
      situacao = 'Sem estoque';
      badgeClass = 'cancelled';
    } else if (p.stock <= p.min_stock) {
      situacao = 'Estoque baixo';
      badgeClass = 'low';
    }

    return `
      <tr>
        <td>
          <strong>${escapeHTML(p.name)}</strong>
          ${p.notes ? `<div style="font-size:12px;color:#667085;">${escapeHTML(p.notes)}</div>` : ''}
        </td>
        <td><code>${escapeHTML(p.sku)}</code></td>
        <td>${escapeHTML(p.category)}</td>
        <td class="right" style="font-weight:600;">${money(p.price_cents)}</td>
        <td class="right">${p.stock} un.</td>
        <td><span class="badge ${badgeClass}">${situacao}</span></td>
        <td class="right">
          <div style="display:inline-flex;gap:4px;">
            <button type="button" class="icon-button" onclick="abrirModalProduto(${p.id})" title="Editar ${escapeHTML(p.name)}" aria-label="Editar ${escapeHTML(p.name)}">
              ${icon('edit', 16)}
            </button>
            <button type="button" class="icon-button danger" onclick="excluirProduto(${p.id}, '${escapeHTML(p.name)}')" title="Excluir ${escapeHTML(p.name)}" aria-label="Excluir ${escapeHTML(p.name)}">
              ${icon('trash', 16)}
            </button>
          </div>
        </td>
      </tr>
    `;
  }).join('');
}

function abrirModalProduto(id = 0) {
  const produto = id ? allProducts.find(p => p.id === id) : null;
  const isEdicao = Boolean(produto);

  const html = `
    <div class="modal-header">
      <div>
        <h2 id="modal-title">${isEdicao ? 'Editar Produto' : 'Novo Produto'}</h2>
        <p>${isEdicao ? 'Atualize as informações do produto.' : 'Cadastre um novo item no estoque.'}</p>
      </div>
      <button type="button" class="icon-button" data-action="close-modal" aria-label="Fechar janela">
        ${icon('close', 18)}
      </button>
    </div>

    <form id="form-produto" onsubmit="salvarProduto(event, ${id})">
      <div class="form-error" style="display:none;margin-bottom:16px;"></div>

      <div class="form-grid">
        <div class="form-group">
          <label for="p-sku">Código único (SKU) *</label>
          <input type="text" id="p-sku" required maxlength="40" value="${escapeHTML(produto ? produto.sku : '')}" placeholder="Ex: CAF-001" />
        </div>

        <div class="form-group">
          <label for="p-category">Categoria *</label>
          <input type="text" id="p-category" required maxlength="60" value="${escapeHTML(produto ? produto.category : '')}" placeholder="Ex: Mercearia" />
        </div>
      </div>

      <div class="form-group">
        <label for="p-name">Nome do produto *</label>
        <input type="text" id="p-name" required maxlength="120" value="${escapeHTML(produto ? produto.name : '')}" placeholder="Ex: Café especial torrado em grãos" />
      </div>

      <div class="form-grid">
        <div class="form-group">
          <label for="p-price">Preço de venda (R$) *</label>
          <input type="text" id="p-price" required value="${produto ? (produto.price_cents / 100).toFixed(2).replace('.', ',') : ''}" placeholder="0,00" />
        </div>

        <div class="form-group">
          <label for="p-stock">Estoque atual *</label>
          <input type="number" id="p-stock" required min="0" max="1000000" value="${produto ? produto.stock : '0'}" />
        </div>

        <div class="form-group">
          <label for="p-min">Estoque mínimo *</label>
          <input type="number" id="p-min" required min="0" max="1000000" value="${produto ? produto.min_stock : '5'}" />
        </div>
      </div>

      <div class="form-group">
        <label for="p-notes">Observações</label>
        <textarea id="p-notes" rows="2" maxlength="500" placeholder="Detalhes opcionais do fornecedor ou especificações">${escapeHTML(produto ? produto.notes : '')}</textarea>
      </div>

      <div class="form-group checkbox">
        <label>
          <input type="checkbox" id="p-active" ${!produto || produto.active ? 'checked' : ''} />
          Produto ativo para vendas
        </label>
      </div>

      <div class="modal-footer" style="display:flex;justify-content:flex-end;gap:12px;margin-top:20px;">
        <button type="button" class="button secondary" data-action="close-modal">Cancelar</button>
        <button type="submit" class="button primary">${isEdicao ? 'Salvar alterações' : 'Cadastrar produto'}</button>
      </div>
    </form>
  `;

  openModal(html);
}

async function salvarProduto(event, id) {
  event.preventDefault();
  const form = event.target;
  const errorEl = form.querySelector('.form-error');
  errorEl.style.display = 'none';
  errorEl.textContent = '';

  try {
    const sku = form.querySelector('#p-sku').value.trim();
    const name = form.querySelector('#p-name').value.trim();
    const category = form.querySelector('#p-category').value.trim();
    const priceStr = form.querySelector('#p-price').value.trim();
    const stock = parseInt(form.querySelector('#p-stock').value, 10);
    const min_stock = parseInt(form.querySelector('#p-min').value, 10);
    const notes = form.querySelector('#p-notes').value.trim();
    const active = form.querySelector('#p-active').checked;

    const price_cents = parseMoney(priceStr, false);

    const payload = {
      sku, name, category, price_cents, stock, min_stock, active, notes
    };

    const url = id ? `/api/products/${id}` : '/api/products';
    const method = id ? 'PUT' : 'POST';

    await api(url, { method, body: JSON.stringify(payload) });

    closeModal();
    toast(id ? 'Produto atualizado com sucesso!' : 'Produto cadastrado com sucesso!');
    await carregarProdutos();
  } catch (err) {
    errorEl.textContent = err.message;
    errorEl.classList.add('visible');
    errorEl.style.display = 'block';
  }
}

async function excluirProduto(id, nome) {
  const confirmed = await askConfirm(
    'Excluir produto?',
    `Tem certeza que deseja excluir "${nome}"? Esta ação não pode ser desfeita. Se o produto possuir vendas, você deverá desativá-lo para manter o histórico.`,
    'Excluir produto',
    true
  );
  if (!confirmed) return;

  try {
    await api(`/api/products/${id}`, { method: 'DELETE' });
    toast('Produto excluído com sucesso!');
    await carregarProdutos();
  } catch (err) {
    toast(err.message, true);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('product-search').addEventListener('input', renderTabela);
  document.getElementById('filter-status').addEventListener('change', renderTabela);
  document.getElementById('btn-novo-produto').addEventListener('click', () => abrirModalProduto(0));
  carregarProdutos();
});
