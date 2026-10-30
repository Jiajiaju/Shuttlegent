/*
 * Shuttlegent panel page (W1 shell, W2 structure, W3 input and references, M3.5 queue, M3.6 write confirmation and results;
 * Shuttlegent.md sections 14.3.0, 14.3.3–14.3.6).
 *
 * The editor pushes state through window.Shuttlegent.apply(message): a full snapshot (reset) after the page calls
 * bridge.ready(), then per-frame deltas (FShuttlegentWebProtocol, field names in ShuttlegentEditorWebField). The page
 * keeps one state object and redraws only the regions a message touched. Commands go back through window.ue.bridge.
 */
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const icons = window.ShuttlegentIcons;
  const markdown = window.ShuttlegentMarkdown;
  const markPlayer = window.ShuttlegentMark;
  const esc = markdown.escapeHtml;

  // Distance from the bottom, in CSS pixels, within which new content keeps the view pinned to the end
  const FOLLOW_THRESHOLD = 24;

  // Consecutive read-only tool calls fold into one group from this many on
  const MIN_GROUP_SIZE = 2;

  // Tallest the draft grows before it scrolls inside the input, in CSS pixels
  const DRAFT_MAX_HEIGHT = 200;

  // How often the page looks for the editor binding before calling ready, in milliseconds
  const BRIDGE_POLL_MS = 50;

  // Panel widths where the layout changes (Shuttlegent UI interaction spec section 2)
  const BREAKPOINT_MEDIUM = 560;
  const BREAKPOINT_WIDE = 900;
  const BREAKPOINT_FULL = 1180;

  const DAY_MS = 24 * 60 * 60 * 1000;

  // Docked pane widths in CSS pixels: default, narrowest and widest a drag can make them
  const SIDEBAR_DEFAULT_WIDTH = 248;
  const SIDEBAR_MIN_WIDTH = 180;
  const SIDEBAR_MAX_WIDTH = 480;
  const INSPECTOR_DEFAULT_WIDTH = 360;
  const INSPECTOR_MIN_WIDTH = 280;
  const INSPECTOR_MAX_WIDTH = 720;

  // Narrowest the conversation gets when the docked panes are dragged wider, in CSS pixels (.workbench.full .chat)
  const CHAT_MIN_WIDTH = 320;

  // Width change per arrow key on a focused pane grip, in CSS pixels
  const RESIZE_KEY_STEP = 16;

  // Context usage shares where the ring turns to the warning and then the error colour
  const USAGE_HIGH_SHARE = 0.8;
  const USAGE_FULL_SHARE = 0.95;

  // Least distance kept between the session menu and the panel edge, in CSS pixels
  const MENU_EDGE_MARGIN = 8;

  // Pause after typing in the @ menu before the editor is asked for candidates, in milliseconds
  const MENU_QUERY_DELAY_MS = 80;

  // Tokens that open a menu: @query anywhere after whitespace, /query at the start of a line
  const REFERENCE_TOKEN = /(^|\s)@([^\s@]*)$/;
  const TEMPLATE_TOKEN = /(^|\n)\/([^\s/]*)$/;

  const TOOL_COMPLETED = 'completed';
  const TOOL_FAILED = 'failed';

  // How often the write confirmation card's countdown updates
  const COUNTDOWN_INTERVAL_MS = 1000;

  const STATUS_TEXT = {
    notConnected: '未连接',
    connecting: '正在连接',
    connected: '已连接',
    querying: '正在查询',
    awaitingPermission: '等待本次授权',
    awaitingWrite: '等待写操作确认',
    writing: '正在修改',
    stopping: '正在停止',
    stopped: '已停止',
    failed: '查询失败',
    lost: '连接已断开',
    resuming: '正在恢复会话',
    startBlocked: '运行时无法启动',
    resumeFailed: '恢复会话失败'
  };
  const WARNING_STATUSES = new Set(['awaitingPermission', 'awaitingWrite', 'failed', 'lost', 'startBlocked', 'resumeFailed']);

  // Background sessions in the sidebar: running ones, ones waiting for a confirmation (Shuttlegent.md section 14.3.7)
  const RUNNING_STATUSES = new Set(['connecting', 'querying', 'writing', 'stopping', 'resuming']);
  const CONFIRM_STATUSES = new Set(['awaitingPermission', 'awaitingWrite']);

  const state = {
    status: 'notConnected',
    connected: false,
    prompting: false,
    stopping: false,
    busy: false,
    runtimeLost: false,
    startIssue: null,
    resumeFailed: false,
    needsVerify: false,
    animateMark: true,
    usedTokens: 0,
    contextSize: 0,
    config: {},
    entries: new Map(),
    order: [],
    sessions: [],
    archived: [],
    currentSessionId: '',
    currentTitle: '',
    canArchiveCurrent: false,
    renameMaxChars: 60,
    permissionMode: 'readOnly',
    shellEnabled: false,
    templates: [],
    runLog: [],
    pendingPermission: null,
    pendingWrite: null,
    queue: { paused: false, items: [] },
    canRetry: false
  };

  // Page-local view state
  const expandedThoughts = new Set();
  const expandedTools = new Set();
  const blockElements = new Map();
  const codeSources = new Map();
  const view = {
    layout: 'full',
    // Docked panes remember whether they stay open; overlays open fresh each time
    docked: { sidebar: true, inspector: true },
    overlay: null,
    tab: 'changes',
    selectedTool: null,
    shownDecisionKey: null,
    answeringDecisionKey: null,
    // Elements focus returns to as dialogs close; a dialog can open over another (delete over the archived list)
    dialogOpeners: [],

    // Docked pane widths the user dragged; 0 means the default
    paneWidths: { sidebar: 0, inspector: 0 },

    // Session the right-click menu, and the rename, archive or delete dialog opened from it, act on
    menuSessionId: '',
    targetSessionId: '',

    // Drafts kept per session: switching sessions neither moves nor clears them (interaction spec section 4.3)
    drafts: new Map(),
    switchingSession: false,

    // Countdown of the write confirmation card
    countdownTimer: 0
  };

  // Draft input: reference chips, history browsing, the open menu, and a send waiting for the editor's answer
  const draftState = {
    refs: [],
    historyIndex: -1,
    historyOriginal: '',
    submitting: false,

    // What was handed to the editor; it comes back into the draft only if the editor refuses it
    submittedText: '',
    submittedRefs: []
  };
  const menu = {
    mode: null,
    query: '',
    tokenStart: 0,
    sections: [],
    items: [],
    index: 0,
    timer: 0
  };
  let changedIds = new Set();
  let wasWorking = false;
  let emptyMark = null;
  let headerMark = null;
  let hasUnseenContent = false;

  function bridge() {
    return window.ue && window.ue.bridge ? window.ue.bridge : null;
  }

  function call(command, ...args) {
    const target = bridge();
    if (target && typeof target[command] === 'function') target[command](...args);
  }

  function localDay(time) {
    const date = new Date(time);
    return new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime();
  }

  function pad(value) {
    return String(value).padStart(2, '0');
  }

  function clock(time, seconds) {
    const date = new Date(time);
    return pad(date.getHours()) + ':' + pad(date.getMinutes()) + (seconds ? ':' + pad(date.getSeconds()) : '');
  }

  /** Today: 14:05; yesterday: 昨天 14:05; earlier: 10-21. */
  function sessionTime(time) {
    const today = localDay(Date.now());
    const day = localDay(time);
    if (day === today) return clock(time, false);
    if (day === today - DAY_MS) return '昨天 ' + clock(time, false);
    const date = new Date(time);
    return pad(date.getMonth() + 1) + '-' + pad(date.getDate());
  }

  // ---------- Conversation ----------

  function isReadOnlyTool(entry) {
    return entry && entry.kind === 'toolCall' && entry.readOnly;
  }

  function isToolFinished(status) {
    return status === TOOL_COMPLETED || status === TOOL_FAILED;
  }

  /** Splits the entry order into blocks: one entry, or a group of consecutive read-only tool calls. */
  function buildBlocks() {
    const blocks = [];
    for (let index = 0; index < state.order.length; ++index) {
      const group = [];
      while (index + group.length < state.order.length && isReadOnlyTool(state.entries.get(state.order[index + group.length]))) {
        group.push(state.order[index + group.length]);
      }
      if (group.length >= MIN_GROUP_SIZE) {
        blocks.push({ key: 'g' + group[0], ids: group });
        index += group.length - 1;
        continue;
      }
      blocks.push({ key: 'e' + state.order[index], ids: [state.order[index]] });
    }
    return blocks;
  }

  function toolIcon(status) {
    if (status === TOOL_COMPLETED) return '<span class="state-icon ok">' + icons.svg('check', 14) + '</span>';
    if (status === TOOL_FAILED) return '<span class="state-icon failed">' + icons.svg('alert', 14) + '</span>';
    return '<span class="state-icon running">' + icons.svg('loader', 14) + '</span>';
  }

  function toolTrailer(status, readOnly, objects, failed) {
    if (!isToolFinished(status)) return '正在执行 ›';
    if (failed > 0) return failed + ' 项失败 ›';
    if (readOnly) return '只读 · ' + objects + ' 个对象 ›';
    return '已完成 ›';
  }

  function renderUser(entry) {
    const references = entry.references && entry.references.length ? '<div class="user-references">' + entry.references.map((line) => '<div>' + esc(line) + '</div>').join('') + '</div>' : '';
    return '<article class="message user">' + esc(entry.text) + references + '</article>';
  }

  function isThinking(entry) {
    return state.prompting && !entry.text && state.order.length > 0 && state.order[state.order.length - 1] === entry.id;
  }

  function renderThought(entry) {
    if (!entry.thought) return '';

    const expanded = expandedThoughts.has(entry.id);
    let label = entry.thoughtSeconds >= 0 ? '已思考 ' + entry.thoughtSeconds + ' 秒' : '思考过程';
    if (isThinking(entry)) label = '思考中…';
    const chevron = '<span class="thought-chevron' + (expanded ? ' open' : '') + '">' + icons.svg('chevron', 12) + '</span>';
    const body = expanded ? '<div class="thought-body">' + esc(entry.thought) + '</div>' : '';
    return '<div class="note thought"><button class="thought-toggle" data-thought="' + entry.id + '" aria-expanded="' + expanded + '" title="展开或收起思考过程">' + label + chevron + '</button>' + body + '</div>';
  }

  function renderAssistant(entry) {
    const rendered = markdown.render(entry.text);
    codeSources.set(entry.id, rendered.codes);
    const header = '<div class="assistant-name"><img src="../png/mark-red-white.png" alt="">Shuttlegent</div>';
    return '<article class="message assistant" data-entry="' + entry.id + '">' + header + renderThought(entry) + '<div class="markdown">' + rendered.html + '</div></article>';
  }

  function renderTool(entry) {
    const failed = entry.toolStatus === TOOL_FAILED;
    const reason = failed && entry.failureReason ? '<div class="tool-failure">' + esc(entry.failureReason) + '</div>' : '';
    return '<div class="tool"><button class="tool-row" data-tool="' + entry.id + '" title="在检视窗格查看参数与结果">' + toolIcon(entry.toolStatus) + '<span class="tool-name">' + esc(entry.displayName) + '</span><span class="tool-summary">' + esc(entry.summary) + '</span><small>' + toolTrailer(entry.toolStatus, entry.readOnly, 1, failed ? 1 : 0) + '</small></button>' + reason + '</div>';
  }

  function renderToolGroup(ids) {
    let status = TOOL_COMPLETED;
    let failed = 0;
    let running = false;
    for (const id of ids) {
      const entry = state.entries.get(id);
      if (!isToolFinished(entry.toolStatus)) running = true;
      if (entry.toolStatus === TOOL_FAILED) ++failed;
      if (entry.toolStatus !== TOOL_COMPLETED) status = entry.toolStatus;
    }
    const trailer = toolTrailer(running ? '' : TOOL_COMPLETED, true, ids.length, failed);
    return '<div class="tool"><button class="tool-row" data-tool="' + ids[0] + '" title="在检视窗格查看这组查询">' + toolIcon(status) + '<span class="tool-name">已查询 ' + ids.length + ' 项</span><span class="tool-summary"></span><small>' + trailer + '</small></button></div>';
  }

  function renderPermission(entry) {
    let suffix = '等待本次授权';
    if (entry.answered) suffix = entry.allowed ? '已允许本次' : '已拒绝';
    return '<div class="note' + (entry.answered ? '' : ' warning') + '">运行时请求额外权限：' + esc(entry.displayName) + ' · ' + suffix + '</div>';
  }

  function writeObjectRow(object) {
    let icon = icons.svg('check', 12);
    let label = '已完成';
    if (object.status === 'failed') {
      icon = icons.svg('alert', 12);
      label = '失败';
    } else if (object.status === 'notRun') {
      icon = icons.svg('partial', 12);
      label = '未执行';
    } else if (object.status === 'rolledBack') {
      icon = icons.svg('undo', 12);
      label = '已回滚';
    }
    const change = object.field ? esc(object.field) + ' ' + esc(object.current) + ' → ' + esc(object.planned) : '';
    const message = object.message ? '<span class="dim"> · ' + esc(object.message) + '</span>' : '';
    return '<li class="write-object ' + object.status + '"><span class="state">' + icon + label + '</span><span class="name" title="' + esc(object.path) + '">' + esc(object.name) + '</span><span class="change">' + change + message + '</span></li>';
  }

  /** Result bar of an editor write (interaction spec sections 5.2, 5.4): real per-object outcome, undo only while its transaction is on top. */
  function renderWriteResult(entry) {
    const objects = entry.objects || [];
    const completed = objects.filter((object) => object.status === 'completed').length;
    let title = icons.svg('check', 14) + ' 已完成 · ' + completed + ' 个对象';
    let warn = false;
    const rolledBack = objects.some((object) => object.status === 'rolledBack');
    if (entry.undone) {
      title = icons.svg('undo', 14) + ' 已撤销本次修改';
    } else if (rolledBack) {
      warn = true;
      title = icons.svg('alert', 14) + ' 失败，已全部回滚 · ' + objects.length + ' 个对象';
    } else if (completed < objects.length) {
      warn = true;
      title = completed > 0 ? icons.svg('partial', 14) + ' 部分完成 · 已完成 ' + completed + ' / ' + objects.length + ' 个对象' : icons.svg('alert', 14) + ' 未完成 · 0 / ' + objects.length + ' 个对象';
    }

    const undoLabel = entry.undone ? '已撤销' : entry.canUndo ? '撤销本次修改' : '撤销不可用';
    const undo = entry.undoable ? '<div class="actions start"><button data-undo-write="' + entry.id + '" ' + (entry.canUndo ? '' : 'disabled') + ' title="' + esc(entry.undoReason) + '">' + undoLabel + '</button></div><p class="fine">' + esc(entry.undoReason) + '</p>' : '<p class="fine">此操作不可撤销。</p>';
    return '<article class="message"><div class="result' + (warn ? ' warn' : '') + '"><h3>' + title + '</h3><p>' + esc(entry.action) + '</p><ul class="write-objects">' + objects.map(writeObjectRow).join('') + '</ul>' + undo + '</div></article>';
  }

  /** A confirmed write whose result never arrived (interaction spec section 5.3): what was confirmed, verified by the user before sending again. */
  function renderWriteUnknown(entry) {
    const objects = entry.objects || [];
    const rows = objects.map((object) => {
      const change = object.field ? esc(object.field) + ' ' + esc(object.current) + ' → ' + esc(object.planned) : '';
      return '<li class="write-object unknown"><span class="state">' + icons.svg('alert', 12) + '未知</span><span class="name" title="' + esc(object.path) + '">' + esc(object.name) + '</span><span class="change">' + change + '</span></li>';
    }).join('');
    const button = '<div class="actions start"><button data-ack-write="' + entry.id + '" ' + (entry.answered ? 'disabled' : '') + '>' + (entry.answered ? '已核对' : '已核对，可以继续') + '</button></div>';
    const hint = entry.answered ? '已核对。' : '运行时在写入途中断开，不知道以下对象是否已经修改，也不会自动重放。请在关卡中核对这些对象，再点“已核对”；核对前不能发送新消息。';
    return '<article class="message"><div class="result warn"><h3>' + icons.svg('alert', 14) + ' 结果未知 · ' + objects.length + ' 个对象</h3><p>' + esc(entry.action) + '</p><ul class="write-objects">' + rows + '</ul><p class="fine">' + hint + '</p>' + button + '</div></article>';
  }

  function renderNotice(entry) {
    return '<div class="note' + (entry.error ? ' error' : '') + '">' + esc(entry.text) + '</div>';
  }

  function renderBlock(block) {
    if (block.ids.length > 1) return renderToolGroup(block.ids);

    const entry = state.entries.get(block.ids[0]);
    switch (entry.kind) {
      case 'user':
        return renderUser(entry);
      case 'assistant':
        return renderAssistant(entry);
      case 'toolCall':
        return renderTool(entry);
      case 'permission':
        return renderPermission(entry);
      case 'writeResult':
        return renderWriteResult(entry);
      case 'writeUnknown':
        return renderWriteUnknown(entry);
      default:
        return renderNotice(entry);
    }
  }

  function isAtEnd() {
    const scroller = $('conversation');
    return scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight <= FOLLOW_THRESHOLD;
  }

  function scrollToEnd() {
    const scroller = $('conversation');
    scroller.scrollTop = scroller.scrollHeight;
    hasUnseenContent = false;
    $('back-bottom').hidden = true;
  }

  // An empty conversation centres the composer and perches the large mark on its top left edge
  function renderEmptyState(empty) {
    $('chat').classList.toggle('is-empty', empty);
    if (!empty) {
      if (emptyMark) markPlayer.detach(emptyMark);
      emptyMark = null;
      $('perch').replaceChildren();
      return;
    }
    if (emptyMark) return;

    $('perch').innerHTML = '<div id="empty-mark" role="img" aria-label="Shuttlegent"></div>';
    emptyMark = markPlayer.attach($('empty-mark'), 'l', $('chat'));
    markPlayer.play(emptyMark, 'intro');
    updateMarkOpacity();
  }

  /** Rebuilds the blocks whose entries changed (all on reset), drops removed ones and restores the order. */
  function renderConversation(reset) {
    const atEnd = isAtEnd();
    const column = $('messages');
    const blocks = buildBlocks();
    const liveKeys = new Set();

    let previous = null;
    for (const block of blocks) {
      liveKeys.add(block.key);
      const signature = block.ids.join(',');
      const first = state.entries.get(block.ids[0]);
      const thinking = first.kind === 'assistant' ? String(isThinking(first)) : '';
      let element = blockElements.get(block.key);
      const stale = reset || !element || element.dataset.ids !== signature || block.ids.some((id) => changedIds.has(id)) || element.dataset.thinking !== thinking;
      if (stale) {
        const holder = document.createElement('div');
        holder.innerHTML = renderBlock(block);
        const fresh = holder.firstElementChild;
        fresh.dataset.ids = signature;
        fresh.dataset.key = block.key;
        fresh.dataset.thinking = thinking;
        if (element) element.replaceWith(fresh);
        element = fresh;
        blockElements.set(block.key, element);
      }

      // Keep DOM order equal to block order
      const expectedNext = previous ? previous.nextElementSibling : column.querySelector('[data-key]');
      if (expectedNext !== element) {
        if (previous) previous.after(element);
        else column.prepend(element);
      }
      previous = element;
    }

    for (const [key, element] of blockElements) {
      if (liveKeys.has(key)) continue;
      element.remove();
      blockElements.delete(key);
    }
    for (const id of codeSources.keys()) {
      if (!state.entries.has(id)) codeSources.delete(id);
    }

    renderEmptyState(state.order.length === 0);

    // Streamed output never steals focus and never forces the view back to the end
    if (atEnd) {
      scrollToEnd();
    } else if (!reset) {
      hasUnseenContent = true;
      $('back-bottom').hidden = false;
    }
  }

  // ---------- Header, banner, composer, decision card ----------

  function updateMarkOpacity() {
    document.documentElement.classList.toggle('disconnected', !state.connected);
  }

  function renderHeader() {
    const status = $('status');
    status.textContent = STATUS_TEXT[state.status] || '';
    status.classList.toggle('warning', WARNING_STATUSES.has(state.status));
    updateMarkOpacity();

    // Working loops while a turn runs and nothing waits for the user; a confirmation, a stop or a lost runtime rests it
    const working = state.prompting && !state.pendingWrite && !state.pendingPermission;
    if (working && !wasWorking) markPlayer.play(headerMark, 'working');
    if (!working && wasWorking && headerMark.motion === 'working') markPlayer.rest(headerMark);
    wasWorking = working;
  }

  function renderTitle() {
    $('session-title').textContent = state.currentTitle || '新会话';
    $('session-title').title = state.currentTitle || '新会话';
  }

  function renderQueue() {
    const queue = $('queue');
    const items = state.queue.items || [];
    queue.hidden = items.length === 0;
    if (!items.length) {
      queue.innerHTML = '';
      return;
    }

    const paused = state.queue.paused;
    const canContinue = !state.busy && !state.runtimeLost;
    const header = paused
      ? '<header class="paused"><span>队列已暂停 · ' + items.length + '</span><button id="resume-queue" ' + (canContinue ? '' : 'disabled') + ' title="' + (canContinue ? '按顺序发送剩余条目' : '当前轮次结束或重新连接后才能继续') + '">继续队列</button></header>'
      : '<header><span>待发送 · ' + items.length + '</span><span>完成后按顺序发送</span></header>';
    queue.innerHTML = header + items.map((item, index) => {
      const refs = item.references && item.references.length ? '<small>' + item.references.length + ' 个引用</small>' : '';
      return '<div class="queue-item"><span class="queue-text" title="' + esc(item.text) + '">' + (index + 1) + '. ' + esc(item.text) + '</span>' + refs + '<button data-take-queued="' + item.id + '" title="取回为草稿；当前草稿会放到这一条的位置">取回</button><button class="icon small" data-remove-queued="' + item.id + '" aria-label="移除第 ' + (index + 1) + ' 条排队消息">' + icons.svg('close', 12) + '</button></div>';
    }).join('');
  }

  function draftReferencesJson() {
    return JSON.stringify(draftState.refs.map((ref) => ({ kind: ref.kind, path: ref.path, name: ref.name, cls: ref.cls })));
  }

  /** The queued item comes back into the draft; whatever was in the draft already went into its place. */
  function onQueueItemTaken(item) {
    draftState.refs = (item.references || []).map((ref) => ({ kind: ref.kind, path: ref.path, name: ref.name, cls: ref.cls, valid: true }));
    draftState.historyIndex = -1;
    setDraft(item.text || '');
    renderReferences();
    watchReferences();
    $('draft').focus();
  }

  /** Each session keeps its own draft; a session the page did not switch to (a new one just created) keeps the current draft. */
  function switchDraft(previousId, nextId) {
    if (!view.switchingSession) return;
    view.switchingSession = false;

    view.drafts.set(previousId, { text: $('draft').value, refs: draftState.refs });
    const saved = view.drafts.get(nextId) || { text: '', refs: [] };
    view.drafts.delete(nextId);
    draftState.refs = saved.refs;
    draftState.historyIndex = -1;
    setDraft(saved.text);
    renderReferences();
    watchReferences();
    closeMenu();
  }

  /** Start guide card (interaction spec section 5.3): what blocks the runtime and where to fix it; never a key value. */
  function startGuide(issue) {
    const code = (text) => '<code>' + esc(text) + '</code>';
    switch (issue.kind) {
      case 'missingKey':
        return { title: '未找到 ' + esc(issue.provider) + ' 密钥', body: '在“编辑器偏好设置 > Shuttlegent > General > Providers”中填写 ' + esc(issue.provider) + ' 的 API 密钥（和端点 URL），然后点“重试”。', settings: true };
      case 'authFailed':
        return { title: esc(issue.provider) + ' 拒绝了当前密钥', body: '鉴权失败，密钥可能有误或已失效。在“编辑器偏好设置 > Shuttlegent > General > Providers”中改正密钥和端点 URL，然后点“重试”，运行时会重新启动并读取新设置。', settings: true };
      case 'missingRuntime':
        return { title: '未找到运行时', body: '期望位置：' + code(issue.runtimePath) + '<br>在插件目录运行 ' + code(issue.buildScript) + ' 构建运行时，或在编辑器偏好设置中指定运行时路径，然后点“重试”。', folder: true };
      default:
        return { title: '运行时启动失败', body: issue.detail ? code(issue.detail) : '查看 Output Log 中的 LogShuttlegent 了解原因。', folder: false };
    }
  }

  function renderBanner() {
    const banner = $('banner');
    banner.classList.remove('guide');
    if (state.startIssue) {
      const guide = startGuide(state.startIssue);
      banner.classList.add('guide');
      banner.innerHTML = '<div class="guide-text"><strong>' + guide.title + '</strong><p>' + guide.body + '</p></div><div class="guide-actions">' + (guide.settings ? '<button id="open-guide-settings">打开偏好设置</button>' : '') + (guide.folder ? '<button id="open-plugin-folder">打开插件目录</button>' : '') + '<button id="retry-start" class="primary">重试</button></div>';
      banner.hidden = false;
      if (guide.settings) $('open-guide-settings').addEventListener('click', () => call('opensettings'));
      if (guide.folder) $('open-plugin-folder').addEventListener('click', () => call('openpluginfolder'));
      $('retry-start').addEventListener('click', () => call('retrystart'));
      return;
    }
    if (state.runtimeLost) {
      banner.innerHTML = '<span>连接中断，草稿与队列已保留。</span><button id="reconnect">重新连接</button>';
      banner.hidden = false;
      $('reconnect').addEventListener('click', () => call('reconnect'));
      return;
    }
    if (state.resumeFailed && state.status !== 'resuming') {
      banner.innerHTML = '<span>无法恢复此会话的运行时上下文，本地记录仍保留。新建会话不会带上之前对话的上下文，当前草稿会带过去。</span><span><button id="retry-resume">重试恢复</button> <button id="resume-new">新建会话</button></span>';
      banner.hidden = false;
      $('retry-resume').addEventListener('click', () => call('retryresume'));

      // Not a page-initiated switch, so the current draft stays in the composer of the new session
      $('resume-new').addEventListener('click', () => call('newconversation'));
      return;
    }
    if (state.status === 'failed') {
      banner.innerHTML = '<span>查询失败，尚未执行修改。草稿和队列已保留。</span>' + (state.canRetry ? '<button id="retry">重试查询</button>' : '');
      banner.hidden = false;
      if (state.canRetry) $('retry').addEventListener('click', () => call('retry'));
      return;
    }
    banner.hidden = true;
    banner.innerHTML = '';
  }

  function renderSelect(select, configId) {
    const option = state.config[configId] || { current: '', choices: [] };
    const choices = option.choices || [];
    select.innerHTML = choices.length ? choices.map((choice) => '<option value="' + esc(choice.value) + '">' + esc(choice.name) + '</option>').join('') : '<option value="">—</option>';
    select.value = choices.length ? option.current : '';
    select.disabled = choices.length === 0 || state.prompting;
  }

  /** Context usage as a ring: the arc is the used share, the colour warns as the context fills; the numbers are in the tooltip. */
  function renderUsage() {
    const usage = $('usage');
    const known = state.contextSize > 0;
    const share = known ? Math.min(state.usedTokens / state.contextSize, 1) : 0;
    const percent = Math.round(share * 100);
    $('usage-bar').style.strokeDasharray = percent + ' 100';
    usage.classList.toggle('high', known && share >= USAGE_HIGH_SHARE && share < USAGE_FULL_SHARE);
    usage.classList.toggle('full', known && share >= USAGE_FULL_SHARE);
    usage.title = known ? '上下文已用 ' + percent + '% · ' + state.usedTokens.toLocaleString() + ' / ' + state.contextSize.toLocaleString() + ' tokens' : '上下文用量未知';
    usage.setAttribute('aria-label', usage.title);
  }

  function hasInvalidReference() {
    return draftState.refs.some((ref) => ref.valid === false);
  }

  function updateSendButton() {
    const send = $('send');
    const invalid = hasInvalidReference();
    send.disabled = !$('draft').value.trim() || invalid || draftState.submitting || state.runtimeLost || !!state.startIssue || state.needsVerify;
    if (invalid) send.title = '有引用已失效：移除或重新添加后再发送';
    else if (state.startIssue) send.title = '运行时无法启动：按上方提示处理后重试';
    else if (state.needsVerify) send.title = '有写入结果未知：核对对象并点“已核对”后再发送';
    else if (state.runtimeLost) send.title = '连接已断开：重新连接后再发送';
    else send.title = state.busy ? '加入队列 · Enter' : '发送 · Enter';
  }

  function renderComposer() {
    const send = $('send');
    const stop = $('stop');

    // While a turn runs the send button queues; stop stays next to it
    send.classList.toggle('queued', state.busy);
    send.innerHTML = state.busy ? '排队' : icons.svg('send', 18);
    send.setAttribute('aria-label', state.busy ? '加入队列' : '发送消息');
    updateSendButton();

    let keys = 'Enter 发送 · Shift+Enter 换行';
    if (state.busy) keys = 'Enter 加入队列 · 停止不会清空草稿';
    if (state.runtimeLost) keys = '草稿已保留，重新连接后再发送';
    if (state.needsVerify) keys = '草稿已保留，核对写入结果后再发送';
    if (state.startIssue) keys = '草稿已保留，运行时启动后再发送';
    $('input-hint-keys').textContent = keys;
    stop.hidden = !state.prompting;
    stop.disabled = state.stopping;
    stop.classList.toggle('is-stopping', state.stopping);
    stop.innerHTML = icons.svg(state.stopping ? 'loader' : 'stop', 14);
    stop.title = state.stopping ? '正在停止，等待当前调用结束' : '停止本轮；已执行的修改不会回滚';
    stop.setAttribute('aria-label', state.stopping ? '停止中' : '停止');
    renderSelect($('model'), 'model');
    renderSelect($('reasoning'), 'reasoning_effort');
    renderUsage();
  }

  function prettyRaw(text) {
    try {
      return JSON.stringify(JSON.parse(text), null, 2);
    } catch (error) {
      return text || '';
    }
  }

  /** Editor write card (interaction spec section 5.1): action, objects with current and planned values, level, undo, raw arguments. */
  function writeCardHtml(request) {
    const rows = request.objects.map((object) => '<tr><td title="' + esc(object.path) + '">' + esc(object.name) + '</td><td>' + esc(object.field) + '</td><td>' + esc(object.current) + '</td><td>' + esc(object.planned) + '</td></tr>').join('');
    const badge = request.undoable ? '同步事务 · 可撤销' : '不可撤销';
    const level = request.level ? ' · ' + esc(request.level) : '';
    return '<header><h3 id="decision-title" tabindex="-1">' + esc(request.action) + '</h3><span class="badge' + (request.undoable ? '' : ' warn') + '">' + badge + '</span></header>'
      + '<p>影响 ' + request.objects.length + ' 个对象' + level + ' · 工具 <code>' + esc(request.tool) + '</code></p>'
      + '<div class="decision-table"><table><thead><tr><th>对象</th><th>字段</th><th>当前</th><th>计划</th></tr></thead><tbody>' + rows + '</tbody></table></div>'
      + '<details><summary>原始参数</summary><pre>' + esc(prettyRaw(request.raw)) + '</pre></details>'
      + '<p class="fine">仅确认这一次操作。对象、关卡或参数变化后需要重新确认。<span id="decision-countdown"></span></p>'
      + '<div class="actions"><button id="decision-reject">取消本次操作</button><button id="decision-allow" class="primary">确认执行</button></div>';
  }

  function permissionCardHtml(request) {
    const target = request.target ? '<p><code>' + esc(request.target) + '</code></p>' : '';
    const content = request.content ? '<p>内容：' + esc(request.content) + '</p>' : '';
    const detail = request.detail ? '<p>说明：' + esc(request.detail) + '</p>' : '';
    return '<header><h3 id="decision-title" tabindex="-1">运行时请求额外权限</h3><span class="badge">不可撤销</span></header><p>操作：' + esc(request.displayName) + '</p>' + target + content + detail + '<p class="fine">本次授权只覆盖这一个请求，不会开放后续请求。拒绝后保留任务记录。</p><div class="actions"><button id="decision-reject">拒绝</button><button id="decision-allow" class="primary">允许本次</button></div>';
  }

  /** The request the card shows: an editor write first, else a runtime permission; null when nothing waits. */
  function currentDecision() {
    if (state.pendingWrite) return { key: 'w' + state.pendingWrite.id, kind: 'write', request: state.pendingWrite };
    if (state.pendingPermission) return { key: 'p' + state.pendingPermission.id, kind: 'permission', request: state.pendingPermission };
    return null;
  }

  /** The decision card replaces the composer while an editor write or a runtime permission request waits for an answer. */
  function renderDecision() {
    const decision = currentDecision();
    const card = $('decision');
    const waiting = decision !== null;
    $('composer').hidden = waiting;
    $('composer-meta').hidden = waiting;
    $('input-hint').hidden = waiting;
    card.hidden = !waiting;
    if (!waiting || decision.kind !== 'write') stopCountdown();
    if (!waiting) {
      // Focus that sat on the card goes back to the draft once it is answered
      const hadFocus = card.contains(document.activeElement);
      card.innerHTML = '';
      view.shownDecisionKey = null;
      view.answeringDecisionKey = null;
      if (hadFocus) $('draft').focus();
      return;
    }
    if (view.shownDecisionKey === decision.key) return;

    // Focus that was on the previous card or in the composer moves to the new title; focus elsewhere (the viewport) is not stolen
    const focusWasOurs = $('composer').contains(document.activeElement) || card.contains(document.activeElement) || document.activeElement === document.body;
    card.classList.toggle('elevation', decision.kind === 'permission');
    card.innerHTML = decision.kind === 'write' ? writeCardHtml(decision.request) : permissionCardHtml(decision.request);
    $('decision-reject').addEventListener('click', () => answerDecision(false));
    $('decision-allow').addEventListener('click', () => answerDecision(true));
    view.shownDecisionKey = decision.key;
    if (decision.kind === 'write') startCountdown(decision.request.expiresAt);
    if (focusWasOurs && document.hasFocus()) $('decision-title').focus({ preventScroll: true });
  }

  /** The write gate times a request out unanswered (the runtime gives up on the call soon after); the card counts down to it. */
  function startCountdown(expiresAt) {
    stopCountdown();
    const tick = () => {
      const label = $('decision-countdown');
      if (!label) return;
      const seconds = Math.max(0, Math.ceil((expiresAt - Date.now()) / 1000));
      label.textContent = ' 剩余 ' + seconds + ' 秒，超时自动取消。';
    };
    tick();
    view.countdownTimer = window.setInterval(tick, COUNTDOWN_INTERVAL_MS);
  }

  function stopCountdown() {
    if (view.countdownTimer) window.clearInterval(view.countdownTimer);
    view.countdownTimer = 0;
  }

  function answerDecision(allow) {
    const decision = currentDecision();
    if (!decision || view.answeringDecisionKey === decision.key) return;

    // One answer per request: repeated clicks do nothing
    view.answeringDecisionKey = decision.key;
    for (const button of $('decision').querySelectorAll('button')) {
      button.disabled = true;
    }
    if (decision.kind === 'write') call('answerwrite', decision.request.id, allow);
    else call('answerpermission', decision.request.id, allow);
  }

  function growDraft() {
    const draft = $('draft');
    draft.style.height = 'auto';
    draft.style.height = Math.min(draft.scrollHeight, DRAFT_MAX_HEIGHT) + 'px';
    draft.style.overflowY = draft.scrollHeight > DRAFT_MAX_HEIGHT ? 'auto' : 'hidden';
    updateSendButton();
  }

  function setDraft(text) {
    const draft = $('draft');
    draft.value = text;
    growDraft();
    draft.setSelectionRange(text.length, text.length);
  }

  /** Sends the draft with a frozen copy of its chips; the draft clears only once the editor accepted it. */
  function submitDraft() {
    const text = $('draft').value;
    if (!text.trim() || state.runtimeLost || draftState.submitting || hasInvalidReference()) return;

    // While a turn runs the editor queues the draft instead of sending it; either way it answers through onSubmitResult
    // The draft clears at once so typing can go on; the editor answers through onSubmitResult
    const references = draftReferencesJson();
    draftState.submitting = true;
    draftState.submittedText = text;
    draftState.submittedRefs = draftState.refs;
    draftState.refs = [];
    draftState.historyIndex = -1;
    closeMenu();
    setDraft('');
    renderReferences();
    call('submit', text, references);
  }

  function onSubmitResult(result) {
    draftState.submitting = false;
    if (result.accepted) {
      updateSendButton();
      watchReferences();
      return;
    }

    // Refused: the sent draft comes back in front of anything typed since, its chips merged back
    const typed = $('draft').value;
    setDraft(typed ? draftState.submittedText + '\n' + typed : draftState.submittedText);
    const kept = new Set(draftState.refs.map((ref) => ref.path));
    draftState.refs = draftState.submittedRefs.filter((ref) => !kept.has(ref.path)).concat(draftState.refs);
    const invalid = new Set(result.invalid || []);
    for (const ref of draftState.refs) {
      if (invalid.has(ref.path)) ref.valid = false;
    }
    renderReferences();
    watchReferences();
  }

  // ---------- Reference chips ----------

  function renderReferences() {
    $('references').innerHTML = draftState.refs.map((ref, index) => {
      const invalid = ref.valid === false;
      const kind = ref.kind === 'actor' ? 'Actor' : '资产';
      const title = invalid ? '对象已删除或失效：移除或重新添加后再发送' : kind + ' · ' + ref.cls + ' · ' + ref.path;
      const alert = invalid ? icons.svg('alert', 12) : '';
      return '<span class="ref' + (invalid ? ' invalid' : '') + '" title="' + esc(title) + '">' + alert + '<span class="ref-name">' + esc(ref.name) + '</span><button data-remove-ref="' + index + '" aria-label="移除引用 ' + esc(ref.name) + '">' + icons.svg('close', 12) + '</button></span>';
    }).join('');
    updateSendButton();
  }

  /** Tells the editor which chips to keep checking; validity comes back through onReferenceValidity. */
  function watchReferences() {
    call('watchreferences', draftReferencesJson());
  }

  /** Adds references, de-duplicated by path (the stable identity), not by name. */
  function addReferences(items) {
    let added = false;
    for (const item of items) {
      if (!item.path || draftState.refs.some((ref) => ref.path === item.path)) continue;
      draftState.refs.push({ kind: item.kind, path: item.path, name: item.name || item.path, cls: item.cls || '', valid: true });
      added = true;
    }
    if (!added) return;

    renderReferences();
    watchReferences();
  }

  function removeReference(index) {
    draftState.refs.splice(index, 1);
    renderReferences();
    watchReferences();
  }

  function onReferenceValidity(validity) {
    for (const ref of draftState.refs) {
      if (ref.path in validity) ref.valid = validity[ref.path];
    }
    renderReferences();
  }

  // ---------- @ and / menus ----------

  function closeMenu() {
    window.clearTimeout(menu.timer);
    menu.mode = null;
    menu.items = [];
    $('suggestions').hidden = true;
    $('suggestions').innerHTML = '';
  }

  function renderMenu(sections, emptyText) {
    let html = '';
    let itemIndex = 0;
    for (const section of sections) {
      if (!section.items.length) continue;
      html += '<div class="menu-label">' + section.label + '</div>';
      for (const item of section.items) {
        html += '<button role="option" data-menu-item="' + itemIndex + '" aria-selected="' + (itemIndex === menu.index) + '" tabindex="-1">' + esc(item.label) + '<small>' + esc(item.detail) + '</small></button>';
        ++itemIndex;
      }
    }
    if (!html) html = '<div class="menu-empty">' + emptyText + '</div>';
    $('suggestions').innerHTML = html;
    $('suggestions').hidden = false;
    const selected = $('suggestions').querySelector('[aria-selected=true]');
    if (selected) selected.scrollIntoView({ block: 'nearest' });
  }

  /** Opens, narrows or closes the menu from the token right before the caret. */
  function updateMenu() {
    const draft = $('draft');
    const before = draft.value.slice(0, draft.selectionStart);
    const reference = REFERENCE_TOKEN.exec(before);
    const template = reference ? null : TEMPLATE_TOKEN.exec(before);
    const match = reference || template;
    if (!match || draft.selectionStart !== draft.selectionEnd) {
      closeMenu();
      return;
    }

    const mode = reference ? 'reference' : 'template';
    const query = match[2];
    if (menu.mode === mode && menu.query === query) return;

    menu.mode = mode;
    menu.query = query;
    menu.tokenStart = before.length - query.length - 1;
    menu.index = 0;
    if (mode === 'template') {
      // Templates come from the project's template directories with every snapshot
      const wanted = query.toLowerCase();
      const items = state.templates.filter((item) => item.name.toLowerCase().includes(wanted)).map((item) => ({ label: '/' + item.name, detail: item.description, body: item.body }));
      menu.sections = [{ label: '模板', items }];
      menu.items = items;
      renderMenu(menu.sections, state.templates.length ? '没有匹配的模板' : '暂无可用模板（项目设置 → Shuttlegent → Template Dirs）');
      return;
    }

    window.clearTimeout(menu.timer);
    menu.timer = window.setTimeout(() => call('queryreferences', query), MENU_QUERY_DELAY_MS);
  }

  function onReferenceResults(result) {
    if (menu.mode !== 'reference' || result.query !== menu.query) return;

    const selection = result.selection || [];
    const sections = [
      { label: '当前选择', items: selection.length ? [{ label: '当前选择', detail: selection.length + ' 个对象（选择快照）', refs: selection }] : [] },
      { label: '关卡 Actor', items: (result.actors || []).map((ref) => ({ label: ref.name, detail: 'Actor · ' + ref.cls, refs: [ref] })) },
      { label: '资产', items: (result.assets || []).map((ref) => ({ label: ref.name, detail: ref.cls + ' · ' + ref.path, refs: [ref] })) }
    ];
    menu.items = sections.flatMap((section) => section.items);
    menu.index = Math.min(menu.index, Math.max(0, menu.items.length - 1));
    renderMenu(sections, '没有匹配的对象');
    menu.sections = sections;
  }

  function moveMenu(step) {
    if (!menu.items.length) return;
    menu.index = (menu.index + step + menu.items.length) % menu.items.length;
    renderMenu(menu.sections || [], '');
  }

  /** Inserts the chosen item in place of the token: a template's text, or references as chips; nothing is sent. */
  function chooseMenu(index) {
    const item = menu.items[index];
    if (!item) return;

    const draft = $('draft');
    const caret = draft.selectionStart;
    const start = menu.tokenStart;
    const inserted = item.body || '';
    draft.value = draft.value.slice(0, start) + inserted + draft.value.slice(caret);
    closeMenu();
    draft.setSelectionRange(start + inserted.length, start + inserted.length);
    growDraft();
    if (item.refs) addReferences(item.refs);
    draft.focus();
  }

  // ---------- History ----------

  function sentPrompts() {
    return state.order.map((id) => state.entries.get(id)).filter((entry) => entry && entry.kind === 'user').map((entry) => entry.text);
  }

  /** Up and Down walk this conversation's sent prompts, only from an empty draft or while already walking them. */
  function browseHistory(step) {
    const draft = $('draft');
    const prompts = sentPrompts();
    if (!prompts.length) return false;
    if (draftState.historyIndex < 0 && draft.value !== '') return false;
    if (draftState.historyIndex < 0 && step > 0) return false;

    if (draftState.historyIndex < 0) {
      draftState.historyOriginal = draft.value;
      draftState.historyIndex = prompts.length;
    }
    const next = draftState.historyIndex + step;
    if (next < 0) return true;
    if (next >= prompts.length) {
      // Back past the newest prompt: the original draft returns
      draftState.historyIndex = -1;
      setDraft(draftState.historyOriginal);
      return true;
    }
    draftState.historyIndex = next;
    setDraft(prompts[next]);
    return true;
  }

  // ---------- Sidebar ----------

  function sessionSection(time) {
    const today = localDay(Date.now());
    const day = localDay(time);
    if (day === today) return '今天';
    if (day === today - DAY_MS) return '昨天';
    return '更早';
  }

  function backgroundActivity(session) {
    const parts = [];
    if (RUNNING_STATUSES.has(session.status)) parts.push('运行中');
    if (CONFIRM_STATUSES.has(session.status)) parts.push('等待确认');
    if (session.queuedCount) parts.push('排队 ' + session.queuedCount);
    return parts.join(' · ');
  }

  function renderSidebar() {
    const query = $('search').value.trim().toLowerCase();
    const visible = state.sessions.filter((session) => (session.title || '新会话').toLowerCase().includes(query));

    let html = '';
    let section = null;
    for (const session of visible) {
      const label = sessionSection(session.updatedAt);
      if (label !== section) {
        html += '<div class="section-label">' + label + '</div>';
        section = label;
      }

      // The current session shows how it runs; a background one that runs, waits or has queued prompts says so; the others show their model and last activity
      const current = session.id === state.currentSessionId;
      const activity = current ? '' : backgroundActivity(session);
      const detail = current ? STATUS_TEXT[state.status] || '' : activity || (session.modelName ? session.modelName + ' · ' : '') + sessionTime(session.updatedAt);
      const warning = (current && WARNING_STATUSES.has(state.status)) || (!current && CONFIRM_STATUSES.has(session.status)) ? ' class="warning"' : '';
      const title = session.title || '新会话';
      html += '<button class="session" data-session="' + esc(session.id) + '" aria-current="' + current + '" title="' + esc(title) + '"><span class="session-name">' + esc(title) + '</span><small' + warning + '>' + esc(detail) + '</small></button>';
    }
    if (!visible.length) html = '<p class="sessions-empty">' + (query ? '没有匹配的会话' : '还没有会话') + '</p>';

    $('sessions').innerHTML = html;
    $('archive-count').textContent = state.archived.length;
    const sandbox = state.permissionMode === 'workspaceWrite' ? '工作区可写' : '只读';
    $('permission-mode').textContent = '权限：' + sandbox + ' · Shell ' + (state.shellEnabled ? '开' : '关');
    renderArchivedList();
  }

  function renderArchivedList() {
    const list = $('archived-list');
    if (!state.archived.length) {
      list.innerHTML = '<p>没有已归档的会话</p>';
      return;
    }
    list.innerHTML = state.archived.map((session) => '<div class="archive-item"><span title="' + esc(session.title || '新会话') + '">' + esc(session.title || '新会话') + '</span><button data-restore="' + esc(session.id) + '">恢复</button><button data-delete="' + esc(session.id) + '">删除</button></div>').join('');
  }

  // ---------- Session menu ----------

  function findSession(id) {
    return state.sessions.find((session) => session.id === id) || state.archived.find((session) => session.id === id) || null;
  }

  function sessionButton(id) {
    return document.querySelector('#sessions [data-session="' + CSS.escape(id) + '"]');
  }

  /** Right-click menu of a session: rename always, archive and delete only while it neither runs nor waits for a confirmation. */
  function openSessionMenu(id, x, y) {
    const session = findSession(id);
    if (!session) return;

    view.menuSessionId = id;
    const menuElement = $('session-menu');
    for (const item of menuElement.querySelectorAll('[data-menu-action]')) {
      const blocked = item.dataset.menuAction !== 'rename' && session.canArchive === false;
      item.disabled = blocked;
      item.title = blocked ? '运行中或等待确认的会话不能归档或删除' : '';
    }

    menuElement.hidden = false;
    menuElement.style.left = Math.max(MENU_EDGE_MARGIN, Math.min(x, window.innerWidth - menuElement.offsetWidth - MENU_EDGE_MARGIN)) + 'px';
    menuElement.style.top = Math.max(MENU_EDGE_MARGIN, Math.min(y, window.innerHeight - menuElement.offsetHeight - MENU_EDGE_MARGIN)) + 'px';
    menuElement.querySelector('button:not(:disabled)').focus();
  }

  function closeSessionMenu(restoreFocus) {
    const menuElement = $('session-menu');
    if (menuElement.hidden) return;

    menuElement.hidden = true;
    const button = sessionButton(view.menuSessionId);
    if (restoreFocus && button) button.focus();
  }

  function runSessionAction(action) {
    const id = view.menuSessionId;
    closeSessionMenu(false);
    if (action === 'rename') openRename(id);
    else if (action === 'archive') openArchive(id);
    else if (action === 'delete') openDelete(id);
  }

  function moveSessionMenu(step) {
    const items = Array.from($('session-menu').querySelectorAll('button:not(:disabled)'));
    const index = items.indexOf(document.activeElement);
    items[(index + step + items.length) % items.length].focus();
  }

  // ---------- Pane widths ----------

  function paneLimits(pane) {
    if (pane === 'sidebar') return { min: SIDEBAR_MIN_WIDTH, max: SIDEBAR_MAX_WIDTH, fallback: SIDEBAR_DEFAULT_WIDTH };
    return { min: INSPECTOR_MIN_WIDTH, max: INSPECTOR_MAX_WIDTH, fallback: INSPECTOR_DEFAULT_WIDTH };
  }

  /** The remembered width, or the default, within the pane's own limits. */
  function preferredWidth(pane) {
    const limits = paneLimits(pane);
    return Math.max(limits.min, Math.min(view.paneWidths[pane] || limits.fallback, limits.max));
  }

  /** Room a pane may take: the panel less the other docked pane and the conversation's minimum. */
  function roomFor(pane) {
    const other = pane === 'sidebar' ? 'inspector' : 'sidebar';
    const otherWidth = paneMode(other) === 'docked' ? preferredWidth(other) : 0;
    return ($('workbench').clientWidth || window.innerWidth) - otherWidth - CHAT_MIN_WIDTH;
  }

  /** Width a docked pane gets now: its preferred width, narrowed so the conversation keeps CHAT_MIN_WIDTH, never below the pane's minimum. */
  function dockedWidth(pane) {
    return Math.max(paneLimits(pane).min, Math.min(preferredWidth(pane), roomFor(pane)));
  }

  function applyPaneWidths() {
    const workbench = $('workbench');
    for (const pane of ['sidebar', 'inspector']) {
      if (paneMode(pane) === 'docked') workbench.style.setProperty('--' + pane + '-width', dockedWidth(pane) + 'px');
    }
  }

  /** A drag or arrow key asked for this width: clamped like a docked width, it becomes the pane's remembered width. */
  function resizePane(pane, wanted) {
    const limits = paneLimits(pane);
    view.paneWidths[pane] = Math.round(Math.max(limits.min, Math.min(wanted, limits.max, roomFor(pane))));
    applyPaneWidths();
  }

  function savePaneWidths() {
    call('setpanewidths', view.paneWidths.sidebar, view.paneWidths.inspector);
  }

  // ---------- Panes and layout ----------

  function layoutForWidth(width) {
    if (width >= BREAKPOINT_FULL) return 'full';
    if (width >= BREAKPOINT_WIDE) return 'wide';
    if (width >= BREAKPOINT_MEDIUM) return 'medium';
    return 'narrow';
  }

  function isDocked(pane) {
    if (pane === 'sidebar') return view.layout === 'full';
    return view.layout === 'full' || view.layout === 'wide';
  }

  function paneMode(pane) {
    if (isDocked(pane)) return view.docked[pane] ? 'docked' : 'hidden';
    return view.overlay === pane ? 'overlay' : 'hidden';
  }

  /** Applies the layout of the current width: only where the panes sit changes, nothing is rebuilt. */
  function applyLayout() {
    const workbench = $('workbench');
    view.layout = layoutForWidth(workbench.clientWidth || window.innerWidth);
    if (view.overlay && isDocked(view.overlay)) view.overlay = null;

    workbench.classList.remove('full', 'wide', 'medium', 'narrow');
    workbench.classList.add(view.layout);
    $('sidebar').dataset.mode = paneMode('sidebar');
    $('inspector').dataset.mode = paneMode('inspector');
    $('scrim').hidden = view.overlay === null;

    // An overlay is modal: the rest of the panel cannot be reached until it closes
    $('chat').inert = view.overlay !== null;
    $('sidebar').inert = view.overlay === 'inspector';
    $('inspector').inert = view.overlay === 'sidebar';

    $('toggle-sidebar').setAttribute('aria-expanded', String(paneMode('sidebar') !== 'hidden'));
    $('toggle-inspector').setAttribute('aria-expanded', String(paneMode('inspector') !== 'hidden'));
    applyPaneWidths();
    if (paneMode('inspector') !== 'hidden') renderInspector();
  }

  function firstFocusable(pane) {
    if (pane === 'sidebar') return $('new-session');
    return document.querySelector('#tabs [aria-selected=true]') || $('tab-changes');
  }

  function openPane(pane) {
    if (isDocked(pane)) {
      view.docked[pane] = true;
      applyLayout();
      return;
    }

    // Overlays are mutually exclusive; focus enters the opened one
    view.overlay = pane;
    applyLayout();
    firstFocusable(pane).focus();
  }

  /** Closes the overlay; focus goes back to the header toggle that opens it. */
  function closeOverlay() {
    const pane = view.overlay;
    if (!pane) return;

    view.overlay = null;
    applyLayout();
    $(pane === 'sidebar' ? 'toggle-sidebar' : 'toggle-inspector').focus();
  }

  function togglePane(pane) {
    if (isDocked(pane)) {
      view.docked[pane] = !view.docked[pane];
      applyLayout();
      return;
    }
    if (view.overlay === pane) closeOverlay();
    else openPane(pane);
  }

  // ---------- Inspector ----------

  function toolStateText(status) {
    if (status === TOOL_COMPLETED) return '已完成';
    if (status === TOOL_FAILED) return '执行失败';
    return '正在执行';
  }

  function prettyJson(text) {
    if (!text) return '';
    try {
      return JSON.stringify(JSON.parse(text), null, 2);
    } catch (error) {
      return text;
    }
  }

  function renderToolDetails() {
    const tools = state.order.map((id) => state.entries.get(id)).filter((entry) => entry && entry.kind === 'toolCall');
    if (!tools.length) return '<p class="empty-note">本会话还没有工具调用。</p>';

    return tools.map((entry) => {
      const expanded = expandedTools.has(entry.id);
      const body = expanded ? '<div class="tool-detail-body"><h4>参数</h4><pre>' + esc(prettyJson(entry.input) || '（无参数）') + '</pre><h4>结果</h4><pre>' + esc(entry.result || '（尚无结果）') + '</pre></div>' : '';
      const selected = view.selectedTool === entry.id ? ' selected' : '';
      return '<div class="tool-detail' + selected + '" id="tool-detail-' + entry.id + '"><button data-expand-tool="' + entry.id + '" aria-expanded="' + expanded + '" title="展开或收起参数与结果">' + toolIcon(entry.toolStatus) + '<span class="tool-name">' + esc(entry.displayName) + '</span><span class="tool-summary">' + esc(entry.summary) + '</span><span class="tool-state">' + toolStateText(entry.toolStatus) + '</span></button>' + body + '</div>';
    }).join('');
  }

  function renderRunLog() {
    if (!state.runLog.length) return '<p class="empty-note">尚无调用记录。密钥、完整环境变量不进入日志。</p>';
    return state.runLog.map((item) => '<div class="log' + (item.warning ? ' warning' : '') + '"><time>' + clock(item.time, true) + '</time>' + esc(item.text) + '</div>').join('');
  }

  function renderInspector() {
    for (const button of document.querySelectorAll('#tabs [data-tab]')) {
      const selected = button.dataset.tab === view.tab;
      button.setAttribute('aria-selected', String(selected));
      button.tabIndex = selected ? 0 : -1;
    }
    const panel = $('inspection');
    panel.setAttribute('aria-labelledby', 'tab-' + view.tab);
    if (view.tab === 'tools') panel.innerHTML = renderToolDetails();
    else if (view.tab === 'log') panel.innerHTML = renderRunLog();
    else panel.innerHTML = '<p class="empty-note">暂无待确认的编辑器变更</p>';
  }

  function selectTab(tab, focus) {
    view.tab = tab;
    renderInspector();
    if (focus) $('tab-' + tab).focus();
  }

  /** Opens the inspector on the tool details of one call (a group opens on its first call). */
  function inspectTool(id) {
    view.selectedTool = id;
    expandedTools.add(id);
    view.tab = 'tools';
    openPane('inspector');
    renderInspector();
    const detail = $('tool-detail-' + id);
    if (detail) detail.scrollIntoView({ block: 'nearest' });
  }

  // ---------- Dialogs ----------

  /** Opens a modal dialog; focus goes back to the opener (the focused element unless given) when it closes. */
  function openDialog(dialog, opener) {
    view.dialogOpeners.push(opener || document.activeElement);
    dialog.showModal();
  }

  function onDialogClosed() {
    const opener = view.dialogOpeners.pop();
    if (opener && document.contains(opener) && !opener.disabled) opener.focus();
  }

  function sessionTitle(id) {
    const session = findSession(id);
    return session && session.title ? session.title : '新会话';
  }

  function openRename(id) {
    const session = findSession(id);
    if (!session) return;

    view.targetSessionId = id;
    const input = $('rename-input');
    input.maxLength = state.renameMaxChars;
    input.value = session.title || '';
    $('rename-error').hidden = true;
    openDialog($('rename-dialog'), sessionButton(id));
    input.select();
  }

  function openArchive(id) {
    view.targetSessionId = id;
    openDialog($('archive-dialog'), sessionButton(id));
  }

  /** Deleting cannot be undone, so it always asks; listed and archived sessions alike. */
  function openDelete(id) {
    view.targetSessionId = id;
    $('delete-name').textContent = sessionTitle(id);
    openDialog($('delete-dialog'), sessionButton(id));
  }

  function saveRename() {
    const name = $('rename-input').value.trim();
    if (!name || name.length > state.renameMaxChars) {
      $('rename-error').textContent = '名称不能为空，最多 ' + state.renameMaxChars + ' 个字符。';
      $('rename-error').hidden = false;
      return;
    }
    call('renamesession', view.targetSessionId, name);
    $('rename-dialog').close();
  }

  // ---------- Editor entry points ----------

  const STATE_KEYS = ['status', 'connected', 'prompting', 'stopping', 'busy', 'runtimeLost', 'startIssue', 'resumeFailed', 'needsVerify', 'animateMark', 'usedTokens', 'contextSize', 'config', 'sessions', 'archived', 'currentSessionId', 'currentTitle', 'canArchiveCurrent', 'renameMaxChars', 'permissionMode', 'shellEnabled', 'templates', 'runLog', 'pendingPermission', 'pendingWrite', 'queue', 'canRetry'];

  function apply(message) {
    const reset = message.reset === true;
    if (reset) {
      state.entries = new Map();
      blockElements.forEach((element) => element.remove());
      blockElements.clear();
    }

    const switchedSession = 'currentSessionId' in message && message.currentSessionId !== state.currentSessionId;
    const previousSessionId = state.currentSessionId;
    for (const key of STATE_KEYS) {
      if (key in message) state[key] = message[key];
    }
    if ('animateMark' in message) markPlayer.setEnabled(state.animateMark);
    if ('fontSize' in message) {
      document.documentElement.style.setProperty('--chat-font-size', message.fontSize + 'px');
      growDraft();
    }
    if ('wideLayout' in message) document.body.classList.toggle('wide-content', message.wideLayout === true);
    if ('sidebarWidth' in message) {
      view.paneWidths.sidebar = message.sidebarWidth;
      view.paneWidths.inspector = message.inspectorWidth;
      applyPaneWidths();
    }
    if (switchedSession) {
      expandedTools.clear();
      view.selectedTool = null;
      if (!reset) switchDraft(previousSessionId, state.currentSessionId);
    }

    let toolsChanged = false;
    for (const entry of message.entries || []) {
      state.entries.set(entry.id, entry);
      changedIds.add(entry.id);
      if (entry.kind === 'toolCall') toolsChanged = true;
    }
    if (message.order) {
      state.order = message.order;
      toolsChanged = true;
      for (const id of Array.from(state.entries.keys())) {
        if (!state.order.includes(id)) state.entries.delete(id);
      }
    }

    // A thought label depends on whether its turn still runs, so status changes also revisit the last entry
    if (message.entries || message.order || reset || 'prompting' in message) {
      renderConversation(reset);
      changedIds = new Set();
    }
    if ('status' in message || 'canRetry' in message || 'startIssue' in message || 'resumeFailed' in message) {
      renderHeader();
      renderBanner();
    }
    if ('queue' in message || 'busy' in message || 'runtimeLost' in message) renderQueue();
    if ('sessions' in message || 'status' in message) {
      renderSidebar();
      renderTitle();

      // The menu's session may be gone (deleted, archived) or running now
      closeSessionMenu(false);
    }
    if ('pendingPermission' in message || 'pendingWrite' in message) renderDecision();
    renderComposer();

    const inspectorVisible = paneMode('inspector') !== 'hidden';
    if (inspectorVisible && ((view.tab === 'tools' && toolsChanged) || (view.tab === 'log' && 'runLog' in message))) renderInspector();

    for (const event of message.events || []) {
      if (event === 'turnCompleted') markPlayer.play(headerMark, 'done');
      if (event === 'turnStopped') markPlayer.rest(headerMark);
    }
  }

  function startNewSession() {
    if (state.currentSessionId) view.switchingSession = true;
    call('newconversation');
    if (view.overlay) closeOverlay();
    $('draft').focus();
  }

  /** Panel shortcuts, matched by the editor against their (rebindable) chords and forwarded here. */
  function runCommand(name) {
    if (document.querySelector('dialog[open]')) return;

    switch (name) {
      case 'toggleSidebar':
        togglePane('sidebar');
        break;
      case 'toggleInspector':
        togglePane('inspector');
        break;
      case 'newSession':
        startNewSession();
        break;
      case 'searchSessions':
        openPane('sidebar');
        $('search').focus();
        $('search').select();
        break;
      case 'focusInput':
        if (view.overlay) closeOverlay();
        if (!$('composer').hidden) $('draft').focus();
        break;
    }
  }

  window.Shuttlegent = {
    apply: apply,
    runCommand: runCommand,
    onDragState(over) {
      document.body.classList.toggle('dragover', over);
    },
    onDrop(items) {
      addReferences(items);
    },
    onReferenceResults: onReferenceResults,
    onReferenceValidity: onReferenceValidity,
    onSubmitResult: onSubmitResult,
    onQueueItemTaken: onQueueItemTaken,
    onDrawerState(open) {
      document.body.classList.toggle('drawer-open', open);
    },
    focusDraft() {
      if (!$('composer').hidden) $('draft').focus();
    }
  };

  // ---------- Page events ----------

  function bindComposer() {
    const draft = $('draft');
    let composing = false;
    draft.addEventListener('input', () => {
      growDraft();

      // Typing leaves history browsing; the menu follows the token at the caret once composition ends
      draftState.historyIndex = -1;
      if (!composing) updateMenu();
    });
    draft.addEventListener('compositionstart', () => { composing = true; });
    draft.addEventListener('compositionend', () => {
      composing = false;
      updateMenu();
    });
    draft.addEventListener('click', updateMenu);
    draft.addEventListener('blur', () => window.setTimeout(() => {
      if (document.activeElement !== draft) closeMenu();
    }, 0));
    draft.addEventListener('keydown', (event) => {
      // Enter during IME composition only picks the candidate
      if (event.isComposing || composing || event.keyCode === 229) return;

      // An open menu takes the arrows, Enter, Tab and Esc first; inserting never sends
      if (menu.mode) {
        if (event.key === 'Escape') {
          event.preventDefault();
          event.stopPropagation();
          closeMenu();
          return;
        }
        if (menu.items.length && (event.key === 'ArrowDown' || event.key === 'ArrowUp')) {
          event.preventDefault();
          moveMenu(event.key === 'ArrowDown' ? 1 : -1);
          return;
        }
        if (menu.items.length && (event.key === 'Enter' || event.key === 'Tab')) {
          event.preventDefault();
          chooseMenu(menu.index);
          return;
        }
      }

      if ((event.key === 'ArrowUp' || event.key === 'ArrowDown') && !event.shiftKey && !event.ctrlKey && !event.altKey) {
        if (browseHistory(event.key === 'ArrowUp' ? -1 : 1)) event.preventDefault();
        return;
      }

      if (event.key !== 'Enter' || event.shiftKey) return;
      event.preventDefault();
      submitDraft();
    });
    draft.addEventListener('keyup', (event) => {
      if (event.key === 'ArrowLeft' || event.key === 'ArrowRight' || event.key === 'Home' || event.key === 'End') updateMenu();
    });

    // Mouse down keeps focus in the draft, so the menu does not close before the click lands
    $('suggestions').addEventListener('mousedown', (event) => event.preventDefault());
    $('suggestions').addEventListener('click', (event) => {
      const item = event.target.closest('[data-menu-item]');
      if (item) chooseMenu(Number(item.dataset.menuItem));
    });
    $('references').addEventListener('click', (event) => {
      const remove = event.target.closest('[data-remove-ref]');
      if (remove) removeReference(Number(remove.dataset.removeRef));
    });
    $('add-reference').addEventListener('click', () => call('togglecontentbrowser'));

    $('queue').addEventListener('click', (event) => {
      if (event.target.closest('#resume-queue')) {
        call('resumequeue');
        return;
      }
      const take = event.target.closest('[data-take-queued]');
      if (take) {
        call('takequeued', Number(take.dataset.takeQueued), $('draft').value, draftReferencesJson());
        return;
      }
      const remove = event.target.closest('[data-remove-queued]');
      if (remove) call('removequeued', Number(remove.dataset.removeQueued));
    });

    $('send').addEventListener('click', submitDraft);
    $('stop').addEventListener('click', () => {
      if (!state.stopping) call('cancel');
    });
    $('model').addEventListener('change', (event) => call('setconfig', 'model', event.target.value));
    $('reasoning').addEventListener('change', (event) => call('setconfig', 'reasoning_effort', event.target.value));
  }

  function bindConversation() {
    $('back-bottom').addEventListener('click', scrollToEnd);
    $('conversation').addEventListener('scroll', () => {
      if (hasUnseenContent && isAtEnd()) {
        hasUnseenContent = false;
        $('back-bottom').hidden = true;
      }
    });

    $('messages').addEventListener('click', (event) => {
      const thought = event.target.closest('[data-thought]');
      if (thought) {
        const id = Number(thought.dataset.thought);
        if (expandedThoughts.has(id)) expandedThoughts.delete(id);
        else expandedThoughts.add(id);
        changedIds.add(id);
        renderConversation(false);
        changedIds = new Set();
        return;
      }

      const tool = event.target.closest('[data-tool]');
      if (tool) {
        inspectTool(Number(tool.dataset.tool));
        return;
      }

      const acknowledge = event.target.closest('[data-ack-write]');
      if (acknowledge) {
        acknowledge.disabled = true;
        call('acknowledgewrite', Number(acknowledge.dataset.ackWrite));
        return;
      }

      const undo = event.target.closest('[data-undo-write]');
      if (undo) {
        undo.disabled = true;
        call('undowrite', Number(undo.dataset.undoWrite));
        return;
      }

      const copyCode = event.target.closest('[data-copy-code]');
      if (copyCode) {
        const article = copyCode.closest('[data-entry]');
        const codes = article ? codeSources.get(Number(article.dataset.entry)) : null;
        if (codes) call('copytext', codes[Number(copyCode.dataset.copyCode)] || '');
      }
    });
  }

  function bindPanes() {
    $('toggle-sidebar').addEventListener('click', () => togglePane('sidebar'));
    $('toggle-inspector').addEventListener('click', () => togglePane('inspector'));
    $('scrim').addEventListener('click', closeOverlay);
    $('new-session').addEventListener('click', startNewSession);
    $('search').addEventListener('input', renderSidebar);
    $('open-settings').addEventListener('click', () => call('opensettings'));
    $('archived-button').addEventListener('click', () => openDialog($('archived-dialog')));

    $('sessions').addEventListener('click', (event) => {
      const session = event.target.closest('[data-session]');
      if (!session) return;

      if (session.dataset.session !== state.currentSessionId) {
        view.switchingSession = true;
        call('opensession', session.dataset.session);
      }
      if (view.overlay) closeOverlay();
    });
    $('sessions').addEventListener('contextmenu', (event) => {
      const session = event.target.closest('[data-session]');
      if (!session) return;

      // The context menu key and Shift+F10 come without a pointer position: the menu opens under the item
      event.preventDefault();
      const rect = session.getBoundingClientRect();
      const fromKeyboard = event.clientX === 0 && event.clientY === 0;
      openSessionMenu(session.dataset.session, fromKeyboard ? rect.left + rect.width / 2 : event.clientX, fromKeyboard ? rect.bottom : event.clientY);
    });
    $('sessions').addEventListener('scroll', () => closeSessionMenu(false));

    const sessionMenu = $('session-menu');
    sessionMenu.addEventListener('click', (event) => {
      const item = event.target.closest('[data-menu-action]');
      if (item && !item.disabled) runSessionAction(item.dataset.menuAction);
    });
    sessionMenu.addEventListener('keydown', (event) => {
      if (event.key === 'ArrowDown') moveSessionMenu(1);
      else if (event.key === 'ArrowUp') moveSessionMenu(-1);
      else if (event.key === 'Escape') closeSessionMenu(true);
      else if (event.key === 'Tab') closeSessionMenu(false);
      else return;
      if (event.key !== 'Tab') event.preventDefault();
      event.stopPropagation();
    });
    document.addEventListener('pointerdown', (event) => {
      if (!sessionMenu.hidden && !sessionMenu.contains(event.target)) closeSessionMenu(false);
    }, true);
    window.addEventListener('blur', () => closeSessionMenu(false));

    bindResizers();

    $('tabs').addEventListener('click', (event) => {
      const tab = event.target.closest('[data-tab]');
      if (tab) selectTab(tab.dataset.tab, false);
    });
    $('tabs').addEventListener('keydown', (event) => {
      const tabs = ['changes', 'tools', 'log'];
      let index = tabs.indexOf(view.tab);
      if (event.key === 'ArrowRight') index = (index + 1) % tabs.length;
      else if (event.key === 'ArrowLeft') index = (index + tabs.length - 1) % tabs.length;
      else if (event.key === 'Home') index = 0;
      else if (event.key === 'End') index = tabs.length - 1;
      else return;
      event.preventDefault();
      selectTab(tabs[index], true);
    });
    $('inspection').addEventListener('click', (event) => {
      const toggle = event.target.closest('[data-expand-tool]');
      if (!toggle) return;

      const id = Number(toggle.dataset.expandTool);
      if (expandedTools.has(id)) expandedTools.delete(id);
      else expandedTools.add(id);
      renderInspector();
      $('tool-detail-' + id).querySelector('button').focus();
    });

    window.addEventListener('resize', () => {
      closeSessionMenu(false);
      applyLayout();
    });
  }

  /** Grips on the docked panes' inner edges: drag to resize, arrow keys step, double click restores the default; widths are saved when a change ends. */
  function bindResizers() {
    for (const grip of document.querySelectorAll('.pane-resizer')) {
      const pane = grip.dataset.pane;

      grip.addEventListener('pointerdown', (event) => {
        if (event.button !== 0) return;
        event.preventDefault();
        grip.setPointerCapture(event.pointerId);
        grip.classList.add('dragging');
        document.body.classList.add('resizing');
      });
      grip.addEventListener('pointermove', (event) => {
        if (!grip.hasPointerCapture(event.pointerId)) return;
        const rect = $(pane).getBoundingClientRect();
        resizePane(pane, pane === 'sidebar' ? event.clientX - rect.left : rect.right - event.clientX);
      });
      grip.addEventListener('pointerup', (event) => {
        if (!grip.hasPointerCapture(event.pointerId)) return;
        grip.releasePointerCapture(event.pointerId);
        savePaneWidths();
      });
      grip.addEventListener('lostpointercapture', () => {
        grip.classList.remove('dragging');
        document.body.classList.remove('resizing');
      });
      grip.addEventListener('dblclick', () => {
        view.paneWidths[pane] = 0;
        applyPaneWidths();
        savePaneWidths();
      });
      grip.addEventListener('keydown', (event) => {
        if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
        event.preventDefault();

        // The sidebar grows to the right, the inspector to the left
        const outward = (event.key === 'ArrowRight') === (pane === 'sidebar');
        resizePane(pane, $(pane).offsetWidth + (outward ? RESIZE_KEY_STEP : -RESIZE_KEY_STEP));
        savePaneWidths();
      });
    }
  }

  function bindDialogs() {
    $('rename-save').addEventListener('click', saveRename);
    $('rename-input').addEventListener('keydown', (event) => {
      if (event.key !== 'Enter' || event.isComposing || event.keyCode === 229) return;
      event.preventDefault();
      saveRename();
    });
    $('archive-confirm').addEventListener('click', () => {
      call('archivesession', view.targetSessionId);
      $('archive-dialog').close();
    });
    $('delete-confirm').addEventListener('click', () => {
      call('deletesession', view.targetSessionId);
      $('delete-dialog').close();
    });
    $('archived-list').addEventListener('click', (event) => {
      const restore = event.target.closest('[data-restore]');
      if (restore) call('restoresession', restore.dataset.restore);

      const remove = event.target.closest('[data-delete]');
      if (remove) openDelete(remove.dataset.delete);
    });

    for (const dialog of document.querySelectorAll('dialog')) {
      dialog.addEventListener('close', onDialogClosed);
      dialog.addEventListener('click', (event) => {
        if (event.target.closest('[data-close]')) dialog.close();
      });
    }
  }

  function bindKeyboard() {
    document.addEventListener('keydown', (event) => {
      // Open dialogs handle their own Esc; the composer handles Enter
      if (event.isComposing || event.keyCode === 229 || document.querySelector('dialog[open]')) return;

      // Esc closes the topmost layer first: an overlay pane, then the decision card (rejects this request only)
      if (event.key === 'Escape') {
        if (view.overlay) {
          event.preventDefault();
          closeOverlay();
          return;
        }
        if (currentDecision()) {
          event.preventDefault();
          answerDecision(false);
        }
        return;
      }

      // Tab stays inside an open overlay pane
      if (event.key === 'Tab' && view.overlay) {
        const pane = $(view.overlay);
        const items = Array.from(pane.querySelectorAll('button:not(:disabled),input,select,[tabindex="0"]')).filter((element) => element.offsetParent !== null);
        if (!items.length) return;
        const first = items[0];
        const last = items[items.length - 1];
        if (event.shiftKey && document.activeElement === first) {
          event.preventDefault();
          last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
          event.preventDefault();
          first.focus();
        }
      }
    });

    // The panel has no browser menu and no page navigation of its own
    document.addEventListener('contextmenu', (event) => event.preventDefault());
  }

  function decorateIcons() {
    for (const element of document.querySelectorAll('[data-icon]')) {
      element.innerHTML = icons.svg(element.dataset.icon, Number(element.dataset.size || 16));
    }
  }

  function connect() {
    if (!bridge()) {
      window.setTimeout(connect, BRIDGE_POLL_MS);
      return;
    }
    call('ready');
  }

  decorateIcons();
  headerMark = markPlayer.attach($('header-mark'), 's');

  // Whether CEF reports a hidden panel tab decides if the shell must pause the mark itself (Shuttlegent.md section 14.3.8)
  document.addEventListener('visibilitychange', () => console.log('Shuttlegent page visibility: ' + document.visibilityState));
  bindComposer();
  bindConversation();
  bindPanes();
  bindDialogs();
  bindKeyboard();
  applyLayout();
  renderConversation(true);
  renderHeader();
  renderTitle();
  renderSidebar();
  renderComposer();
  connect();
})();
