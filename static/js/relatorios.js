'use strict';

let rawSales = [];
let rawProducts = [];

const paymentNames = {
  pix: 'Pix',
  dinheiro: 'Dinheiro',
  credito: 'Cartão de crédito',
  debito: 'Cartão de débito'
};

async function carregarDadosRelatorio() {
  try {
    const [sales, products] = await Promise.all([
      api('/api/sales'),
      api('/api/products')
    ]);
    rawSales = sales;
    rawProducts = products;
    calcularRelatorios();
  } catch (err) {
    toast('Erro ao carregar dados do relatório: ' + err.message, true);
  }
}

function calcularRelatorios() {
  const from = document.getElementById('rep-from').value;
  const to = document.getElementById('rep-to').value;

  // Filtra as vendas pelo intervalo de datas
  const vendasFiltradas = rawSales.filter(s => {
    const dataVenda = (s.created_at || '').slice(0, 10);
    if (from && dataVenda < from) return false;
    if (to && dataVenda > to) return false;
    return true;
  });

  const concluidas = vendasFiltradas.filter(s => s.status === 'completed');

  // 1. Indicadores Chave
  const faturamentoTotalCents = concluidas.reduce((acc, s) => acc + s.total_cents, 0);
  const totalVendas = concluidas.length;
  const totalUnidades = concluidas.reduce((acc, s) => {
    return acc + (s.items || []).reduce((sub, it) => sub + it.quantity, 0);
  }, 0);
  const ticketMedioCents = totalVendas > 0 ? Math.round(faturamentoTotalCents / totalVendas) : 0;

  document.getElementById('rep-total').textContent = money(faturamentoTotalCents);
  document.getElementById('rep-sales-count').textContent = totalVendas;
  document.getElementById('rep-items-sold').textContent = totalUnidades;
  document.getElementById('rep-average').textContent = money(ticketMedioCents);

  // 2. Ranking Top 10 Produtos Mais Vendidos
  const produtosMap = new Map(); // product_id -> { sku, name, quantity, gross_cents }
  for (const s of concluidas) {
    for (const it of (s.items || [])) {
      const cur = produtosMap.get(it.product_id) || {
        sku: it.sku,
        name: it.name,
        quantity: 0,
        gross_cents: 0
      };
      cur.quantity += it.quantity;
      cur.gross_cents += it.total_cents;
      produtosMap.set(it.product_id, cur);
    }
  }

  const topProdutos = Array.from(produtosMap.values())
    .sort((a, b) => b.quantity - a.quantity || b.gross_cents - a.gross_cents)
    .slice(0, 10);

  const topContainer = document.getElementById('top-products-container');
  if (topProdutos.length === 0) {
    topContainer.innerHTML = '<p class="empty-state">Nenhum produto vendido no período.</p>';
  } else {
    topContainer.innerHTML = `
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Produto</th>
              <th class="right">Qtd</th>
              <th class="right">Faturamento</th>
            </tr>
          </thead>
          <tbody>
            ${topProdutos.map(p => `
              <tr>
                <td>
                  <strong>${escapeHTML(p.name)}</strong>
                  <div style="font-size:12px;color:#667085;">${escapeHTML(p.sku)}</div>
                </td>
                <td class="right">${p.quantity} un.</td>
                <td class="right" style="font-weight:600;">${money(p.gross_cents)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    `;
  }

  // 3. Faturamento por Meio de Pagamento
  const pagamentosMap = new Map(); // payment_method -> { count, total_cents }
  for (const s of concluidas) {
    const cur = pagamentosMap.get(s.payment_method) || { count: 0, total_cents: 0 };
    cur.count++;
    cur.total_cents += s.total_cents;
    pagamentosMap.set(s.payment_method, cur);
  }

  const pagamentosList = Array.from(pagamentosMap.entries())
    .sort((a, b) => b[1].total_cents - a[1].total_cents);

  const payContainer = document.getElementById('payments-container');
  if (pagamentosList.length === 0) {
    payContainer.innerHTML = '<p class="empty-state">Nenhuma venda registrada no período.</p>';
  } else {
    payContainer.innerHTML = `
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Meio</th>
              <th class="right">Vendas</th>
              <th class="right">Total</th>
            </tr>
          </thead>
          <tbody>
            ${pagamentosList.map(([metodo, dados]) => `
              <tr>
                <td><strong>${paymentNames[metodo] || metodo}</strong></td>
                <td class="right">${dados.count}</td>
                <td class="right" style="font-weight:600;">${money(dados.total_cents)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    `;
  }

  // 4. Evolução Diária de Vendas
  const diasMap = new Map(); // dateStr -> { count, total_cents }
  for (const s of concluidas) {
    const dia = (s.created_at || '').slice(0, 10);
    const cur = diasMap.get(dia) || { count: 0, total_cents: 0 };
    cur.count++;
    cur.total_cents += s.total_cents;
    diasMap.set(dia, cur);
  }

  const diasList = Array.from(diasMap.entries()).sort((a, b) => a[0].localeCompare(b[0]));

  const dailyContainer = document.getElementById('daily-sales-container');
  if (diasList.length === 0) {
    dailyContainer.innerHTML = '<p class="empty-state">Sem dados de vendas para o gráfico diário.</p>';
  } else {
    dailyContainer.innerHTML = `
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Data</th>
              <th class="right">Transações</th>
              <th class="right">Faturamento Diário</th>
            </tr>
          </thead>
          <tbody>
            ${diasList.map(([dia, dados]) => `
              <tr>
                <td><strong>${shortDate(dia)}</strong></td>
                <td class="right">${dados.count}</td>
                <td class="right" style="font-weight:600;">${money(dados.total_cents)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    `;
  }
}

// Exportação em formato CSV
function exportarCSV() {
  const from = document.getElementById('rep-from').value;
  const to = document.getElementById('rep-to').value;

  const concluidas = rawSales.filter(s => {
    const d = (s.created_at || '').slice(0, 10);
    if (from && d < from) return false;
    if (to && d > to) return false;
    return s.status === 'completed';
  });

  const totalVendido = concluidas.reduce((acc, s) => acc + s.total_cents, 0);

  let csv = 'Venda;Data;Cliente;Pagamento;Total\r\n';
  for (const s of concluidas) {
    const valor = (s.total_cents / 100).toFixed(2).replace('.', ',');
    csv += `#${s.id};"${s.created_at}";"${s.customer || 'Cliente balcão'}";"${paymentNames[s.payment_method] || s.payment_method}";"${valor}"\r\n`;
  }
  csv += `\r\nTotal Geral;;;;"${(totalVendido / 100).toFixed(2).replace('.', ',')}"\r\n`;

  const blob = new Blob(['\uFEFF' + csv], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `relatorio-vendas-${from || 'inicio'}-a-${to || 'fim'}.csv`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

document.addEventListener('DOMContentLoaded', () => {
  // Inicializa datas com o mês corrente
  const hoje = todayISO();
  const d = new Date();
  d.setDate(d.getDate() - 30);
  const trintaDiasAtras = d.toISOString().slice(0, 10);

  document.getElementById('rep-from').value = trintaDiasAtras;
  document.getElementById('rep-to').value = hoje;

  document.getElementById('form-periodo').addEventListener('submit', (e) => {
    e.preventDefault();
    calcularRelatorios();
  });

  document.getElementById('btn-periodo-tudo').addEventListener('click', () => {
    document.getElementById('rep-from').value = '';
    document.getElementById('rep-to').value = '';
    calcularRelatorios();
  });

  document.getElementById('btn-export-csv').addEventListener('click', exportarCSV);

  carregarDadosRelatorio();
});
