'use strict';

/* Ícones SVG vetorizados */
const ICONS = {
  dashboard: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
  box: '<path d="m12 3 9 5-9 5-9-5 9-5Z"/><path d="M3 8v9l9 5 9-5V8M12 13v9M7.5 5.5l9 5"/>',
  bag: '<path d="M5 7h14l1 14H4L5 7Z"/><path d="M8 8V6a4 4 0 0 1 8 0v2"/>',
  chart: '<path d="M4 3v17h17M8 15v-4M13 15V7M18 15V4"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  search: '<circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/>',
  edit: '<path d="m15 5 4 4M4 20l4-1L20 7a2.8 2.8 0 0 0-4-4L4 15l-1 6Z"/>',
  trash: '<path d="M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7M14 10v7"/>',
  close: '<path d="m6 6 12 12M6 18 18 6"/>',
  power: '<path d="M18.36 6.64a9 9 0 1 1-12.73 0M12 2v10"/>',
  calendar: '<rect x="3" y="5" width="18" height="16" rx="2"/><path d="M7 3v4m10-4v4M3 11h18"/>',
  download: '<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>',
  print: '<path d="M7 8V3h10v5M7 17H3V9h18v8h-4M7 14h10v7H7Z"/><path d="M17 11h1"/>',
  alert: '<path d="M10 4 2 18a2 2 0 0 0 2 3h16a2 2 0 0 0 2-3L14 4a2.3 2.3 0 0 0-4 0Z"/><path d="M12 9v4m0 3v1"/>',
  check: '<path d="m5 12 4 4L19 6"/>'
};

function icon(name, size = 18) {
  return `<svg width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONS[name] || ICONS.box}</svg>`;
}

/* Formatação e Utilitários */
function escapeHTML(str) {
  return String(str ?? '').replace(/[&<>"']/g, c => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  }[c]));
}

function money(cents) {
  return new Intl.NumberFormat('pt-BR', { style: 'currency', currency: 'BRL' })
    .format((Number(cents) || 0) / 100);
}

function parseMoney(value, allowZero = true) {
  const clean = String(value).trim();
  if (!/^\d{1,13}([,.]\d{1,2})?$/.test(clean)) {
    throw new Error('Informe o valor com até duas casas decimais, por exemplo: 24,90.');
  }
  const [whole, fraction = ''] = clean.replace(',', '.').split('.');
  const cents = Number(whole) * 100 + Number(fraction.padEnd(2, '0'));
  if (!Number.isSafeInteger(cents) || (!allowZero && cents <= 0)) {
    throw new Error('O valor deve ser maior que zero.');
  }
  return cents;
}

function normalize(text) {
  return String(text || '').normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
}

function todayISO() {
  const d = new Date();
  const year = d.getFullYear();
  const month = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  return `${year}-${month}-${day}`;
}

function shortDate(iso) {
  if (!iso) return '';
  const [y, m, d] = iso.slice(0, 10).split('-');
  return `${d}/${m}/${y}`;
}

function dateTime(iso) {
  if (!iso) return '';
  return new Intl.DateTimeFormat('pt-BR', { dateStyle: 'short', timeStyle: 'short' }).format(new Date(iso));
}

/* Chamadas AJAX para a API C */
async function api(path, options = {}) {
  const res = await fetch(path, {
    cache: 'no-store',
    ...options,
    headers: { 'Content-Type': 'application/json', ...options.headers }
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new Error(data.error || 'Erro na operação do servidor.');
  }
  return data;
}

/* Feedback Visual (Toasts) */
function toast(message, isError = false) {
  const container = document.getElementById('toasts');
  if (!container) return;
  const t = document.createElement('div');
  t.className = `toast ${isError ? 'error' : 'success'}`;
  t.innerHTML = `${icon(isError ? 'alert' : 'check', 16)} <span>${escapeHTML(message)}</span>`;
  container.appendChild(t);
  setTimeout(() => {
    t.classList.add('fade-out');
    setTimeout(() => t.remove(), 300);
  }, 4000);
}

/* Modais e Confirmações */
function openModal(html, className = '') {
  const modal = document.getElementById('modal');
  if (!modal) return;
  modal.className = className;
  modal.innerHTML = html;
  if (!modal.open) modal.showModal();
}

function closeModal() {
  const modal = document.getElementById('modal');
  if (modal && modal.open) modal.close();
}

function askConfirm(title, text, confirmBtnText = 'Confirmar', isDanger = true) {
  return new Promise((resolve) => {
    const dialog = document.getElementById('confirm-modal');
    if (!dialog) { resolve(true); return; }
    dialog.innerHTML = `
      <div class="confirm-content">
        <h2 id="confirm-title">${escapeHTML(title)}</h2>
        <p>${escapeHTML(text)}</p>
        <div class="confirm-actions">
          <button type="button" class="button secondary" id="btn-confirm-cancel">Cancelar</button>
          <button type="button" class="button ${isDanger ? 'danger' : 'primary'}" id="btn-confirm-ok">${escapeHTML(confirmBtnText)}</button>
        </div>
      </div>
    `;
    const btnCancel = dialog.querySelector('#btn-confirm-cancel');
    const btnOk = dialog.querySelector('#btn-confirm-ok');

    const clean = (ans) => {
      dialog.close();
      resolve(ans);
    };

    btnCancel.onclick = () => clean(false);
    btnOk.onclick = () => clean(true);
    dialog.showModal();
  });
}

/* Encerramento do Servidor C (Requisito 1) */
async function shutdownSystem() {
  const confirmed = await askConfirm(
    'Encerrar o sistema?',
    'Isso fechará o executável em C. Todos os dados serão preservados no arquivo SQLite.',
    'Encerrar servidor',
    true
  );
  if (!confirmed) return;

  try {
    await api('/api/shutdown', { method: 'POST' });
    document.body.innerHTML = `
      <div style="display:flex;align-items:center;justify-content:center;height:100vh;flex-direction:column;font-family:sans-serif;color:#475467;text-align:center;">
        <h1 style="color:#1d2939;margin-bottom:8px;">Servidor Encerrado</h1>
        <p>Os dados foram preservados no banco SQLite.</p>
        <p>Para reiniciar, execute <code>./build/vendas</code> no terminal.</p>
      </div>
    `;
  } catch (err) {
    toast('Falha ao encerrar: ' + err.message, true);
  }
}

/* Eventos Globais Comuns */
document.addEventListener('DOMContentLoaded', () => {
  // Data atual no topo
  const dateEl = document.getElementById('current-date');
  if (dateEl) {
    dateEl.textContent = new Intl.DateTimeFormat('pt-BR', { day: 'numeric', month: 'long', year: 'numeric' }).format(new Date());
  }

  // Fecha modal ao clicar em fechar
  document.addEventListener('click', (e) => {
    if (e.target.closest('[data-action="close-modal"]')) {
      closeModal();
    }
    if (e.target.closest('[data-action="shutdown"]')) {
      shutdownSystem();
    }
  });
});
