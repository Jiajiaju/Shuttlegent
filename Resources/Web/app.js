/*
 * Shuttlegent panel page (W1 shell, W2 structure, W3 input and references, M3.5 queue, M3.6 write confirmation and results).
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
  const warpPlayer = window.ShuttlegentWarp;
  const esc = markdown.escapeHtml;

  // Panel text; the language is fixed for the page's lifetime (i18n.js), so constants below may translate at load
  const i18n = window.ShuttlegentI18n;
  const t = i18n.t;

  // Distance from the bottom, in CSS pixels, within which new content keeps the view pinned to the end
  const FOLLOW_THRESHOLD = 24;

  // One weave of the running-step mark (app.css .weave, sg-weave-*), in milliseconds; every running mark shares its phase
  const WEAVE_PERIOD_MS = 2000;

  // Kinds of call a process line names before it only counts the rest
  const PROCESS_KINDS_SHOWN = 4;

  // How often the elapsed seconds of a running step refresh, in milliseconds
  const STEP_TICK_MS = 1000;

  // How often the page looks for the editor binding before calling ready, in milliseconds
  const BRIDGE_POLL_MS = 50;

  // Panel widths where the layout changes
  const BREAKPOINT_MEDIUM = 560;
  const BREAKPOINT_WIDE = 900;
  const BREAKPOINT_FULL = 1180;

  const DAY_MS = 24 * 60 * 60 * 1000;

  // Docked sidebar width in CSS pixels: default, narrowest and widest a drag can make it
  const SIDEBAR_DEFAULT_WIDTH = 248;
  const SIDEBAR_MIN_WIDTH = 180;
  const SIDEBAR_MAX_WIDTH = 480;

  // The inspector's fixed width in CSS pixels, docked or as an overlay
  const INSPECTOR_WIDTH = 360;

  // Narrowest the conversation gets when the docked sidebar is dragged wider, in CSS pixels (.workbench.full .chat)
  const CHAT_MIN_WIDTH = 320;

  // Width change per arrow key on the focused sidebar grip, in CSS pixels
  const RESIZE_KEY_STEP = 16;

  // Context usage shares where the context card turns to the warning and then the error colour
  const USAGE_HIGH_SHARE = 0.8;
  const USAGE_FULL_SHARE = 0.95;

  // Placeholder of the message input when nothing holds sending back
  const DRAFT_PLACEHOLDER = t('composer.placeholder');

  // Token counts in short form (99.3k, 1M) on the context card
  const TOKENS_PER_THOUSAND = 1000;
  const TOKENS_PER_MILLION = 1000000;

  // Relative message times (Claude style), and how often they refresh, in milliseconds
  const MINUTE_MS = 60 * 1000;
  const HOUR_MS = 60 * MINUTE_MS;
  const WEEK_MS = 7 * DAY_MS;
  const TIME_REFRESH_MS = 30000;

  // How long the copy button shows its check mark, in milliseconds
  const COPIED_MS = 1500;

  // Inspector cards, top to bottom
  const INSPECTOR_CARDS = ['context', 'memory', 'mcp', 'tools', 'log'];

  // Least distance kept between the session menu and the panel edge, in CSS pixels
  const MENU_EDGE_MARGIN = 8;

  // Distance a pressed session row has to move before it counts as a drag out of the list, in CSS pixels
  const SESSION_DRAG_THRESHOLD = 4;

  // Modes in menu order: how editor writes are confirmed, and plan (read-only until a plan is approved)
  const MODES = [
    { value: 'manual', label: t('mode.manual'), detail: t('mode.manualDetail') },
    { value: 'acceptEdits', label: t('mode.acceptEdits'), detail: t('mode.acceptEditsDetail') },
    { value: 'auto', label: t('mode.auto'), detail: t('mode.autoDetail') },
    { value: 'plan', label: t('mode.plan'), detail: t('mode.planDetail') }
  ];
  // Share of the memory file limit at which the memory card summary warns
  const MEMORY_WARN_SHARE = 0.9;

  // Session list dots: what each attention value means, for the tooltip and screen readers
  const ATTENTION_TEXT = { done: t('attention.done'), input: t('attention.input') };

  // Default font stacks; fonts from Editor Preferences go in front of them (tokens.css carries the same stacks)
  const FONT_UI = '"Segoe UI","Microsoft YaHei UI","Microsoft YaHei",sans-serif';
  const FONT_MONO = '"Cascadia Mono",Consolas,"Microsoft YaHei UI",monospace';

  // A font family name the page accepts from settings: letters, digits, spaces and - _ . only; at most this many names
  const FONT_NAME = /^[\p{L}\p{N} _.\-]+$/u;
  const MAX_FONT_NAMES = 4;
  const EFFORT_NOTE = t('effort.note');

  // Names for the runtime's reasoning effort values; the empty value is the provider's own default. Unknown values keep the runtime's name
  const EFFORT_NAMES = { '': t('effort.default'), off: t('effort.off'), minimal: t('effort.minimal'), low: t('effort.low'), medium: t('effort.medium'), high: t('effort.high'), xhigh: t('effort.xhigh'), max: t('effort.max') };

  // Gap between a picker and what opened it, and least distance kept from the panel edge, in CSS pixels
  const PICKER_GAP = 6;
  const PICKER_EDGE = 8;

  // Distance an object link must move before it counts as a drag rather than a click; offset of the dragged label from the pointer; in CSS pixels
  const OBJECT_DRAG_THRESHOLD = 5;
  const OBJECT_DRAG_OFFSET = 12;

  // Pause after typing in the @ menu before the editor is asked for candidates, in milliseconds
  const MENU_QUERY_DELAY_MS = 80;

  // Tokens that open a menu: @query anywhere after whitespace, /query at the start of a line
  const REFERENCE_TOKEN = /(^|\s)@([^\s@]*)$/;
  const TEMPLATE_TOKEN = /(^|\n)\/([^\s/]*)$/;

  // Built-in commands the editor runs itself instead of sending them to the model; they win over a template of the same name
  const COMMAND_LINE = /^\/(compact|plan|undo|clear|memory)(?:\s+([\s\S]*))?$/;
  const COMMANDS = [
    { name: 'compact', body: '/compact', detail: t('command.compact') },
    { name: 'plan', body: '/plan ', detail: t('command.plan') },
    { name: 'undo', body: '/undo', detail: t('command.undo') },
    { name: 'clear', body: '/clear', detail: t('command.clear') },
    { name: 'memory', body: '/memory', detail: t('command.memory') }
  ];

  // Memory notes in the conversation: what each action says before the remembered line
  const MEMORY_ACTION_TEXT = { added: t('memory.added'), updated: t('memory.updated'), removed: t('memory.removed') };

  // Editor tools that change memory; the conversation shows their memory note instead of a tool row
  const MEMORY_TOOL = /(^|__)(remember|forget)$/;

  // Images in one draft; longest side after the page scales a pasted or picked image down; encoded size above which a PNG is re-encoded as JPEG, in bytes, and its quality
  const MAX_DRAFT_IMAGES = 6;
  const MAX_IMAGE_SIDE = 2048;
  const MAX_PNG_BYTES = 4 * 1024 * 1024;
  const JPEG_QUALITY = 0.9;

  // Pause in typing after which the draft goes to the editor to be saved, in milliseconds; a draft image read back from disk must look like this
  const DRAFT_SAVE_MS = 1000;
  const DRAFT_IMAGE_TYPE = /^image\/(png|jpeg|gif|webp)$/;
  const BASE64 = /^[A-Za-z0-9+/]+={0,2}$/;

  // Where the editor's result text names a screenshot it saved (FShuttlegentToolsetImages.cpp writes "saved to <path>]")
  const SAVED_IMAGE_PATTERN = /saved to ([^\]\r\n]+?\.png)\]/g;

  // Asset thumbnails asked for in one requestthumbnails call, the most the editor takes at once (ShuttlegentEditorThumbnail::MaxPerRequest)
  const THUMBNAIL_REQUEST_BATCH = 24;

  // How long a command's refusal stays beside the composer, in milliseconds
  const COMMAND_NOTE_MS = 5000;

  const TOOL_COMPLETED = 'completed';
  const TOOL_FAILED = 'failed';

  // How often the write confirmation card's countdown updates
  const COUNTDOWN_INTERVAL_MS = 1000;

  const STATUS_TEXT = {
    notConnected: t('status.notConnected'),
    connecting: t('status.connecting'),
    connected: t('status.connected'),
    querying: t('status.querying'),
    awaitingPermission: t('status.awaitingPermission'),
    awaitingPlanReview: t('status.awaitingPlanReview'),
    awaitingWrite: t('status.awaitingWrite'),
    writing: t('status.writing'),
    stopping: t('status.stopping'),
    stopped: t('status.stopped'),
    failed: t('status.failed'),
    lost: t('status.lost'),
    resuming: t('status.resuming'),
    startBlocked: t('status.startBlocked'),
    resumeFailed: t('status.resumeFailed')
  };

  // The header shows a status chip only while something is in progress or wrong; a running turn shows through the mark instead
  const STATUS_CHIP = {
    notConnected: { tone: '', text: t('status.notConnected') },
    connecting: { tone: '', text: t('status.connecting'), spin: true },
    resuming: { tone: '', text: t('status.resuming'), spin: true },
    stopping: { tone: '', text: t('status.stopping'), spin: true },
    awaitingWrite: { tone: 'warning', text: t('status.awaitingConfirm') },
    awaitingPermission: { tone: 'warning', text: t('status.awaitingConfirm') },
    awaitingPlanReview: { tone: 'warning', text: t('status.awaitingPlanReview') },
    resumeFailed: { tone: 'warning', text: t('status.resumeFailedShort') },
    failed: { tone: 'danger', text: t('status.failed') },
    lost: { tone: 'danger', text: t('status.lost') },
    startBlocked: { tone: 'danger', text: t('status.startBlocked') }
  };

  // Background sessions in the sidebar: running ones, ones waiting for a confirmation
  const RUNNING_STATUSES = new Set(['connecting', 'querying', 'writing', 'stopping', 'resuming']);
  const CONFIRM_STATUSES = new Set(['awaitingPermission', 'awaitingPlanReview', 'awaitingWrite']);

  // Plan entry review outcomes (feature plan C2), as the chip on the plan in the conversation, with the chip's tone
  const PLAN_STATUS_TEXT = { pending: t('plan.pending'), approved: t('plan.approved'), revised: t('plan.revised'), cancelled: t('plan.cancelled') };
  const PLAN_STATUS_TONE = { pending: 'warning', approved: 'success', revised: 'warning', cancelled: '' };

  // Choice cards (ask_choices): the chip of each state
  const CHOICE_STATUS_TEXT = { pending: t('choice.pending'), answered: t('choice.answered'), unanswered: t('choice.unanswered'), cancelled: t('choice.cancelled') };
  const CHOICE_STATUS_TONE = { pending: 'warning', answered: 'success', unanswered: '', cancelled: '' };

  const state = {
    status: 'notConnected',
    connected: false,
    prompting: false,
    stopping: false,
    busy: false,
    runtimeLost: false,
    startIssue: null,

    // No connected model: { state: 'none' | 'testing' | 'failed', failedUrl, failedBuiltin }; null once one connects
    modelSetup: null,
    resumeFailed: false,
    needsVerify: false,
    animateMark: true,
    usedTokens: 0,
    contextSize: 0,
    contextBreakdown: null,
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
    pendingQuestion: null,
    queue: { paused: false, items: [] },
    canRetry: false,
    canEditHistory: false,
    editHistoryReason: '',
    mcp: null,
    todos: [],
    compacting: false,
    canCompact: false,
    compactReason: '',
    planMode: false,
    writeMode: 'manual',
    canSetPlanMode: false,
    canAttachImages: false,
    attachImagesReason: '',
    planModeReason: '',
    pendingPlanReview: null
  };

  // Page-local view state
  const expandedThoughts = new Set();
  // Runs of tool calls opened in the conversation, by the id of their first call
  const expandedRuns = new Set();
  const expandedTools = new Set();
  const blockElements = new Map();
  const codeSources = new Map();
  // Object links in replies (feature plan C6): last known existence by path, and the paths the editor is watching
  const objectValidity = new Map();
  let watchedObjectKey = '';
  // Images in replies (markdown.js figures): what is known of each, by kind and key — a src, or false when it has none or failed to
  // load — and the asset paths whose thumbnails the editor is still fetching
  const imageStates = new Map();
  const requestedThumbnails = new Set();
  // What the user has picked and written on each waiting choice card, by entry id, so redraws keep it:
  // { selected: [ids], notes: { id: text }, overall, open: [ids whose note field is open], submitting, error }
  const choiceDrafts = new Map();
  // Replies whose "show more" was used: how many asset thumbnails each now shows (markdown.thumbnailPage when absent)
  const thumbnailLimits = new Map();
  const view = {
    layout: 'full',
    // The agent, model or mode picker that is open under the composer ('agent-menu', 'model-menu', 'mode-menu'), or null
    openPicker: null,
    // Docked panes remember whether they stay open; overlays open fresh each time
    docked: { sidebar: true, inspector: true },
    overlay: null,
    // Inspector cards that are expanded; the context card starts open
    cards: { context: true, memory: true, mcp: false, tools: false, log: false },
    // Collapsible groups inside cards that are open (MCP tools, toolsets, external servers)
    cardGroups: new Set(),
    selectedTool: null,
    shownDecisionKey: null,
    // User message the rewind dialog is about
    rewindEntryId: null,
    // Hides the refusal of a slash command beside the composer
    commandNoteTimer: 0,
    answeringDecisionKey: null,
    // Elements focus returns to as dialogs close; a dialog can open over another (delete over the archived list)
    dialogOpeners: [],

    // Docked sidebar width the user dragged, 0 means the default; the inspector's saved value is only passed back, its width is fixed
    paneWidths: { sidebar: 0, inspector: 0 },

    // The conversation has no messages yet: the large mark alone, no inspector
    empty: true,

    // Session the right-click menu, and the rename, archive or delete dialog opened from it, act on
    menuSessionId: '',
    targetSessionId: '',

    // Drafts are kept per session by the editor, on disk: switching sessions neither moves nor clears them
    switchingSession: false,

    // The pending save of the draft, and the last draft handed to the editor per session ('' is the new conversation)
    draftSaveTimer: 0,
    savedDrafts: new Map(),
    sessionView: false,

    // Countdown of the write confirmation card
    countdownTimer: 0,

    // The todo list folded or opened by the user; null follows the list (folded once every item is done)
    todosFolded: null
  };

  // Draft input: reference chips, history browsing, the open menu, and a send waiting for the editor's answer
  const draftState = {
    refs: [],
    historyIndex: -1,
    historyOriginal: '',
    submitting: false,

    // What was handed to the editor; it comes back into the draft only if the editor refuses it
    submittedText: '',
    submittedRefs: [],

    // Images attached to the draft { mimeType, data, url }; the editor sends them only while the conversation is idle
    images: [],
    submittedImages: []
  };
  const menu = {
    mode: null,
    query: '',
    tokenLength: 0,
    sections: [],
    items: [],
    index: 0,
    timer: 0
  };
  let changedIds = new Set();
  let wasWorking = false;
  let emptyMark = null;
  // The message input (composer-editor.js), created by bindComposer; its value is Markdown
  let draftInput = null;

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

  /** Today: 14:05; yesterday: Yesterday 14:05; earlier: 10-21. */
  function sessionTime(time) {
    const today = localDay(Date.now());
    const day = localDay(time);
    if (day === today) return clock(time, false);
    if (day === today - DAY_MS) return t('time.yesterdayAt', { time: clock(time, false) });
    const date = new Date(time);
    return pad(date.getMonth() + 1) + '-' + pad(date.getDate());
  }

  // ---------- Conversation ----------

  function isToolCall(entry) {
    return entry && entry.kind === 'toolCall';
  }

  function isToolFinished(status) {
    return status === TOOL_COMPLETED || status === TOOL_FAILED;
  }

  function isMemoryTool(entry) {
    return !!entry && entry.kind === 'toolCall' && MEMORY_TOOL.test(entry.tool || '');
  }

  function isWork(entry) {
    return isToolCall(entry) || (!!entry && entry.kind === 'assistant');
  }

  /**
   * Splits the entry order into blocks. In a run of replies and tool calls, everything up to the last call (thoughts, the replies'
   * interim text and the calls) folds into one process line; the reply text after the last call shows, and its thought joins the
   * line. A run without calls shows as it is. The process block's signature names the entry after it, or, while it ends the order,
   * whether the turn runs.
   */
  /**
   * The entry order as shown: memory tool calls drop out (they show as memory notes; their rows stay only in the tool details
   * card), and each choice card moves to the end of its turn, before the next user message, so it follows the turn's last reply
   * and does not split its process line.
   */
  function displayOrder() {
    const order = [];
    let held = [];
    for (const id of state.order) {
      const entry = state.entries.get(id);
      if (isMemoryTool(entry)) continue;
      if (entry && entry.kind === 'choice') {
        held.push(id);
        continue;
      }
      if (entry && entry.kind === 'user') {
        order.push(...held);
        held = [];
      }
      order.push(id);
    }
    return order.concat(held);
  }

  function isChoice(id) {
    const entry = state.entries.get(id);
    return !!entry && entry.kind === 'choice';
  }

  function hasPendingChoice() {
    return state.order.some((id) => isChoice(id) && state.entries.get(id).choice && state.entries.get(id).choice.status === 'pending');
  }

  function buildBlocks() {
    const blocks = [];
    const order = displayOrder();
    for (let index = 0; index < order.length; ++index) {
      let end = index;
      while (end < order.length && isWork(state.entries.get(order[end]))) ++end;
      let lastTool = -1;
      for (let at = index; at < end; ++at) {
        if (isToolCall(state.entries.get(order[at]))) lastTool = at;
      }
      if (lastTool < 0) {
        blocks.push({ key: 'e' + order[index], ids: [order[index]], signature: String(order[index]) });
        continue;
      }

      const members = order.slice(index, lastTool + 1);
      const after = lastTool + 1 < end ? state.entries.get(order[lastTool + 1]) : null;
      if (after && after.thought) members.push(after.id);
      const next = lastTool + 1 < order.length ? order[lastTool + 1] : '';
      // tail: only the replies after the last call (and the turn's choice cards) follow this block, so while the turn runs it is still the turn's work
      const tail = order.slice(end).every(isChoice);
      blocks.push({ key: 'p' + members[0], ids: members, run: true, tail: tail, endId: next === '' ? null : next, signature: members.join(',') + '>' + next + (tail ? '|' + state.prompting : '') });
      if (after && after.thought) {
        blocks.push({ key: 'e' + after.id, ids: [after.id], hideThought: true, signature: after.id + '~' });
        index = lastTool + 1;
      } else {
        index = lastTool;
      }
    }
    return blocks;
  }

  /** State mark of a tool call: a check when done, an alert when it failed, a turning loader while it runs. */
  function toolIcon(status, size) {
    if (status === TOOL_COMPLETED) return '<span class="state-icon ok">' + icons.svg('check', size) + '</span>';
    if (status === TOOL_FAILED) return '<span class="state-icon failed">' + icons.svg('alert', size) + '</span>';
    return '<span class="state-icon running">' + icons.svg('loader', size) + '</span>';
  }

  /** One event row of the conversation: icon, label, optional detail and trailer; a button when it reacts to clicks. */
  function eventHtml(options) {
    const tag = options.attributes ? 'button' : 'div';
    const tone = options.tone ? ' is-' + options.tone : '';
    const detail = options.detail ? '<span class="ev-detail">' + options.detail + '</span>' : '';
    const trail = options.trail ? '<span class="ev-trail">' + options.trail + '</span>' : '';
    return '<' + tag + ' class="event' + tone + (options.extraClass ? ' ' + options.extraClass : '') + '"' + (options.attributes || '') + '>' + options.icon
      + '<span class="ev-label' + (options.wrap ? ' is-wrap' : '') + '">' + options.label + '</span>' + (options.caret ? '<span class="ev-caret">' + icons.svg('caret', 14) + '</span>' : '') + detail + trail + (options.after || '') + '</' + tag + '>';
  }

  /** Time of a message as Claude shows it: just now, minutes or hours ago, then the date. */
  function relativeTime(time) {
    const elapsed = Date.now() - time;
    if (elapsed < MINUTE_MS) return t('time.justNow');
    if (elapsed < HOUR_MS) return t('time.minutesAgo', { count: Math.floor(elapsed / MINUTE_MS) });
    if (elapsed < DAY_MS) return t('time.hoursAgo', { count: Math.floor(elapsed / HOUR_MS) });
    if (elapsed < WEEK_MS) return t('time.daysAgo', { count: Math.floor(elapsed / DAY_MS) });
    return i18n.formatMonthDay(time);
  }

  function fullTime(time) {
    const date = new Date(time);
    const pad = (value) => String(value).padStart(2, '0');
    return date.getFullYear() + '-' + pad(date.getMonth() + 1) + '-' + pad(date.getDate()) + ' ' + pad(date.getHours()) + ':' + pad(date.getMinutes());
  }

  /** A user message with the row Claude shows under it on hover: time, copy, rewind to here, branch from here. */
  function renderUser(entry) {
    const references = entry.references && entry.references.length ? '<div class="user-references">' + entry.references.map((line) => '<div>' + esc(line) + '</div>').join('') + '</div>' : '';
    const time = entry.time ? '<time class="user-time" data-time="' + entry.time + '" title="' + fullTime(entry.time) + '">' + relativeTime(entry.time) + '</time><span class="user-dot">·</span>' : '';
    const actions = '<div class="user-actions">' + time
      + '<button class="btn btn-icon btn-sm user-action" data-user-copy="' + entry.id + '" title="' + t('common.copy') + '" aria-label="' + t('common.copy') + '">' + icons.svg('copy', 14) + '</button>'
      + '<button class="btn btn-icon btn-sm user-action history" data-user-rewind="' + entry.id + '" aria-label="' + t('user.rewind') + '">' + icons.svg('rewind', 14) + '</button>'
      + '<button class="btn btn-icon btn-sm user-action history" data-user-branch="' + entry.id + '" aria-label="' + t('user.branch') + '">' + icons.svg('branch', 14) + '</button>'
      + '</div>';
    const images = entry.images && entry.images.length ? '<div class="user-images">' + entry.images.map((url) => '<img src="' + esc(url) + '" alt="" data-view-image="' + esc(url) + '">').join('') + '</div>' : '';

    // The text is Markdown, shown as replies are; single line breaks stay as typed
    const rendered = markdown.render(entry.text || '');
    codeSources.set(entry.id, rendered.codes);
    const text = entry.text ? '<div class="markdown">' + rendered.html + '</div>' : '';
    return '<div class="user-turn"><article class="message user" data-entry="' + entry.id + '">' + text + images + references + '</article>' + actions + '</div>';
  }

  /** Rewind and branch need an idle conversation the runtime has open; otherwise they stay visible but say why they are off. */
  function refreshHistoryActions() {
    const locked = !state.canEditHistory;
    for (const button of document.querySelectorAll('#messages .user-action.history')) {
      button.classList.toggle('locked', locked);
      button.setAttribute('aria-disabled', String(locked));
      const what = button.dataset.userRewind ? t('user.rewindTitle') : t('user.branchTitle');
      button.title = locked ? t('user.locked', { action: what, reason: state.editHistoryReason || t('user.unavailable') }) : what;
    }
  }

  function refreshMessageTimes() {
    for (const element of document.querySelectorAll('#messages .user-time[data-time]')) {
      element.textContent = relativeTime(Number(element.dataset.time));
    }
  }

  /** What rewinding to a user message will do, counted from the entries after it, for the confirmation dialog. */
  function rewindSummary(entryId) {
    const start = state.order.indexOf(entryId);
    let undo = 0;
    let kept = 0;
    let messages = 0;
    for (const id of state.order.slice(start)) {
      const entry = state.entries.get(id);
      if (!entry) continue;
      if (entry.kind === 'user') messages++;
      if (entry.kind === 'writeResult' && !entry.undone) {
        if (entry.undoable) undo++;
        else kept++;
      }
      if (entry.kind === 'writeUnknown') kept++;
    }
    return { undo, kept, messages };
  }

  function openRewind(entryId, opener) {
    const summary = rewindSummary(entryId);
    const lines = [];
    lines.push(summary.undo ? t('rewind.undo', { count: summary.undo }) : t('rewind.nothingToUndo'));
    if (summary.kept) lines.push(t('rewind.kept', { count: summary.kept }));
    lines.push(t('rewind.messages', { count: summary.messages }));
    lines.push(t('rewind.draft'));
    $('rewind-list').innerHTML = lines.map((line) => '<li>' + esc(line) + '</li>').join('');
    view.rewindEntryId = entryId;
    openDialog($('rewind-dialog'), opener);
  }

  function isThinking(entry) {
    return state.prompting && !entry.text && state.order.length > 0 && state.order[state.order.length - 1] === entry.id;
  }

  function renderThought(entry, inProcess) {
    if (!entry.thought) return '';

    const expanded = expandedThoughts.has(entry.id);
    // A reply thinking right now shows only in the running line at the end of the conversation
    if (isThinking(entry) && !inProcess) return '';
    const label = entry.thoughtSeconds >= 0 ? t('thought.seconds', { count: entry.thoughtSeconds }) : t('thought.label');
    const row = eventHtml({ icon: icons.svg('thought', 16), label: label, caret: true, attributes: ' data-thought="' + entry.id + '" aria-expanded="' + expanded + '" title="' + t('thought.toggle') + '"' });
    return row + (expanded ? '<div class="event-body">' + esc(entry.thought) + '</div>' : '');
  }

  /** A reply: its thought as an event row, then the text; replies carry no name or mark of their own. */
  function renderAssistant(entry, hideThought) {
    const rendered = markdown.render(entry.text, thumbnailLimits.get(entry.id));
    codeSources.set(entry.id, rendered.codes);
    const text = entry.text ? '<div class="markdown">' + rendered.html + moreThumbnailsHtml(entry.id, rendered.hiddenThumbnails) + '</div>' : '';
    return '<article class="message assistant" data-entry="' + entry.id + '">' + (hideThought ? '' : renderThought(entry)) + text + '</article>';
  }

  /** Screenshots a tool returned, from the saved paths the editor writes into its result text. */
  function toolImages(entry) {
    const urls = [];
    for (const match of String(entry.result || '').matchAll(SAVED_IMAGE_PATTERN)) urls.push('file:///' + match[1].replace(/\\/g, '/'));
    return urls.length ? '<div class="tool-images">' + urls.map((url) => '<img src="' + esc(url) + '" alt="" data-view-image="' + esc(url) + '">').join('') + '</div>' : '';
  }

  /** A tool call as an event row: state icon, name and summary; a failure adds its reason underneath. Clicking opens its details. */
  /** The running-step mark: the logo with its shuttle weaving in from the left, beating down and flying out to the right, in phase with every other. */
  function weaveIcon() {
    return '<span class="weave" style="--weave-delay:-' + (Date.now() % WEAVE_PERIOD_MS) + 'ms">' + markPlayer.markup() + '</span>';
  }

  /** Seconds, or minutes and seconds, since a time, kept current by refreshElapsed while it shows. */
  function elapsedText(since) {
    const seconds = Math.max(0, Math.round((Date.now() - since) / 1000));
    return seconds < 60 ? t('time.seconds', { count: seconds }) : t('time.minutesSeconds', { minutes: Math.floor(seconds / 60), seconds: seconds % 60 });
  }

  /** Time since a moment, kept current by refreshElapsed while it shows. */
  function elapsedHtml(since) {
    if (!since) return '';
    return '<span class="ev-elapsed" data-since="' + since + '">' + elapsedText(since) + '</span>';
  }

  function refreshElapsed() {
    for (const element of document.querySelectorAll('#working [data-since]')) element.textContent = elapsedText(Number(element.dataset.since));
  }

  /** One call inside an opened run: state, name and summary; a click shows its arguments and result in the inspector. */
  function renderToolStep(entry) {
    const failed = entry.toolStatus === TOOL_FAILED;
    const reason = failed && entry.failureReason ? '<div class="event-body is-danger">' + esc(entry.failureReason) + '</div>' : '';
    return eventHtml({ icon: toolIcon(entry.toolStatus, 14), label: esc(entry.displayName), detail: esc(entry.summary || ''), tone: failed ? 'danger' : '', extraClass: 'run-step', attributes: ' data-tool="' + entry.id + '" title="' + t('tool.showInInspector') + '"' }) + reason;
  }

  /**
   * Consecutive tool calls as one line, as Claude shows them: while running the weave, the call under way and the seconds so far;
   * done, the one call or how many, the seconds the run took, and any failures. A click opens the calls; screenshots always show.
   */
  /**
   * What a run of calls did, as Claude sums it up: each kind of call once in order of first use, with its count, and the failures;
   * past four kinds the rest is counted.
   */
  function toolSummary(tools) {
    const kinds = new Map();
    for (const entry of tools) kinds.set(entry.displayName, (kinds.get(entry.displayName) || 0) + 1);
    const parts = Array.from(kinds, ([name, count]) => (count > 1 ? t('tool.kindCount', { name: esc(name), count: count }) : esc(name)));
    const separator = t('tool.kindSeparator');
    return parts.length > PROCESS_KINDS_SHOWN ? t('tool.kindsMore', { kinds: parts.slice(0, PROCESS_KINDS_SHOWN).join(separator), count: parts.length }) : parts.join(separator);
  }

  /** Whether the turn is writing its reply text now: it runs and its last entry is a reply with text. */
  function replyUnderWay() {
    const entry = state.entries.get(state.order[state.order.length - 1]);
    return state.prompting && !!entry && entry.kind === 'assistant' && !!entry.text;
  }

  /** The time since this turn's message, for the running line. */
  function turnStart() {
    for (let index = state.order.length - 1; index >= 0; --index) {
      const entry = state.entries.get(state.order[index]);
      if (entry && entry.kind === 'user') return entry.time || 0;
    }
    return 0;
  }

  /** One reply inside an opened process line: its thought, then its interim text, muted (the reply shown below keeps its text). */
  function renderWorkReply(entry, textShownBelow) {
    const rendered = textShownBelow || !entry.text ? null : markdown.render(entry.text);
    if (rendered) codeSources.set(entry.id, rendered.codes);
    return renderThought(entry, true) + (rendered ? '<div class="markdown run-note" data-entry="' + entry.id + '">' + rendered.html + '</div>' : '');
  }

  /**
   * A turn's work as one line, opened with the triangle. Running (it is then the last line of the conversation, below queued
   * messages): the weave, the time since the turn's message and what goes on now. Done: what the calls did and any failures.
   */
  function processHtml(ids, endId, live) {
    const entries = ids.map((id) => state.entries.get(id));
    const tools = entries.filter(isToolCall);
    const thoughts = entries.filter((entry) => entry.kind === 'assistant');
    const runningTool = tools.filter((entry) => !isToolFinished(entry.toolStatus)).pop();
    const failed = tools.filter((entry) => entry.toolStatus === TOOL_FAILED).length;
    const first = entries[0];
    const last = entries[entries.length - 1];
    const expanded = expandedRuns.has(first.id);

    let icon = icons.svg('tools', 16);
    let label = '';
    if (live) {
      icon = weaveIcon();
      const start = turnStart();
      let current = t('work.working');
      if (runningTool) current = t('work.running', { tool: '<span class="ev-current">' + esc(runningTool.displayName) + '</span>' });
      else if (thoughts.some(isThinking)) current = t('work.thinking');
      else if (replyUnderWay()) current = t('work.replying');
      label = (start ? elapsedHtml(start) + '<span class="ev-sep"> · </span>' : '') + current;
    } else {
      if (failed) icon = toolIcon(TOOL_FAILED, 16);
      label = toolSummary(tools);
    }
    if (failed) label += '<span class="ev-failed">' + t('work.failures', { count: failed }) + '</span>';

    const row = eventHtml({ icon: icon, label: label, caret: true, attributes: ' data-run="' + first.id + '" aria-expanded="' + expanded + '" title="' + t('work.toggle') + '"' });
    const textBelow = last.kind === 'assistant' && !!last.text;
    const steps = expanded ? '<div class="run-steps">' + entries.map((entry) => entry.kind === 'assistant' ? renderWorkReply(entry, entry === last && textBelow) : renderToolStep(entry)).join('') + '</div>' : '';
    return '<div class="tool process">' + row + steps + tools.map(toolImages).join('') + '</div>';
  }

  /** Whether a process block is the turn's work still going on: a call runs, or the turn runs and only its replies follow it. */
  function isLiveProcess(block) {
    if (!block || !block.run) return false;
    const running = block.ids.some((id) => {
      const entry = state.entries.get(id);
      return isToolCall(entry) && !isToolFinished(entry.toolStatus);
    });
    return running || (state.prompting && block.tail);
  }

  /** In the conversation a running process block stays away: the running line at the end shows it. */
  function renderProcess(block) {
    if (isLiveProcess(block)) return '<div class="tool process" hidden></div>';
    return processHtml(block.ids, block.endId, false);
  }

  function renderPermission(entry) {
    let label = t('permission.waiting', { tool: esc(entry.displayName) });
    if (entry.answered) label = entry.allowed ? t('permission.allowed', { tool: esc(entry.displayName) }) : t('permission.rejected', { tool: esc(entry.displayName) });
    return eventHtml({ icon: icons.svg('shield', 16), label: label, tone: entry.answered ? '' : 'warning', wrap: true });
  }

  /** One object of a write: state, name, the change and why it failed; the change reads "Field current → planned". */
  function objectRow(object, status) {
    const states = {
      completed: ['', 'check', t('object.completed')],
      failed: ['failed', 'alert', t('object.failed')],
      notRun: ['skipped', 'partial', t('object.notRun')],
      rolledBack: ['skipped', 'undo', t('object.rolledBack')],
      unknown: ['unknown', 'alert', t('object.unknown')]
    };
    const [cls, icon, label] = states[status] || states.completed;
    const change = object.field ? esc(object.field) + ' ' + esc(object.current) + ' → ' + esc(object.planned) : '';
    const message = object.message ? (change ? ' · ' : '') + esc(object.message) : '';
    return '<li><span class="st' + (cls ? ' ' + cls : '') + '">' + icons.svg(icon, 12) + label + '</span><span class="nm" title="' + esc(object.path) + '">' + esc(object.name) + '</span><span class="ch">' + change + message + '</span></li>';
  }

  /** Result card of an editor write: real per-object outcome, undo only while its transaction is on top. */
  function renderWriteResult(entry) {
    const objects = entry.objects || [];
    const completed = objects.filter((object) => object.status === 'completed').length;
    let icon = icons.svg('circle-check', 16).replace('class="sg-icon"', 'class="sg-icon tone-success"');
    let title = t('write.completed', { count: completed });
    const rolledBack = objects.some((object) => object.status === 'rolledBack');
    if (entry.undone) {
      icon = icons.svg('undo', 16);
      title = t('write.undone');
    } else if (rolledBack) {
      icon = icons.svg('alert', 16).replace('class="sg-icon"', 'class="sg-icon tone-danger"');
      title = t('write.rolledBack', { count: objects.length });
    } else if (completed < objects.length) {
      icon = completed > 0 ? icons.svg('partial', 16).replace('class="sg-icon"', 'class="sg-icon tone-warning"') : icons.svg('alert', 16).replace('class="sg-icon"', 'class="sg-icon tone-danger"');
      title = completed > 0 ? t('write.partial', { done: completed, count: objects.length }) : t('write.notDone', { count: objects.length });
    }

    const auto = entry.autoApproved && !entry.undone ? '<span class="chip" title="' + t('write.autoTitle') + '">' + t('write.auto') + '</span>' : '';
    const list = entry.undone ? '' : '<ul class="obj-list">' + objects.map((object) => objectRow(object, object.status)).join('') + '</ul>';
    let foot = '';
    if (entry.undoable && !entry.undone) {
      const undoLabel = entry.canUndo ? t('write.undo') : t('write.undoUnavailable');
      foot = '<div class="card-foot"><span class="hint">' + esc(entry.undoReason) + '</span><button class="btn btn-sm" data-undo-write="' + entry.id + '" ' + (entry.canUndo ? '' : 'disabled') + ' title="' + esc(entry.undoReason) + '">' + undoLabel + '</button></div>';
    } else if (!entry.undoable) {
      foot = '<div class="card-foot"><span class="hint">' + t('write.notUndoable') + '</span></div>';
    }
    return '<article class="card result"><div class="card-head">' + icon + '<span class="card-title' + (entry.undone ? ' is-quiet' : '') + '">' + title + '</span>' + auto + '</div><div class="card-desc">' + esc(entry.action) + '</div>' + list + foot + '</article>';
  }

  /** A confirmed write whose result never arrived: what was confirmed, verified by the user before sending again. */
  function renderWriteUnknown(entry) {
    const objects = entry.objects || [];
    const rows = objects.map((object) => objectRow(object, 'unknown')).join('');
    const button = '<button class="btn btn-sm' + (entry.answered ? '' : ' btn-primary') + '" data-ack-write="' + entry.id + '" ' + (entry.answered ? 'disabled' : '') + '>' + (entry.answered ? t('write.checked') : t('write.checkContinue')) + '</button>';
    const hint = entry.answered ? t('write.checked') : t('write.unknownHint');
    return '<article class="card result"><div class="card-head">' + icons.svg('alert', 16).replace('class="sg-icon"', 'class="sg-icon tone-warning"') + '<span class="card-title">' + t('write.unknown', { count: objects.length }) + '</span></div>'
      + '<div class="card-desc">' + esc(entry.action) + '</div><ul class="obj-list">' + rows + '</ul><div class="card-desc">' + hint + '</div><div class="card-foot">' + button + '</div></article>';
  }

  /** A plan the agent presented (feature plan C2), kept in the conversation with its review outcome and feedback. */
  function renderPlan(entry) {
    const rendered = markdown.render(entry.text || '');
    codeSources.set(entry.id, rendered.codes);
    const status = entry.planStatus || 'pending';
    const tone = PLAN_STATUS_TONE[status] ? ' chip-' + PLAN_STATUS_TONE[status] : '';
    const feedback = status === 'revised' && entry.feedback ? '<div class="card-desc is-warning">' + t('plan.feedback', { text: esc(entry.feedback) }) + '</div>' : '';
    return '<article class="card plan-entry ' + esc(status) + '" data-entry="' + entry.id + '"' + (status === 'cancelled' ? ' style="opacity:.7"' : '') + '><div class="card-head">' + icons.svg('plan', 16).replace('class="sg-icon"', 'class="sg-icon tone-info"') + '<span class="card-title">' + t('plan.title') + '</span><span class="chip' + tone + '">' + esc(PLAN_STATUS_TEXT[status] || status) + '</span></div>'
      + '<div class="markdown" style="font-size:.92em">' + rendered.html + '</div>' + feedback + '</article>';
  }

  function choiceDraft(entryId) {
    if (!choiceDrafts.has(entryId)) choiceDrafts.set(entryId, { selected: [], notes: {}, overall: '', open: [], submitting: false, error: '' });
    return choiceDrafts.get(entryId);
  }

  /** One option of a choice card: its picture (same frame as a reply image), pick mark, title, line of description and its note. */
  function choiceOptionHtml(choice, option, draft, live) {
    const picked = draft.selected.includes(option.id);
    const note = draft.notes[option.id] || '';
    const frame = option.image ? markdown.imageFrameFor(option.image) : '';
    const mark = '<span class="choice-mark' + (choice.multiSelect ? ' is-check' : ' is-radio') + '" aria-hidden="true">' + (picked ? icons.svg('check', 12) : '') + '</span>';

    let noteHtml = '';
    if (choice.itemNotes && live) {
      noteHtml = draft.open.includes(option.id) || note
        ? '<textarea class="field choice-note" data-choice-note="' + esc(option.id) + '" rows="2" placeholder="' + t('choice.notePlaceholder') + '" aria-label="' + t('choice.noteLabel', { option: esc(option.title) }) + '">' + esc(note) + '</textarea>'
        : '<button class="btn btn-ghost btn-sm choice-note-open" data-choice-note-open="' + esc(option.id) + '">' + icons.svg('edit', 12) + t('choice.writeNote') + '</button>';
    } else if (note) {
      noteHtml = '<div class="choice-note-text">' + esc(note) + '</div>';
    }

    const role = live ? ' role="' + (choice.multiSelect ? 'checkbox' : 'radio') + '" aria-checked="' + picked + '" tabindex="0"' : '';
    return '<div class="choice-option' + (picked ? ' picked' : '') + (live ? '' : ' readonly') + '" data-choice-option="' + esc(option.id) + '"' + role + '>'
      + (frame ? '<div class="choice-image">' + frame + '</div>' : '')
      + '<div class="choice-body">' + mark + '<div class="choice-text"><div class="choice-title">' + esc(option.title) + '</div>' + (option.description ? '<div class="choice-desc">' + esc(option.description) + '</div>' : '') + '</div></div>'
      + noteHtml + '</div>';
  }

  /**
   * A choice card (ask_choices) in the conversation: options to pick (one or several), notes per option and overall when the card
   * allows them, and Submit, which sends the choice as the next message. Once it is no longer waiting it shows what was sent.
   */
  function renderChoice(entry) {
    const choice = entry.choice || { options: [] };
    const status = choice.status || 'pending';
    const live = status === 'pending';
    const draft = live ? choiceDraft(entry.id) : { selected: choice.selected || [], notes: choice.notes || {}, overall: choice.overall || '', open: [], submitting: false, error: '' };
    const tone = CHOICE_STATUS_TONE[status] ? ' chip-' + CHOICE_STATUS_TONE[status] : '';
    const head = '<div class="card-head">' + icons.svg('question', 16).replace('class="sg-icon"', 'class="sg-icon tone-info"') + '<span class="card-title">' + esc(choice.question || '') + '</span><span class="chip' + tone + '">' + esc(CHOICE_STATUS_TEXT[status] || status) + '</span></div>';
    const options = '<div class="choice-grid">' + (choice.options || []).map((option) => choiceOptionHtml(choice, option, draft, live)).join('') + '</div>';

    let overall = '';
    if (choice.overallNote && live) overall = '<textarea class="field choice-overall" data-choice-overall="' + entry.id + '" rows="2" placeholder="' + t('choice.overallPlaceholder') + '" aria-label="' + t('choice.overallLabel') + '">' + esc(draft.overall) + '</textarea>';
    else if (draft.overall) overall = '<div class="choice-note-text">' + t('choice.overallText', { text: esc(draft.overall) }) + '</div>';

    const error = draft.error ? '<div class="card-desc is-danger">' + esc(draft.error) + '</div>' : '';
    const hint = (choice.multiSelect ? t('choice.multiSelect') : t('choice.singleSelect')) + t('choice.zoomHint') + (choice.itemNotes || choice.overallNote ? t('choice.submitHint') : '');
    const foot = live ? '<div class="card-foot"><span class="hint">' + hint + '</span><button class="btn btn-primary" data-choice-submit="' + entry.id + '"' + (draft.submitting ? ' disabled' : '') + '>' + (draft.submitting ? t('choice.submitting') : t('choice.submit')) + '</button></div>' : '';
    return '<article class="card choice-card ' + esc(status) + '" data-entry="' + entry.id + '" data-choice="' + entry.id + '">' + head + options + overall + error + foot + '</article>';
  }

  /** Redraws one choice card after a pick or a note field opens; its thumbnails come from the cache or are asked for. */
  function redrawChoice(entryId) {
    changedIds.add(entryId);
    renderConversation(false);
    changedIds = new Set();
    requestThumbnails();
  }

  function toggleChoiceOption(card, optionId) {
    const entryId = Number(card.dataset.choice);
    const entry = state.entries.get(entryId);
    if (!entry || !entry.choice || entry.choice.status !== 'pending') return;

    const draft = choiceDraft(entryId);
    const index = draft.selected.indexOf(optionId);
    if (index >= 0) draft.selected.splice(index, 1);
    else if (entry.choice.multiSelect) draft.selected.push(optionId);
    else draft.selected = [optionId];
    draft.error = '';
    redrawChoice(entryId);
  }

  function submitChoice(entryId) {
    const draft = choiceDraft(entryId);
    const notes = {};
    for (const id in draft.notes) {
      if (draft.notes[id].trim()) notes[id] = draft.notes[id].trim();
    }
    if (!draft.selected.length && !Object.keys(notes).length && !draft.overall.trim()) {
      draft.error = t('choice.empty');
      redrawChoice(entryId);
      return;
    }

    draft.submitting = true;
    draft.error = '';
    redrawChoice(entryId);
    call('submitchoice', entryId, JSON.stringify({ selected: draft.selected, notes: notes, overall: draft.overall.trim() }));
  }

  /** The editor refused a choice card's submission: { id, reason }. */
  function onChoiceRejected(result) {
    const draft = choiceDraft(result.id);
    draft.submitting = false;
    draft.error = result.reason || t('choice.cannotSubmit');
    redrawChoice(result.id);
  }

  /** A divider row in the conversation (feature plan C7): compacting now, how much it took away, or why it failed. */
  function renderCompaction(entry) {
    const manual = !!entry.manual;
    let text;
    if (entry.compactionStatus === 'running') text = icons.svg('loader', 14) + (manual ? t('compaction.runningManual') : t('compaction.runningAuto'));
    else if (entry.compactionStatus === 'failed') text = icons.svg('alert', 14) + (manual ? t('compaction.failedManual', { error: esc(entry.text) }) : t('compaction.failedAuto', { error: esc(entry.text) }));
    else text = icons.svg('compress', 14) + (manual ? t('compaction.doneManual') : t('compaction.doneAuto')) + (entry.shadowedTokens > 0 ? t('compaction.shadowed', { tokens: i18n.formatNumber(entry.shadowedTokens) }) : '');
    const tone = entry.compactionStatus === 'failed' ? ' is-danger' : entry.compactionStatus === 'running' ? ' running' : '';
    return '<div class="event-divider' + tone + '" title="' + t('compaction.title') + '"><span>' + text + '</span></div>';
  }

  /** One grey line: how many memories this conversation started with, or a memory the agent saved, changed or dropped (undoable). */
  function renderMemoryNote(entry) {
    if (entry.memoryAction === 'read') {
      return eventHtml({ icon: icons.svg('memory', 16), label: t('memory.read', { count: Number(entry.text) || 0 }), attributes: ' data-show-memory title="' + t('memory.showAll') + '"' });
    }
    const after = entry.undone ? '<span class="ev-trail">' + t('memory.undone') + '</span>' : '<button class="btn btn-link" data-undo-memory="' + entry.id + '">' + t('memory.undo') + '</button>';
    return eventHtml({ icon: icons.svg('memory', 16), label: (MEMORY_ACTION_TEXT[entry.memoryAction] || '') + esc(entry.text), wrap: true, extraClass: entry.undone ? 'is-undone' : '', after: after });
  }

  function renderNotice(entry) {
    return eventHtml({ icon: icons.svg(entry.error ? 'alert' : 'info', 16), label: esc(entry.text), tone: entry.error ? 'danger' : '', wrap: true });
  }

  function renderBlock(block) {
    if (block.run) return renderProcess(block);

    const entry = state.entries.get(block.ids[0]);
    switch (entry.kind) {
      case 'user':
        return renderUser(entry);
      case 'assistant':
        return renderAssistant(entry, block.hideThought);
      case 'permission':
        return renderPermission(entry);
      case 'writeResult':
        return renderWriteResult(entry);
      case 'writeUnknown':
        return renderWriteUnknown(entry);
      case 'memory':
        return renderMemoryNote(entry);
      case 'compaction':
        return renderCompaction(entry);
      case 'plan':
        return renderPlan(entry);
      case 'choice':
        return renderChoice(entry);
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
    updateBackBottom();
  }

  /** The round button above the composer: shown whenever the view is away from the end; three dots while the turn still writes below. */
  function updateBackBottom() {
    const button = $('back-bottom');
    button.hidden = state.order.length === 0 || isAtEnd();
    button.classList.toggle('is-live', !!state.prompting);
  }

  // An empty conversation keeps the composer at the bottom and centres the large mark in the space above it; the inspector stays away until the first message
  function renderEmptyState(empty) {
    $('chat').classList.toggle('is-empty', empty);
    if (view.empty !== empty) {
      view.empty = empty;
      applyLayout();
    }
    if (!empty) {
      if (emptyMark) markPlayer.detach(emptyMark);
      emptyMark = null;
      $('perch').replaceChildren();
      return;
    }
    if (emptyMark) return;

    $('perch').innerHTML = '<div id="empty-mark" role="img" aria-label="Shuttlegent"></div>';
    emptyMark = markPlayer.attach($('empty-mark'), 'l', $('chat'));
    $('empty-mark').addEventListener('shuttleflight', (event) => warpPlayer.track(event.detail.node, event.detail.duration));
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
      const signature = block.signature;
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
    syncObjectLinks();

    // Streamed output never steals focus and never forces the view back to the end; with nothing left below (an empty or
    // shorter conversation after a switch) there is nothing to go back to
    if (atEnd || state.order.length === 0 || isAtEnd()) {
      scrollToEnd();
    } else {
      updateBackBottom();
    }
  }

  // ---------- Header, banner, composer, decision card ----------

  function updateMarkOpacity() {
    document.documentElement.classList.toggle('disconnected', !state.connected);
  }

  function renderHeader() {
    const status = $('status');
    const chip = STATUS_CHIP[state.status];
    status.hidden = !chip;
    if (chip) {
      status.className = 'chip chip-md' + (chip.tone ? ' chip-' + chip.tone : '');
      const lead = chip.spin ? '<span class="spin">' + icons.svg('loader', 12) + '</span>' : '<span class="dot"></span>';
      status.innerHTML = lead + '<span class="label">' + chip.text + '</span>';
      status.title = chip.text;
    }
    updateMarkOpacity();

    // While a turn runs and nothing waits for the user, the conversation's end weaves; a confirmation, a stop or a lost runtime
    // clears it (app.css hides it while a step in the conversation weaves)
    const working = state.prompting && !state.pendingWrite && !state.pendingPermission && !state.pendingQuestion && !state.pendingPlanReview;
    $('working').hidden = !working;
    wasWorking = working;
    renderWorking();
  }

  /**
   * The line at the end of the conversation while a turn works (as Claude): the running process line when there is one, opened with
   * the triangle; otherwise the weave, the time since the turn's message and "thinking" or "working".
   */
  function renderWorking() {
    const line = $('working');
    if (line.hidden) {
      line.replaceChildren();
      return;
    }

    const live = buildBlocks().filter(isLiveProcess).pop() || null;
    if (live) {
      line.innerHTML = processHtml(live.ids, live.endId, true);
      return;
    }

    const start = turnStart();
    const lastEntry = state.entries.get(state.order[state.order.length - 1]);
    let current = t('work.working');
    if (lastEntry && lastEntry.kind === 'assistant' && isThinking(lastEntry)) current = t('work.thinking');
    else if (replyUnderWay()) current = t('work.replying');
    line.innerHTML = '<div class="event">' + weaveIcon() + '<span class="ev-label">' + (start ? elapsedHtml(start) + '<span class="ev-sep"> · </span>' : '') + current + '</span></div>';
  }

  function renderTitle() {
    $('session-title').textContent = state.currentTitle || t('session.untitled');
    $('session-title').title = state.currentTitle || t('session.untitled');
  }

  // ---------- Todo list (feature plan C1) ----------

  /**
   * The agent's latest todo list above the composer, as in Claude: a header with progress that folds the list away,
   * then one row per item. A list whose items are all done folds itself once the turn ends; an empty list hides.
   */
  function renderTodos() {
    const panel = $('todos');
    const todos = state.todos || [];
    panel.hidden = todos.length === 0;
    if (!todos.length) {
      view.todosFolded = null;
      return;
    }

    // Folds itself once every item is done and the turn ended, and while a decision card waits; a fold or unfold by the user wins
    const done = todos.filter((todo) => todo.status === 'completed').length;
    const folded = view.todosFolded !== null ? view.todosFolded : (done === todos.length && !state.prompting) || currentDecision() !== null;
    const current = todos.find((todo) => todo.status === 'in_progress');
    const summary = current ? esc(current.content) : done === todos.length ? t('todos.allDone') : '';
    const header = '<button class="stack-head" data-todos-toggle aria-expanded="' + !folded + '">' + icons.svg('caret', 14)
      + '<span class="t num">' + t('todos.progress', { done: done, total: todos.length }) + '</span><span class="s">' + summary + '</span></button>';
    const rows = folded ? '' : '<ol class="stack-list">' + todos.map((todo) => {
      const icon = todo.status === 'completed' ? icons.svg('check', 13) : todo.status === 'in_progress' ? icons.svg('loader', 13) : '<span class="todo-ring"></span>';
      return '<li class="todo ' + esc(todo.status) + '"><span class="todo-mark">' + icon + '</span><span class="todo-text">' + esc(todo.content) + '</span></li>';
    }).join('') + '</ol>';
    panel.innerHTML = header + rows;
  }

  /** Messages sent while a turn runs show at the end of the conversation as the user's bubbles, waiting their turn. */
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
    const note = paused
      ? '<div class="queue-note is-paused">' + t('queue.paused') + '<button class="btn btn-sm" id="resume-queue" ' + (canContinue ? '' : 'disabled') + ' title="' + (canContinue ? t('queue.resumeTitle') : t('queue.resumeBlocked')) + '">' + t('queue.resume') + '</button></div>'
      : '';
    queue.innerHTML = items.map((item) => {
      const refs = item.references && item.references.length ? '<div class="user-references">' + t('queue.references', { count: item.references.length }) + '</div>' : '';
      const rendered = markdown.render(item.text || '');
      return '<div class="user-turn is-queued"><article class="message user"><div class="markdown">' + rendered.html + '</div>' + refs + '</article>'
        + '<div class="user-actions"><span class="queued-mark">' + (paused ? t('queue.itemPaused') : t('queue.itemWaiting')) + '</span><span class="user-dot">·</span>'
        + '<button class="btn btn-sm btn-ghost" data-take-queued="' + item.id + '" title="' + t('queue.takeTitle') + '">' + t('queue.take') + '</button>'
        + '<button class="btn btn-icon btn-sm" data-remove-queued="' + item.id + '" title="' + t('common.remove') + '" aria-label="' + t('queue.remove') + '">' + icons.svg('close', 14) + '</button></div></div>';
    }).join('') + note;
    if (isAtEnd()) scrollToEnd();
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
    draftInput.focus();
  }

  /**
   * Each session keeps its own draft. Leaving a session always swaps drafts (the user switched, or the session moved into a tab);
   * a new conversation that just got its session id, without the user switching, keeps the current draft.
   */
  function switchDraft(previousId, saved) {
    const switching = view.switchingSession || previousId !== '';
    view.switchingSession = false;
    if (!switching) return;

    saveDraftNow(previousId);
    loadDraft(saved);
    closeMenu();
  }

  /** The draft as the editor stores it: text, chips and images; '' when there is nothing to keep. */
  function draftJson() {
    const text = draftInput.value;
    if (!text.trim() && !draftState.refs.length && !draftState.images.length) return '';
    return JSON.stringify({
      text: text,
      refs: draftState.refs.map((ref) => ({ kind: ref.kind, path: ref.path, name: ref.name, cls: ref.cls })),
      images: draftState.images.map((image) => ({ mimeType: image.mimeType, data: image.data }))
    });
  }

  /** Hands the draft to the editor once typing pauses; a draft that has not changed since the last save is not sent again. */
  function scheduleDraftSave() {
    if (!draftInput) return;
    window.clearTimeout(view.draftSaveTimer);
    view.draftSaveTimer = window.setTimeout(() => saveDraftNow(state.currentSessionId), DRAFT_SAVE_MS);
  }

  function saveDraftNow(sessionId) {
    window.clearTimeout(view.draftSaveTimer);
    view.draftSaveTimer = 0;
    const json = draftJson();
    if ((view.savedDrafts.get(sessionId) || '') === json) return;
    view.savedDrafts.set(sessionId, json);
    call('savedraft', sessionId, json);
  }

  /** Puts a draft the editor stored back into the input; images that do not look like a draft image are dropped. */
  function loadDraft(saved) {
    const draft = saved && typeof saved === 'object' ? saved : {};
    const refs = Array.isArray(draft.refs) ? draft.refs : [];
    const images = Array.isArray(draft.images) ? draft.images : [];
    draftState.refs = refs.filter((ref) => ref && typeof ref.path === 'string' && ref.path)
      .map((ref) => ({ kind: ref.kind === 'actor' ? 'actor' : 'asset', path: ref.path, name: String(ref.name || ''), cls: String(ref.cls || ''), valid: true }));
    draftState.images = images.filter((image) => image && DRAFT_IMAGE_TYPE.test(image.mimeType) && typeof image.data === 'string' && BASE64.test(image.data))
      .slice(0, MAX_DRAFT_IMAGES)
      .map((image) => ({ mimeType: image.mimeType, data: image.data, url: 'data:' + image.mimeType + ';base64,' + image.data }));
    draftState.historyIndex = -1;
    setDraft(typeof draft.text === 'string' ? draft.text : '');
    renderReferences();
    renderAttachments();
    watchReferences();
    view.savedDrafts.set(state.currentSessionId, draftJson());
  }

  /** Start guide card: what blocks the runtime and where to fix it; never a key value. */
  function startGuide(issue) {
    const code = (text) => '<code>' + esc(text) + '</code>';
    switch (issue.kind) {
      case 'missingKey':
        return { tone: '', icon: 'key', title: t('start.missingKeyTitle'), body: t('start.missingKeyBody'), settings: true };
      case 'authFailed':
        return { tone: 'danger', icon: 'key', title: t('start.authFailedTitle', { provider: esc(issue.provider) }), body: t('start.authFailedBody'), settings: true };
      case 'missingRuntime':
        return { tone: 'danger', icon: 'alert', title: t('start.missingRuntimeTitle'), body: t('start.missingRuntimeBody', { path: code(issue.runtimePath), releases: '<a href="' + esc(issue.releasesUrl) + '">' + t('start.releasesPage') + '</a>' }), folder: true };
      default:
        return { tone: 'danger', icon: 'alert', title: t('start.failedTitle'), body: issue.detail ? code(issue.detail) : t('start.failedBody'), folder: false };
    }
  }

  /** A session-wide notice at the top of the conversation column: icon, title, one line of help, and its buttons. */
  function calloutHtml(tone, icon, title, body, actions) {
    return '<div class="callout' + (tone ? ' is-' + tone : '') + '" role="alert">' + icons.svg(icon, 16) + '<div class="callout-body"><div class="callout-title">' + title + '</div>'
      + (body ? '<div class="callout-text">' + body + '</div>' : '') + '</div><div class="callout-actions">' + actions + '</div></div>';
  }

  function renderBanner() {
    const banner = $('banner');

    // With no connected model the connect card already says what to do
    const covered = state.modelSetup && state.startIssue && state.startIssue.kind === 'missingKey';
    if (state.startIssue && !covered) {
      const guide = startGuide(state.startIssue);
      const actions = (guide.settings ? '<button class="btn btn-sm" id="open-guide-settings">' + t('banner.openSettings') + '</button>' : '') + (guide.folder ? '<button class="btn btn-sm" id="open-plugin-folder">' + t('banner.openPluginFolder') + '</button>' : '') + '<button class="btn btn-sm btn-primary" id="retry-start">' + t('common.retry') + '</button>';
      banner.innerHTML = calloutHtml(guide.tone, guide.icon, guide.title, guide.body, actions);
      banner.hidden = false;
      if (guide.settings) $('open-guide-settings').addEventListener('click', () => call('opensettings'));
      if (guide.folder) $('open-plugin-folder').addEventListener('click', () => call('openpluginfolder'));
      $('retry-start').addEventListener('click', () => call('retrystart'));
      return;
    }
    if (state.runtimeLost) {
      banner.innerHTML = calloutHtml('', 'unplug', t('banner.lostTitle'), t('banner.kept'), '<button class="btn btn-sm" id="reconnect">' + t('banner.reconnect') + '</button>');
      banner.hidden = false;
      $('reconnect').addEventListener('click', () => call('reconnect'));
      return;
    }
    if (state.resumeFailed && state.status !== 'resuming') {
      banner.innerHTML = calloutHtml('', 'alert', t('banner.resumeFailedTitle'), t('banner.resumeFailedBody'), '<button class="btn btn-sm" id="resume-new">' + t('banner.newSession') + '</button><button class="btn btn-sm" id="retry-resume">' + t('banner.retryResume') + '</button>');
      banner.hidden = false;
      $('retry-resume').addEventListener('click', () => call('retryresume'));

      // Not a page-initiated switch, so the current draft stays in the composer of the new session
      $('resume-new').addEventListener('click', () => call('newconversation'));
      return;
    }
    if (state.status === 'failed') {
      banner.innerHTML = calloutHtml('danger', 'alert', t('banner.failedTitle'), t('banner.kept'), state.canRetry ? '<button class="btn btn-sm" id="retry">' + t('common.retry') + '</button>' : '');
      banner.hidden = false;
      if (state.canRetry) $('retry').addEventListener('click', () => call('retry'));
      return;
    }
    banner.hidden = true;
    banner.innerHTML = '';
  }

  /**
   * The connect-a-model card, shown while no connection is connected: DeepSeek needs only a key, another service its address too.
   * Built once and kept, so status updates never wipe what is being typed; the key goes to the editor and is cleared, never sent back.
   */
  function renderSetup() {
    const slot = $('setup-slot');
    const setup = state.modelSetup;
    $('chat').classList.toggle('has-setup', !!setup);
    if (!setup) {
      slot.hidden = true;
      return;
    }

    if (!slot.firstChild) {
      slot.innerHTML = '<section class="card setup-card" aria-labelledby="setup-title">'
        + '<div class="setup-head">' + icons.svg('key', 16) + '<h2 class="setup-title" id="setup-title">' + t('setup.title') + '</h2></div>'
        + '<p class="setup-lead">' + t('setup.lead') + '</p>'
        + '<div class="setup-kinds" role="radiogroup" aria-label="' + t('setup.kinds') + '">'
        + '<button role="radio" aria-checked="true" data-setup-kind="builtin">DeepSeek</button>'
        + '<button role="radio" aria-checked="false" data-setup-kind="custom">' + t('setup.custom') + '</button></div>'
        + '<label class="setup-field" id="setup-url-row" hidden><span>' + t('setup.url') + '</span><input class="field" id="setup-url" placeholder="https://…/v1" spellcheck="false" autocomplete="off"></label>'
        + '<label class="setup-field"><span>' + t('setup.key') + '</span><input class="field" id="setup-key" type="password" spellcheck="false" autocomplete="off"></label>'
        + '<div class="setup-foot"><span class="setup-status" id="setup-status" role="status"></span><button class="btn btn-primary btn-sm" id="setup-connect">' + t('setup.connect') + '</button></div>'
        + '<button class="btn btn-link setup-settings" id="setup-settings">' + t('setup.manage') + '</button>'
        + '</section>';

      for (const kind of slot.querySelectorAll('[data-setup-kind]')) {
        kind.addEventListener('click', () => {
          for (const other of slot.querySelectorAll('[data-setup-kind]')) other.setAttribute('aria-checked', String(other === kind));
          $('setup-url-row').hidden = kind.dataset.setupKind === 'builtin';
          view.setupNote = '';
          renderSetup();
          ($('setup-url-row').hidden ? $('setup-key') : $('setup-url')).focus();
        });
      }
      const submit = () => {
        const builtin = $('setup-url-row').hidden;
        const url = $('setup-url').value.trim();
        const key = $('setup-key').value.trim();
        if (!key || (!builtin && !url)) {
          view.setupNote = builtin ? t('setup.needKey') : t('setup.needUrlKey');
          renderSetup();
          return;
        }
        call('connectmodel', builtin, builtin ? '' : url, key);
        $('setup-key').value = '';
        view.setupNote = '';
      };
      $('setup-connect').addEventListener('click', submit);
      for (const id of ['setup-url', 'setup-key']) {
        $(id).addEventListener('keydown', (event) => {
          if (event.key !== 'Enter') return;
          event.preventDefault();
          submit();
        });
      }
      $('setup-settings').addEventListener('click', () => call('opensettings'));
    }

    // A note about the form wins until the next attempt; then the editor's state: testing, the failure, or nothing yet
    const status = $('setup-status');
    const testing = setup.state === 'testing';
    let text = view.setupNote || '';
    let danger = !!view.setupNote;
    if (!text && testing) text = t('setup.testing');
    if (!text && setup.state === 'failed') {
      text = t('setup.failed', { target: setup.failedBuiltin ? 'DeepSeek' : setup.failedUrl });
      danger = true;
    }
    status.textContent = text;
    status.classList.toggle('is-danger', danger);
    $('setup-connect').disabled = testing;
    slot.hidden = false;
  }

  function configChoices(configId) {
    const option = state.config[configId] || { current: '', choices: [] };
    return { current: option.current, choices: option.choices || [], locked: !!option.locked };
  }

  /** A model choice reads "Provider / Model"; the trigger and the menu show the model alone. */
  function shortModelName(name) {
    const index = name.lastIndexOf(' / ');
    return index >= 0 ? name.slice(index + 3) : name;
  }

  function choiceName(configId) {
    const { current, choices } = configChoices(configId);
    const choice = choices.find((item) => item.value === current);
    return choice ? choice.name : '';
  }

  /** A reasoning effort choice reads its Chinese name; the trigger and the effort row show the current one. */
  function effortName(choice) {
    return Object.prototype.hasOwnProperty.call(EFFORT_NAMES, choice.value) ? EFFORT_NAMES[choice.value] : choice.name;
  }

  function currentEffortName() {
    const { current, choices } = configChoices('reasoning_effort');
    const choice = choices.find((item) => item.value === current);
    return choice ? effortName(choice) : '';
  }

  function hasInvalidReference() {
    return draftState.refs.some((ref) => ref.valid === false);
  }

  function updateSendButton() {
    const send = $('send');
    const invalid = hasInvalidReference();
    const imagesBlocked = draftState.images.length > 0 && !state.canAttachImages;
    const empty = !draftInput.value.trim() && !draftState.images.length;

    // As in Claude the send button shows only while there is something to send; it greys out when sending has to wait
    send.hidden = empty;
    send.disabled = empty || invalid || imagesBlocked || draftState.submitting || state.runtimeLost || !!state.startIssue || state.needsVerify;
    if (invalid) send.title = t('composer.invalidReference');
    else if (imagesBlocked) send.title = state.attachImagesReason || t('composer.imagesBlocked');
    else if (state.startIssue) send.title = t('composer.startBlocked');
    else if (state.needsVerify) send.title = t('composer.needsVerify');
    else if (state.runtimeLost) send.title = t('composer.lost');
    else send.title = state.busy ? t('composer.queueTitle') : t('composer.sendTitle');
  }

  function renderComposer() {
    const send = $('send');
    const stop = $('stop');

    // While a turn runs the same button queues the message; stop stays next to it
    send.setAttribute('aria-label', state.busy ? t('composer.queue') : t('composer.send'));
    updateSendButton();

    // The input's placeholder says why sending waits; there is no hint line under the composer
    let placeholder = DRAFT_PLACEHOLDER;
    if (hasPendingChoice()) placeholder = t('composer.placeholderChoice');
    if (state.busy) placeholder = t('composer.placeholderBusy');
    if (state.runtimeLost) placeholder = t('composer.placeholderLost');
    if (state.needsVerify) placeholder = t('composer.placeholderVerify');
    if (state.startIssue) placeholder = t('composer.placeholderStart');
    draftInput.placeholder = placeholder;
    stop.hidden = !state.prompting;
    stop.disabled = state.stopping;
    stop.classList.toggle('is-stopping', state.stopping);
    stop.innerHTML = state.stopping ? icons.svg('loader', 16) : icons.svg('stop', 24);
    stop.title = state.stopping ? t('composer.stoppingTitle') : t('composer.stopTitle');
    stop.setAttribute('aria-label', state.stopping ? t('composer.stopping') : t('composer.stop'));
    renderMetaTriggers();
    refreshOpenPicker();
    renderImageButton();
  }

  function currentMode() {
    return state.planMode ? 'plan' : state.writeMode;
  }

  /** Below the composer, right-aligned: agent, model with reasoning effort, then mode; the composer outline shows any mode other than manual. */
  function renderMetaTriggers() {
    // The agent is fixed once the first message is sent: the session history belongs to the agent that wrote it
    const agents = configChoices('agent');
    $('agent-name').textContent = choiceName('agent') || 'Agent';
    const agentTrigger = $('agent-trigger');
    agentTrigger.hidden = agents.choices.length === 0;
    agentTrigger.disabled = agents.locked || state.prompting;
    agentTrigger.title = agents.locked ? t('meta.agentLocked') : t('meta.agentTitle');

    const models = configChoices('model');
    $('model-name').textContent = shortModelName(choiceName('model')) || t('menu.model');
    $('effort-name').textContent = currentEffortName();
    const modelTrigger = $('model-trigger');
    modelTrigger.disabled = models.choices.length === 0 || state.prompting;
    modelTrigger.title = state.prompting ? t('meta.modelLocked') : t('meta.modelTitle');

    const mode = currentMode();
    const info = MODES.find((item) => item.value === mode);
    const modeTrigger = $('mode-trigger');
    modeTrigger.innerHTML = '<span>' + info.label + '</span>';
    modeTrigger.title = t('meta.modeTitle', { detail: info.detail });
    for (const item of MODES) {
      $('composer').classList.toggle('mode-' + item.value, item.value === mode);
      modeTrigger.classList.toggle('mode-' + item.value, item.value === mode);
    }
  }

  function pickerItemHtml(attributes, label, detail, checked, disabled) {
    return '<button class="menu-item" role="menuitemradio" aria-checked="' + checked + '"' + attributes + (disabled ? ' disabled' : '') + '><span class="menu-text">' + esc(label)
      + (detail ? '<small>' + esc(detail) + '</small>' : '') + '</span>'
      + (checked ? '<span class="menu-check">' + icons.svg('check', 16) + '</span>' : '') + '</button>';
  }

  function agentMenuHtml() {
    const agents = configChoices('agent');
    return '<div class="menu-label">Agent</div>' + agents.choices.map((choice) => pickerItemHtml(' data-pick-agent="' + esc(choice.value) + '"', choice.name, '', choice.value === agents.current, false)).join('');
  }

  function modelMenuHtml() {
    const models = configChoices('model');
    const efforts = configChoices('reasoning_effort');
    let html = '<div class="menu-label">' + t('menu.model') + '</div>' + models.choices.map((choice) => pickerItemHtml(' data-pick-model="' + esc(choice.value) + '"', shortModelName(choice.name), '', choice.value === models.current, false)).join('');
    if (efforts.choices.length) {
      html += '<div class="menu-sep"></div><button class="menu-item" id="effort-row" role="menuitem" aria-haspopup="menu" aria-expanded="false"><span class="menu-text">' + t('menu.effort') + '</span><span class="menu-trail">' + esc(currentEffortName()) + icons.svg('chevron', 14) + '</span></button>';
    }
    return html;
  }

  function effortMenuHtml() {
    const efforts = configChoices('reasoning_effort');
    return '<div class="menu-note">' + EFFORT_NOTE + '</div>' + efforts.choices.map((choice) => pickerItemHtml(' data-pick-effort="' + esc(choice.value) + '"', effortName(choice), '',choice.value === efforts.current, false)).join('');
  }

  function modeMenuHtml() {
    const mode = currentMode();
    return '<div class="menu-label">' + t('menu.mode') + '</div>' + MODES.map((item) => {
      const disabled = item.value === 'plan' && !state.canSetPlanMode && !state.planMode;
      return pickerItemHtml(' data-pick-mode="' + item.value + '"' + (disabled && state.planModeReason ? ' title="' + esc(state.planModeReason) + '"' : ''), item.label, item.detail, item.value === mode, disabled);
    }).join('');
  }

  /** Places a picker above its anchor, right edges aligned, kept inside the panel. */
  function placePicker(picker, anchor) {
    const rect = anchor.getBoundingClientRect();
    const left = Math.min(Math.max(rect.right - picker.offsetWidth, PICKER_EDGE), window.innerWidth - picker.offsetWidth - PICKER_EDGE);
    const top = Math.max(rect.top - picker.offsetHeight - PICKER_GAP, PICKER_EDGE);
    picker.style.left = left + 'px';
    picker.style.top = top + 'px';
  }

  /** The effort submenu opens beside the model menu, its bottom level with the effort row; it flips left when there is no room. */
  function placeSubmenu(picker, row) {
    const menu = $('model-menu').getBoundingClientRect();
    const rect = row.getBoundingClientRect();
    let left = menu.right + PICKER_GAP;
    if (left + picker.offsetWidth > window.innerWidth - PICKER_EDGE) left = Math.max(menu.left - picker.offsetWidth - PICKER_GAP, PICKER_EDGE);
    const top = Math.min(Math.max(rect.bottom - picker.offsetHeight, PICKER_EDGE), window.innerHeight - picker.offsetHeight - PICKER_EDGE);
    picker.style.left = left + 'px';
    picker.style.top = top + 'px';
  }

  function pickerTrigger(id) {
    return $(id === 'agent-menu' ? 'agent-trigger' : id === 'model-menu' ? 'model-trigger' : 'mode-trigger');
  }

  function fillPicker(id) {
    $(id).innerHTML = id === 'agent-menu' ? agentMenuHtml() : id === 'model-menu' ? modelMenuHtml() : id === 'effort-menu' ? effortMenuHtml() : modeMenuHtml();
  }

  function openPicker(id, focusFirst) {
    closePickers();
    const picker = $(id);
    fillPicker(id);
    picker.hidden = false;
    placePicker(picker, pickerTrigger(id));
    pickerTrigger(id).setAttribute('aria-expanded', 'true');
    view.openPicker = id;
    if (focusFirst) focusPickerItem(picker, 0);
  }

  function openEffortMenu(focusFirst) {
    const row = $('effort-row');
    if (!row) return;
    const picker = $('effort-menu');
    fillPicker('effort-menu');
    picker.hidden = false;
    placeSubmenu(picker, row);
    row.classList.add('open');
    row.setAttribute('aria-expanded', 'true');
    if (focusFirst) focusPickerItem(picker, 0);
  }

  function closeEffortMenu() {
    $('effort-menu').hidden = true;
    const row = $('effort-row');
    if (!row) return;
    row.classList.remove('open');
    row.setAttribute('aria-expanded', 'false');
  }

  function closePickers() {
    for (const id of ['agent-menu', 'model-menu', 'effort-menu', 'mode-menu']) {
      $(id).hidden = true;
    }
    $('agent-trigger').setAttribute('aria-expanded', 'false');
    $('model-trigger').setAttribute('aria-expanded', 'false');
    $('mode-trigger').setAttribute('aria-expanded', 'false');
    view.openPicker = null;
  }

  /** A state push while a picker is open redraws it in place, so the check follows what the editor applied. */
  function refreshOpenPicker() {
    if (!view.openPicker) return;
    if ((view.openPicker === 'model-menu' || view.openPicker === 'agent-menu') && pickerTrigger(view.openPicker).disabled) {
      closePickers();
      return;
    }

    const effortOpen = !$('effort-menu').hidden;
    fillPicker(view.openPicker);
    placePicker($(view.openPicker), pickerTrigger(view.openPicker));
    if (effortOpen) openEffortMenu(false);
  }

  function focusPickerItem(picker, index) {
    const items = Array.from(picker.querySelectorAll('.menu-item:not(:disabled)'));
    if (items.length) items[(index + items.length) % items.length].focus();
  }

  /** Memory card buttons, and a fresh copy of the file whenever the panel regains focus (it may have been edited outside). */
  function bindMemory() {
    $('card-body-memory').addEventListener('click', (event) => {
      const forget = event.target.closest('[data-forget-memory]');
      if (forget) {
        const item = state.memory && state.memory.items[Number(forget.dataset.forgetMemory)];
        forget.disabled = true;
        if (item) call('forgetmemory', item.text);
        return;
      }
      if (event.target.closest('#memory-open')) call('openmemoryfile');
    });
    window.addEventListener('focus', () => call('refreshmemory'));
  }

  function bindPickers() {
    for (const id of ['agent-menu', 'model-menu', 'mode-menu']) {
      const trigger = pickerTrigger(id);
      trigger.addEventListener('click', () => {
        if (view.openPicker === id) closePickers();
        else openPicker(id, false);
      });
      trigger.addEventListener('keydown', (event) => {
        if (event.key !== 'ArrowUp' && event.key !== 'ArrowDown') return;
        event.preventDefault();
        openPicker(id, true);
      });
    }

    $('agent-menu').addEventListener('click', (event) => {
      const agent = event.target.closest('[data-pick-agent]');
      if (!agent) return;
      if (agent.getAttribute('aria-checked') !== 'true') call('setconfig', 'agent', agent.dataset.pickAgent);
      closePickers();
    });
    $('model-menu').addEventListener('click', (event) => {
      const model = event.target.closest('[data-pick-model]');
      if (model) {
        if (model.getAttribute('aria-checked') !== 'true') call('setconfig', 'model', model.dataset.pickModel);
        closePickers();
        return;
      }
      if (!event.target.closest('#effort-row')) return;
      if ($('effort-menu').hidden) openEffortMenu(false);
      else closeEffortMenu();
    });
    // Hovering the effort row opens its submenu; hovering a model row closes it
    $('model-menu').addEventListener('mouseover', (event) => {
      const item = event.target.closest('.menu-item');
      if (!item) return;
      if (item.id === 'effort-row') openEffortMenu(false);
      else closeEffortMenu();
    });
    $('effort-menu').addEventListener('click', (event) => {
      const effort = event.target.closest('[data-pick-effort]');
      if (!effort) return;
      if (effort.getAttribute('aria-checked') !== 'true') call('setconfig', 'reasoning_effort', effort.dataset.pickEffort);
      closePickers();
    });
    $('mode-menu').addEventListener('click', (event) => {
      const mode = event.target.closest('[data-pick-mode]');
      if (!mode || mode.disabled) return;
      closePickers();
      setMode(mode.dataset.pickMode);
    });

    // Outside clicks close; the triggers handle their own clicks
    document.addEventListener('mousedown', (event) => {
      if (!view.openPicker || event.target.closest('.picker, .meta-trigger')) return;
      closePickers();
    });
    window.addEventListener('resize', closePickers);
    window.addEventListener('blur', closePickers);

    // Arrows move between items, Right opens the effort submenu and Left leaves it, Esc closes and returns focus to the trigger
    document.addEventListener('keydown', (event) => {
      if (!view.openPicker) return;
      const picker = event.target.closest ? event.target.closest('.picker') : null;
      if (event.key === 'Escape') {
        event.preventDefault();
        event.stopPropagation();
        if (picker && picker.id === 'effort-menu') {
          closeEffortMenu();
          $('effort-row').focus();
          return;
        }
        const trigger = pickerTrigger(view.openPicker);
        closePickers();
        trigger.focus();
        return;
      }
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        const target = picker || $(view.openPicker);
        const items = Array.from(target.querySelectorAll('.menu-item:not(:disabled)'));
        const index = items.indexOf(document.activeElement);
        const step = event.key === 'ArrowDown' ? 1 : -1;
        focusPickerItem(target, index < 0 ? (step > 0 ? 0 : items.length - 1) : index + step);
        return;
      }
      if (event.key === 'ArrowRight' && document.activeElement && document.activeElement.id === 'effort-row') {
        event.preventDefault();
        openEffortMenu(true);
        return;
      }
      if (event.key === 'ArrowLeft' && picker && picker.id === 'effort-menu') {
        event.preventDefault();
        closeEffortMenu();
        $('effort-row').focus();
      }
    }, true);
  }

  /** Plan goes through the runtime (it also leaves plan mode); the other modes only change how editor writes are confirmed. */
  function setMode(mode) {
    if (mode === currentMode()) return;
    if (mode === 'plan') {
      if (state.canSetPlanMode) call('setplanmode', true);
      else showCommandNote(state.planModeReason || t('note.cannotEnterPlan'));
      return;
    }
    call('setwritemode', mode);
  }

  function prettyRaw(text) {
    try {
      return JSON.stringify(JSON.parse(text), null, 2);
    } catch (error) {
      return text || '';
    }
  }

  /** Editor write card: action, objects with current and planned values, level, undo, raw arguments. */
  /** Head row of a decision card: icon, the title that takes focus, and an optional chip. */
  function decisionHead(icon, tone, title, chip) {
    const svg = icons.svg(icon, 16).replace('class="sg-icon"', 'class="sg-icon' + (tone ? ' tone-' + tone : '') + '"');
    return '<div class="card-head">' + svg + '<h3 class="card-title" id="decision-title" tabindex="-1">' + title + '</h3>' + (chip || '') + '</div>';
  }

  /** Editor write card: action, objects with current and planned values, level, undo, raw arguments. */
  function writeCardHtml(request) {
    const plannedClass = request.destructive ? 'planned is-danger' : 'planned';
    const rows = request.objects.map((object) => '<tr><td title="' + esc(object.path) + '">' + esc(object.name) + '</td><td title="' + esc(object.field) + '">' + esc(object.field) + '</td><td title="' + esc(object.current) + '">' + esc(object.current) + '</td><td class="' + plannedClass + '" title="' + esc(object.planned) + '">' + esc(object.planned) + '</td></tr>').join('');
    const chip = request.undoable ? '<span class="chip chip-success">' + t('decision.undoable') + '</span>' : '<span class="chip chip-danger">' + t('decision.notUndoable') + '</span>';
    const level = request.level ? esc(request.level) + ' · ' : '';
    // Deletes, moves and renames are called out so they are not approved in passing
    const destructive = request.destructive ? '<div class="note-strip">' + icons.svg('alert', 14) + '<span>' + t('decision.destructive') + '</span></div>' : '';
    return decisionHead(request.destructive ? 'trash' : 'edit', request.destructive ? 'danger' : '', esc(request.action), chip)
      + destructive
      + '<div class="card-desc">' + level + t('decision.objects', { count: request.objects.length }) + ' · <span class="mono">' + esc(request.tool) + '</span></div>'
      + '<div class="mini-table"><table><thead><tr><th>' + t('decision.object') + '</th><th>' + t('decision.field') + '</th><th>' + t('decision.current') + '</th><th>' + t('decision.planned') + '</th></tr></thead><tbody>' + rows + '</tbody></table></div>'
      + '<button class="disclosure" id="decision-raw-toggle" aria-expanded="false">' + icons.svg('caret', 14) + t('decision.raw') + '</button><pre class="pre" id="decision-raw" hidden>' + esc(prettyRaw(request.raw)) + '</pre>'
      + '<div class="card-foot"><span class="hint num" title="' + t('decision.onceTitle') + '">' + t('decision.once') + '<span id="decision-countdown"></span></span><button class="btn" id="decision-reject">' + t('common.cancel') + '</button><button class="btn ' + (request.destructive ? 'btn-danger' : 'btn-primary') + '" id="decision-allow">' + t('decision.confirm') + '</button></div>';
  }

  function permissionCardHtml(request) {
    const lines = [];
    if (request.target) lines.push('<span class="mono">' + esc(request.target) + '</span>');
    if (request.content) lines.push(t('decision.content', { text: esc(request.content) }));
    if (request.detail) lines.push(t('decision.detail', { text: esc(request.detail) }));
    return decisionHead('shield', 'warning', t('decision.permissionTitle', { tool: esc(request.displayName) }), '<span class="chip chip-danger">' + t('decision.notUndoable') + '</span>')
      + (lines.length ? '<div class="card-desc">' + lines.join('<br>') + '</div>' : '')
      + '<div class="card-foot"><span class="hint" title="' + t('decision.rejectTitle') + '">' + t('decision.allowOnceHint') + '</span><button class="btn" id="decision-reject">' + t('decision.reject') + '</button><button class="btn btn-primary" id="decision-allow">' + t('decision.allowOnce') + '</button></div>';
  }

  /** Question the agent asked (ask_user): the options as numbered buttons, an own answer when allowed, skip. */
  function questionCardHtml(request) {
    const options = request.options.map((option, index) => '<button class="option" data-question-option="' + index + '"><kbd class="kbd">' + (index + 1) + '</kbd>' + esc(option) + '</button>').join('');
    const other = request.allowOther
      ? '<div class="option-other"><input class="field" id="question-other" placeholder="' + t('decision.otherPlaceholder') + '" aria-label="' + t('decision.otherLabel') + '" autocomplete="off"><button class="btn" id="question-other-send">' + t('decision.answer') + '</button></div>'
      : '';
    return decisionHead('question', 'info', esc(request.question), '')
      + '<div class="option-list">' + options + other + '</div>'
      + '<div class="card-foot"><button class="btn btn-ghost lead" id="decision-reject">' + t('decision.skip') + '</button><span class="hint num">' + t('decision.questionHint') + '<span id="decision-countdown"></span></span></div>';
  }

  /** The request the card shows: an editor write first, then a runtime permission, then a question; null when nothing waits. */
  function currentDecision() {
    if (state.pendingWrite) return { key: 'w' + state.pendingWrite.id, kind: 'write', request: state.pendingWrite };
    if (state.pendingPermission) return { key: 'p' + state.pendingPermission.id, kind: 'permission', request: state.pendingPermission };
    if (state.pendingQuestion) return { key: 'q' + state.pendingQuestion.id, kind: 'question', request: state.pendingQuestion };
    if (state.pendingPlanReview) return { key: 'r' + state.pendingPlanReview.reviewId, kind: 'plan', request: state.pendingPlanReview };
    return null;
  }

  /** Plan the agent presented in plan mode: the plan itself, optional feedback, send back or approve. It waits without a time limit. */
  function planCardHtml(request) {
    const rendered = markdown.render(request.plan || '');
    return decisionHead('plan', 'info', t('decision.planTitle'), '<span class="chip chip-info">' + t('decision.planMode') + '</span>')
      + '<div class="plan-box markdown">' + rendered.html + '</div>'
      + '<textarea id="plan-feedback" class="field" rows="2" placeholder="' + t('decision.feedbackPlaceholder') + '" aria-label="' + t('decision.feedbackLabel') + '"></textarea>'
      + '<div class="card-desc">' + t('decision.planHint') + '</div>'
      + '<div class="card-foot"><button class="btn btn-ghost lead" id="decision-reject">' + t('decision.keepPlanning') + '</button><button class="btn" id="decision-allow-manual">' + t('decision.approveManual') + '</button><button class="btn btn-primary" id="decision-allow">' + t('decision.approve') + '</button></div>';
  }

  /** The decision card replaces the composer while an editor write or a runtime permission request waits for an answer. */
  function renderDecision() {
    const decision = currentDecision();
    const card = $('decision');
    const waiting = decision !== null;
    $('composer').hidden = waiting;
    $('composer-meta').hidden = waiting;
    if (waiting) closePickers();
    card.hidden = !waiting;
    renderTodos();
    if (!waiting || decision.kind === 'permission' || decision.kind === 'plan') stopCountdown();
    if (!waiting) {
      // Focus that sat on the card goes back to the draft once it is answered
      const hadFocus = card.contains(document.activeElement);
      card.innerHTML = '';
      view.shownDecisionKey = null;
      view.answeringDecisionKey = null;
      if (hadFocus) draftInput.focus();
      return;
    }
    if (view.shownDecisionKey === decision.key) return;

    // Focus that was on the previous card or in the composer moves to the new title; focus elsewhere (the viewport) is not stolen
    const focusWasOurs = $('composer').contains(document.activeElement) || card.contains(document.activeElement) || document.activeElement === document.body;
    card.classList.toggle('card-danger', decision.kind === 'write' && !!decision.request.destructive);
    if (decision.kind === 'question') {
      card.innerHTML = questionCardHtml(decision.request);
      for (const button of card.querySelectorAll('[data-question-option]')) {
        button.addEventListener('click', () => answerQuestion(decision.request.options[Number(button.dataset.questionOption)]));
      }
      if (decision.request.allowOther) {
        const sendOther = () => answerQuestion($('question-other').value.trim());
        $('question-other-send').addEventListener('click', sendOther);
        $('question-other').addEventListener('keydown', (event) => {
          if (event.key !== 'Enter') return;
          event.preventDefault();
          sendOther();
        });
      }
      $('decision-reject').addEventListener('click', () => answerDecision(false));
    } else {
      if (decision.kind === 'plan') card.innerHTML = planCardHtml(decision.request);
      else card.innerHTML = decision.kind === 'write' ? writeCardHtml(decision.request) : permissionCardHtml(decision.request);
      $('decision-reject').addEventListener('click', () => answerDecision(false));
      $('decision-allow').addEventListener('click', () => answerDecision(true));
      if (decision.kind === 'plan') $('decision-allow-manual').addEventListener('click', () => answerDecision(true, false));
      if (decision.kind === 'write') {
        $('decision-raw-toggle').addEventListener('click', () => {
          const raw = $('decision-raw');
          raw.hidden = !raw.hidden;
          $('decision-raw-toggle').setAttribute('aria-expanded', String(!raw.hidden));
        });
      }
    }
    view.shownDecisionKey = decision.key;
    if (decision.kind !== 'permission' && decision.kind !== 'plan') startCountdown(decision.request.expiresAt, decision.kind === 'question' ? (seconds) => t('decision.countdownQuestion', { count: seconds }) : (seconds) => t('decision.countdownCancel', { count: seconds }));
    if (focusWasOurs && document.hasFocus()) $('decision-title').focus({ preventScroll: true });
  }

  /** The write gate times a request out unanswered (the runtime gives up on the call soon after); the card counts down to it. */
  function startCountdown(expiresAt, countdownText) {
    stopCountdown();
    const tick = () => {
      const label = $('decision-countdown');
      if (!label) return;
      const seconds = Math.max(0, Math.ceil((expiresAt - Date.now()) / 1000));
      label.textContent = ' · ' + countdownText(seconds);
    };
    tick();
    view.countdownTimer = window.setInterval(tick, COUNTDOWN_INTERVAL_MS);
  }

  function stopCountdown() {
    if (view.countdownTimer) window.clearInterval(view.countdownTimer);
    view.countdownTimer = 0;
  }

  /** acceptEdits applies to a plan only: approving it also picks how the writes that follow are confirmed. */
  function answerDecision(allow, acceptEdits = true) {
    const decision = currentDecision();
    if (!decision || view.answeringDecisionKey === decision.key) return;

    // One answer per request: repeated clicks do nothing
    view.answeringDecisionKey = decision.key;
    for (const button of $('decision').querySelectorAll('button')) {
      button.disabled = true;
    }
    if (decision.kind === 'write') call('answerwrite', decision.request.id, allow);
    else if (decision.kind === 'plan') call('answerplanreview', allow, allow ? '' : (($('plan-feedback') || {}).value || '').trim(), allow && acceptEdits);
    else if (decision.kind === 'question') call('answerquestion', decision.request.id, '', true);
    else call('answerpermission', decision.request.id, allow);
  }

  /** Sends the option picked or the own answer to the question on the card; an empty answer does nothing. */
  function answerQuestion(answer) {
    const decision = currentDecision();
    if (!decision || decision.kind !== 'question' || !answer || view.answeringDecisionKey === decision.key) return;

    view.answeringDecisionKey = decision.key;
    for (const control of $('decision').querySelectorAll('button,input')) {
      control.disabled = true;
    }
    call('answerquestion', decision.request.id, answer, false);
  }

  /** After the draft changes: one line keeps the buttons centred on it, from the second line on they stay at the bottom (as in Claude). */
  function growDraft() {
    const host = $('draft-host');
    const lineHeight = parseFloat(getComputedStyle(host).lineHeight);
    $('composer').classList.toggle('is-multiline', draftInput.element.offsetHeight > lineHeight * 2);
    updateSendButton();
  }

  function setDraft(text) {
    draftInput.value = text;
    growDraft();
    scheduleDraftSave();
  }

  /** Sends the draft with a frozen copy of its chips; the draft clears only once the editor accepted it. */
  function submitDraft() {
    const text = draftInput.value;
    if (draftState.submitting) return;

    const command = COMMAND_LINE.exec(text.trim());
    if (command) {
      runSlashCommand(command[1], (command[2] || '').trim());
      return;
    }
    sendDraft(text, false);
  }

  function sendDraft(text, enterPlanMode) {
    if ((!text.trim() && !draftState.images.length) || state.runtimeLost || draftState.submitting || hasInvalidReference()) return;
    if (draftState.images.length && !state.canAttachImages) {
      showCommandNote(state.attachImagesReason || t('composer.imagesBlocked'));
      return;
    }

    // While a turn runs the editor queues the draft instead of sending it; either way it answers through onSubmitResult
    // The draft clears at once so typing can go on; the editor answers through onSubmitResult
    const references = draftReferencesJson();
    draftState.submitting = true;
    draftState.submittedText = text;
    draftState.submittedRefs = draftState.refs;
    draftState.refs = [];
    const images = JSON.stringify(draftState.images.map((image) => ({ mimeType: image.mimeType, data: image.data })));
    draftState.submittedImages = draftState.images;
    draftState.images = [];
    renderAttachments();
    draftState.historyIndex = -1;
    closeMenu();
    setDraft('');
    renderReferences();
    call('submit', text, references, enterPlanMode, images);
  }

  // ---------- Draft images ----------

  /** A paste holding images adds them as thumbnails; returns whether it took the paste (text is left to the input). */
  function pasteImages(event) {
    const files = Array.from((event.clipboardData && event.clipboardData.items) || []).filter((item) => item.kind === 'file' && item.type.startsWith('image/')).map((item) => item.getAsFile()).filter(Boolean);
    if (!files.length) return false;

    event.preventDefault();
    Promise.all(files.map(readImageBlob)).then(addImages, () => showCommandNote(t('note.clipboardImage')));
    return true;
  }

  /** Reads a pasted or dropped blob as { mimeType, data }. */
  function readImageBlob(blob) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve({ mimeType: blob.type, data: String(reader.result).split(',')[1] || '' });
      reader.onerror = () => reject(reader.error);
      reader.readAsDataURL(blob);
    });
  }

  /** Scales an image down to MAX_IMAGE_SIDE and keeps it under MAX_PNG_BYTES; GIFs pass unchanged so they keep their frames. */
  function prepareImage(image) {
    return new Promise((resolve, reject) => {
      const source = 'data:' + image.mimeType + ';base64,' + image.data;
      const element = new Image();
      element.onerror = () => reject(new Error('unreadable image'));
      element.onload = () => {
        const scale = Math.min(1, MAX_IMAGE_SIDE / Math.max(element.naturalWidth, element.naturalHeight));
        const tooLarge = image.data.length * 3 / 4 > MAX_PNG_BYTES;
        if (image.mimeType === 'image/gif' || (scale === 1 && !tooLarge)) {
          resolve({ mimeType: image.mimeType, data: image.data, url: source });
          return;
        }

        const canvas = document.createElement('canvas');
        canvas.width = Math.round(element.naturalWidth * scale);
        canvas.height = Math.round(element.naturalHeight * scale);
        canvas.getContext('2d').drawImage(element, 0, 0, canvas.width, canvas.height);
        let type = image.mimeType === 'image/png' ? 'image/png' : 'image/jpeg';
        let url = canvas.toDataURL(type, JPEG_QUALITY);
        if (type === 'image/png' && url.length * 3 / 4 > MAX_PNG_BYTES) {
          type = 'image/jpeg';
          url = canvas.toDataURL(type, JPEG_QUALITY);
        }
        resolve({ mimeType: type, data: url.split(',')[1], url: url });
      };
      element.src = source;
    });
  }

  /** Attaches images from the clipboard, a file pick or a file drop; refuses with the reason when the model or the moment does not allow images. */
  function addImages(images) {
    if (!images.length) return;
    if (!state.canAttachImages) {
      showCommandNote(state.attachImagesReason || t('note.cannotAddImages'));
      return;
    }

    const room = MAX_DRAFT_IMAGES - draftState.images.length;
    if (images.length > room) showCommandNote(t('note.tooManyImages', { count: MAX_DRAFT_IMAGES }));
    for (const image of images.slice(0, Math.max(room, 0))) {
      prepareImage(image).then((prepared) => {
        if (draftState.images.length >= MAX_DRAFT_IMAGES) return;
        draftState.images.push(prepared);
        renderAttachments();
        updateSendButton();
      }, () => showCommandNote(t('note.imageSkipped')));
    }
  }

  function renderAttachments() {
    const box = $('attachments');
    box.hidden = !draftState.images.length;
    box.innerHTML = draftState.images.map((image, index) => '<div class="attachment"><img src="' + image.url + '" alt="" data-view-image="' + index + '"><button data-remove-image="' + index + '" title="' + t('common.remove') + '" aria-label="' + t('composer.removeImage') + '">' + icons.svg('close', 12) + '</button></div>').join('');
    scheduleDraftSave();
  }

  function renderImageButton() {
    const button = $('add-image');
    button.classList.toggle('off', !state.canAttachImages);
    button.title = state.canAttachImages ? t('composer.addImageTitle') : (state.attachImagesReason || t('note.cannotAddImages'));
  }

  function openImageViewer(url) {
    const viewer = $('image-viewer');
    viewer.querySelector('img').src = url;
    viewer.hidden = false;
  }

  function bindImages() {
    $('add-image').addEventListener('click', () => {
      if (!state.canAttachImages) {
        showCommandNote(state.attachImagesReason || t('note.cannotAddImages'));
        return;
      }
      call('pickimages');
    });
    $('attachments').addEventListener('click', (event) => {
      const remove = event.target.closest('[data-remove-image]');
      if (remove) {
        draftState.images.splice(Number(remove.dataset.removeImage), 1);
        renderAttachments();
        updateSendButton();
        return;
      }
      const view = event.target.closest('[data-view-image]');
      if (view) openImageViewer(draftState.images[Number(view.dataset.viewImage)].url);
    });
    $('image-viewer').addEventListener('click', () => { $('image-viewer').hidden = true; });

    // Load errors do not bubble, so the page listens while they travel down
    markdown.setImageLookup((kind, key) => imageStates.get(imageKey(kind, key)));
    document.addEventListener('error', onImageError, true);
    document.addEventListener('click', (event) => {
      const view = event.target.closest('.tool-detail-body [data-view-image]');
      if (view) openImageViewer(view.dataset.viewImage);
    });
    document.addEventListener('keydown', (event) => {
      if (event.key === 'Escape' && !$('image-viewer').hidden) {
        $('image-viewer').hidden = true;
        event.stopPropagation();
      }
    }, true);
  }

  // ---------- Slash commands ----------

  /** Why a built-in command cannot run now, or an empty string. */
  function commandBlocker(name) {
    if (name === 'compact' && !state.canCompact) return state.compactReason || t('note.cannotCompact');
    if (name === 'plan' && !state.canSetPlanMode) return state.planModeReason || t('note.cannotSwitchPlan');
    if (name === 'undo' && !lastUserEntryId()) return t('note.nothingToUndo');
    if (name === 'undo' && !state.canEditHistory) return state.editHistoryReason || t('note.cannotUndo');
    return '';
  }

  function lastUserEntryId() {
    for (let index = state.order.length - 1; index >= 0; --index) {
      const entry = state.entries.get(state.order[index]);
      if (entry && entry.kind === 'user') return entry.id;
    }
    return 0;
  }

  /** Runs /compact, /plan, /undo or /clear in the editor; a refused command keeps the draft and says why beside the composer. */
  function runSlashCommand(name, args) {
    const blocker = commandBlocker(name);
    if (blocker) {
      showCommandNote(blocker);
      return;
    }

    switch (name) {
      case 'compact':
        if (args) {
          showCommandNote(t('note.compactNoArgs'));
          return;
        }
        call('compactcontext');
        break;
      case 'plan':
        if (args && args !== 'off') {
          // The message goes out once the runtime has plan mode on; chips travel with it
          sendDraft(draftInput.value.replace(/^\s*\/plan\s+/, ''), true);
          return;
        }
        if (args === 'off' && !state.planMode) showCommandNote(t('note.planOff'));
        else if (!args && state.planMode) showCommandNote(t('note.planOn'));
        else call('setplanmode', !args);
        break;
      case 'undo':
        clearCommandDraft();
        openRewind(lastUserEntryId(), draftInput.element);
        return;
      case 'clear':
        clearCommandDraft();
        startNewSession();
        return;
      case 'memory':
        clearCommandDraft();
        showMemoryCard();
        return;
    }
    clearCommandDraft();
  }

  function clearCommandDraft() {
    draftState.historyIndex = -1;
    closeMenu();
    setDraft('');
    updateSendButton();
  }

  function showCommandNote(text) {
    const note = $('command-note');
    note.textContent = text;
    note.hidden = false;
    window.clearTimeout(view.commandNoteTimer);
    view.commandNoteTimer = window.setTimeout(hideCommandNote, COMMAND_NOTE_MS);
  }

  function hideCommandNote() {
    window.clearTimeout(view.commandNoteTimer);
    $('command-note').hidden = true;
  }

  function onSubmitResult(result) {
    draftState.submitting = false;
    if (result.accepted) {
      draftState.submittedImages = [];
      updateSendButton();
      watchReferences();
      return;
    }

    // Refused: the sent draft comes back in front of anything typed since, its chips merged back
    const typed = draftInput.value;
    setDraft(typed ? draftState.submittedText + '\n' + typed : draftState.submittedText);
    draftState.images = draftState.submittedImages.concat(draftState.images).slice(0, MAX_DRAFT_IMAGES);
    draftState.submittedImages = [];
    renderAttachments();
    if (result.reason) showCommandNote(result.reason);
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
      const kind = ref.kind === 'actor' ? 'Actor' : t('reference.asset');
      const title = invalid ? t('reference.invalid') : kind + ' · ' + ref.cls + ' · ' + ref.path;
      const lead = icons.svg(invalid ? 'alert' : ref.kind === 'actor' ? 'locate' : 'file', 12);
      const cls = ref.cls && !invalid ? '<span class="ref-kind">' + esc(ref.cls) + '</span>' : '';
      return '<span class="ref' + (invalid ? ' invalid' : '') + '" title="' + esc(title) + '">' + lead + '<span class="ref-name">' + esc(ref.name) + '</span>' + cls + '<button data-remove-ref="' + index + '" title="' + t('common.remove') + '" aria-label="' + t('reference.remove', { name: esc(ref.name) }) + '">' + icons.svg('close', 12) + '</button></span>';
    }).join('');
    updateSendButton();
    scheduleDraftSave();
  }

  /** Tells the editor which chips and reply object links to keep checking; validity comes back through onReferenceValidity. */
  function watchReferences() {
    const refs = draftState.refs.map((ref) => ({ kind: ref.kind, path: ref.path, name: ref.name, cls: ref.cls }));
    call('watchreferences', JSON.stringify(refs.concat(messageObjects())));
  }

  // ---------- Object links in replies ----------

  /** Distinct { kind, path } of every object link currently in the conversation. */
  function messageObjects() {
    const seen = new Map();
    for (const link of $('messages').querySelectorAll('[data-obj-path]')) {
      seen.set(link.dataset.objPath, { kind: link.dataset.objKind, path: link.dataset.objPath });
    }
    return [...seen.values()];
  }

  /** After a redraw: re-watch when the set of linked objects changed, and mark links whose object is gone. */
  function syncObjectLinks() {
    const key = messageObjects().map((item) => item.kind + ':' + item.path).sort().join('\n');
    if (key !== watchedObjectKey) {
      watchedObjectKey = key;
      watchReferences();
    }
    applyObjectValidity();
  }

  function applyObjectValidity() {
    for (const link of $('messages').querySelectorAll('[data-obj-path]')) {
      const missing = objectValidity.get(link.dataset.objPath) === false;
      if (link.dataset.hint === undefined) link.dataset.hint = link.title;
      link.classList.toggle('missing', missing);
      link.title = missing ? t('reference.gone') + '\n' + link.dataset.objPath : link.dataset.hint;
      if (missing) link.setAttribute('aria-disabled', 'true');
      else link.removeAttribute('aria-disabled');
    }
  }

  /** Single click reveals the object, double click opens it; a missing object does nothing. */
  function revealObject(link, open) {
    if (objectValidity.get(link.dataset.objPath) === false) return;
    call('revealobject', link.dataset.objKind, link.dataset.objPath, open);
  }

  // ---------- Images in replies ----------

  function imageKey(kind, key) {
    return kind + '|' + key;
  }

  /** Figures of one image anywhere on the page (the conversation, a plan under review). */
  function imageFrames(kind, key) {
    return [...document.querySelectorAll('[data-img-kind="' + kind + '"]')].filter((frame) => frame.dataset.imgKey === key);
  }

  /** Redraws the figures of an image with what is now known of it, keeping the view at the end when it was there. */
  function setImageState(kind, key, known) {
    imageStates.set(imageKey(kind, key), known);
    const atEnd = isAtEnd();
    for (const frame of imageFrames(kind, key)) frame.innerHTML = markdown.imageFrame(kind, key, known);
    if (atEnd) scrollToEnd();
  }

  /** Asks the editor for the asset thumbnails on the page it has not been asked for yet, in batches it takes. */
  function requestThumbnails() {
    const paths = [];
    for (const frame of document.querySelectorAll('[data-img-kind="asset"]')) {
      const path = frame.dataset.imgKey;
      if (imageStates.has(imageKey('asset', path)) || requestedThumbnails.has(path)) continue;
      requestedThumbnails.add(path);
      paths.push(path);
    }
    for (let start = 0; start < paths.length; start += THUMBNAIL_REQUEST_BATCH) {
      call('requestthumbnails', JSON.stringify(paths.slice(start, start + THUMBNAIL_REQUEST_BATCH)));
    }
  }

  /** Under a reply that holds more asset images than it shows: how many are left as links, and a button that shows the next page. */
  function moreThumbnailsHtml(entryId, hidden) {
    if (!hidden) return '';
    const next = Math.min(hidden, markdown.thumbnailPage);
    return '<div class="md-figure-more"><span>' + t('thumbnails.hidden', { count: hidden }) + '</span><button class="btn btn-sm" data-more-thumbnails="' + entryId + '">' + t('thumbnails.showNext', { count: next }) + '</button></div>';
  }

  /** "Show more" under a reply: the next page of its asset images turns from links into thumbnails. */
  function showMoreThumbnails(entryId) {
    thumbnailLimits.set(entryId, (thumbnailLimits.get(entryId) || markdown.thumbnailPage) + markdown.thumbnailPage);
    changedIds.add(entryId);
    renderConversation(false);
    changedIds = new Set();
    requestThumbnails();
  }

  /** Thumbnails from the editor, one or more per call: { path: data URL, or null when the asset has none }. */
  function onThumbnails(thumbnails) {
    for (const path in thumbnails || {}) {
      requestedThumbnails.delete(path);
      setImageState('asset', path, thumbnails[path] || false);
    }
  }

  /** A local or web image that fails to load turns into its placeholder, and stays one when the reply redraws. */
  function onImageError(event) {
    const frame = event.target.tagName === 'IMG' ? event.target.closest('[data-img-kind]') : null;
    if (frame) setImageState(frame.dataset.imgKind, frame.dataset.imgKey, false);
  }

  function isOverComposer(event) {
    const target = document.elementFromPoint(event.clientX, event.clientY);
    return !!target && !!target.closest('#composer');
  }

  /**
   * Dragging an object link onto the composer adds it as a reference chip; the editor fills in its class and name (onDrop).
   * Pointer events, because the editor's browser does not run native HTML drag and drop. A drag never also counts as a click.
   */
  function bindObjectDrag() {
    let drag = null;
    let suppressClick = false;

    const endDrag = () => {
      if (drag && drag.ghost) drag.ghost.remove();
      $('composer').classList.remove('drop-target');
      document.body.classList.remove('obj-dragging');
      drag = null;
    };

    $('messages').addEventListener('mousedown', (event) => {
      const link = event.button === 0 ? event.target.closest('[data-obj-path]') : null;
      if (!link || link.classList.contains('missing')) return;

      event.preventDefault();
      drag = { kind: link.dataset.objKind, path: link.dataset.objPath, label: link.textContent, x: event.clientX, y: event.clientY, ghost: null };
    });
    document.addEventListener('mousemove', (event) => {
      if (!drag) return;
      if (!drag.ghost) {
        if (Math.hypot(event.clientX - drag.x, event.clientY - drag.y) < OBJECT_DRAG_THRESHOLD) return;
        drag.ghost = document.createElement('div');
        drag.ghost.className = 'obj-drag-ghost';
        drag.ghost.textContent = drag.label;
        document.body.appendChild(drag.ghost);
        document.body.classList.add('obj-dragging');
      }

      drag.ghost.style.left = (event.clientX + OBJECT_DRAG_OFFSET) + 'px';
      drag.ghost.style.top = (event.clientY + OBJECT_DRAG_OFFSET) + 'px';
      $('composer').classList.toggle('drop-target', isOverComposer(event));
    });
    document.addEventListener('mouseup', (event) => {
      if (!drag) return;
      if (!drag.ghost) {
        drag = null;
        return;
      }

      if (isOverComposer(event)) {
        call('addobjectreference', drag.kind, drag.path);
        draftInput.focus();
      }
      endDrag();
      suppressClick = true;
      window.setTimeout(() => { suppressClick = false; }, 0);
    });
    document.addEventListener('click', (event) => {
      if (!suppressClick) return;

      suppressClick = false;
      event.preventDefault();
      event.stopPropagation();
    }, true);
    document.addEventListener('keydown', (event) => {
      if (event.key !== 'Escape' || !drag || !drag.ghost) return;

      endDrag();
      event.stopPropagation();
    }, true);
    window.addEventListener('blur', endDrag);
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

    for (const path in validity) {
      objectValidity.set(path, validity[path]);
    }
    applyObjectValidity();
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
        const icon = section.icon ? icons.svg(section.icon, 16) : '';
        const tag = item.tag ? '<span class="menu-trail">' + esc(item.tag) + '</span>' : '';
        html += '<button class="menu-item' + (item.off ? ' is-off' : '') + '" role="option" data-menu-item="' + itemIndex + '" aria-selected="' + (itemIndex === menu.index) + '" tabindex="-1">' + icon
          + '<span class="menu-text">' + esc(item.label) + '<small>' + esc(item.detail) + '</small></span>' + tag + '</button>';
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
    const before = draftInput.textBeforeCaret();
    const reference = REFERENCE_TOKEN.exec(before);
    const template = reference ? null : TEMPLATE_TOKEN.exec(before);
    const match = reference || template;
    if (!match || !draftInput.selectionCollapsed()) {
      closeMenu();
      return;
    }

    const mode = reference ? 'reference' : 'template';
    const query = match[2];
    if (menu.mode === mode && menu.query === query) return;

    menu.mode = mode;
    menu.query = query;
    menu.tokenLength = query.length + 1;
    menu.index = 0;
    if (mode === 'template') {
      // Built-in commands first, greyed with the reason when they cannot run; templates come from the project's template directories with every snapshot
      const wanted = query.toLowerCase();
      const commands = COMMANDS.filter((item) => item.name.includes(wanted)).map((item) => {
        const blocker = commandBlocker(item.name);
        return { label: '/' + item.name, detail: blocker || item.detail, body: item.body, command: true, off: !!blocker };
      });
      const items = state.templates.filter((item) => item.name.toLowerCase().includes(wanted)).map((item) => ({ label: '/' + item.name, detail: item.description, body: item.body, tag: item.builtin ? t('suggest.builtin') : '' }));
      menu.sections = [{ label: t('suggest.commands'), icon: 'terminal', items: commands }, { label: t('suggest.templates'), icon: 'file', items }];
      menu.items = commands.concat(items);
      renderMenu(menu.sections, state.templates.length ? t('suggest.noCommandOrTemplate') : t('suggest.noCommandNoTemplates'));
      return;
    }

    window.clearTimeout(menu.timer);
    menu.timer = window.setTimeout(() => call('queryreferences', query), MENU_QUERY_DELAY_MS);
  }

  function onReferenceResults(result) {
    if (menu.mode !== 'reference' || result.query !== menu.query) return;

    const selection = result.selection || [];
    const sections = [
      { label: t('suggest.selection'), icon: 'selection', items: selection.length ? [{ label: t('suggest.selection'), detail: t('suggest.selectionDetail', { count: selection.length }), refs: selection }] : [] },
      { label: t('suggest.actors'), icon: 'locate', items: (result.actors || []).map((ref) => ({ label: ref.name, detail: 'Actor · ' + ref.cls, refs: [ref] })) },
      { label: t('suggest.assets'), icon: 'file', items: (result.assets || []).map((ref) => ({ label: ref.name, detail: ref.cls + ' · ' + ref.path, refs: [ref] })) }
    ];
    menu.items = sections.flatMap((section) => section.items);
    menu.index = Math.min(menu.index, Math.max(0, menu.items.length - 1));
    renderMenu(sections, t('suggest.noObjects'));
    menu.sections = sections;
  }

  function moveMenu(step) {
    if (!menu.items.length) return;
    menu.index = (menu.index + step + menu.items.length) % menu.items.length;
    renderMenu(menu.sections || [], '');
  }

  /** Inserts the chosen item in place of the token: a template's or command's text, or references as chips; nothing is sent, except that a command typed in full runs. */
  function chooseMenu(index) {
    const item = menu.items[index];
    if (!item) return;

    // A command already typed in full runs instead of being inserted again
    if (item.command && draftInput.value.trim() === item.body.trim()) {
      submitDraft();
      return;
    }

    const length = menu.tokenLength;
    closeMenu();
    draftInput.replaceBeforeCaret(length, item.body || '');
    growDraft();
    if (item.refs) addReferences(item.refs);
    draftInput.focus();
  }

  // ---------- History ----------

  function sentPrompts() {
    return state.order.map((id) => state.entries.get(id)).filter((entry) => entry && entry.kind === 'user').map((entry) => entry.text);
  }

  /** Up and Down walk this conversation's sent prompts, only from an empty draft or while already walking them. */
  function browseHistory(step) {
    const draft = draftInput;
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
    if (day === today) return t('sidebar.today');
    if (day === today - DAY_MS) return t('sidebar.yesterday');
    return t('sidebar.earlier');
  }

  function backgroundActivity(session) {
    const parts = [];
    if (RUNNING_STATUSES.has(session.status)) parts.push(t('sidebar.running'));
    if (CONFIRM_STATUSES.has(session.status)) parts.push(t('status.awaitingConfirm'));
    if (session.queuedCount) parts.push(t('sidebar.queued', { count: session.queuedCount }));
    return parts.join(' · ');
  }

  function renderSidebar() {
    const query = $('search').value.trim().toLowerCase();
    const visible = state.sessions.filter((session) => (session.title || t('session.untitled')).toLowerCase().includes(query));

    let html = '';
    let section = null;
    for (const session of visible) {
      const label = sessionSection(session.updatedAt);
      if (label !== section) {
        html += '<div class="section-label">' + label + '</div>';
        section = label;
      }

      // One line per session. A background one shows a turning loader while it runs, a yellow dot while it waits for the
      // user and an accent dot when it finished unseen; the open one shows nothing. The tooltip carries the details.
      const current = session.id === state.currentSessionId;
      const title = session.title || t('session.untitled');
      const attention = current ? '' : ATTENTION_TEXT[session.attention] || '';
      const activity = current ? STATUS_TEXT[state.status] || '' : backgroundActivity(session);
      let mark = '';
      if (!current && RUNNING_STATUSES.has(session.status)) mark = '<span class="session-mark spin" role="img" aria-label="' + t('sidebar.running') + '">' + icons.svg('loader', 14) + '</span>';
      else if (!current && (CONFIRM_STATUSES.has(session.status) || session.attention === 'input')) mark = '<span class="session-mark" role="img" aria-label="' + (attention || t('status.awaitingConfirm')) + '"><span class="dot input"></span></span>';
      else if (attention) mark = '<span class="session-mark" role="img" aria-label="' + attention + '"><span class="dot ' + session.attention + '"></span></span>';
      if (session.inTab) mark = '<span class="session-mark session-tab" role="img" aria-label="' + t('sidebar.inTab') + '">' + icons.svg('external', 14) + '</span>' + mark;
      const tooltip = [title, session.inTab ? t('sidebar.inTab') : '', attention || activity, (session.modelName ? session.modelName + ' · ' : '') + sessionTime(session.updatedAt)].filter(Boolean).join('\n');
      html += '<button class="session" data-session="' + esc(session.id) + '" aria-current="' + current + '" title="' + esc(tooltip) + '"><span class="session-name">' + esc(title) + '</span>' + mark + '</button>';
    }
    if (!visible.length) html = '<p class="sessions-empty">' + (query ? t('sidebar.noMatch') : t('sidebar.empty')) + '</p>';

    $('sessions').innerHTML = html;
    const archived = t('sidebar.archivedCount', { count: state.archived.length });
    $('archived-button').title = archived;
    $('archived-button').setAttribute('aria-label', archived);
    const sandbox = state.permissionMode === 'workspaceWrite' ? t('sidebar.workspaceWrite') : t('sidebar.readOnly');
    $('permission-mode').textContent = t('sidebar.permissionMode', { sandbox: sandbox, shell: state.shellEnabled ? t('sidebar.on') : t('sidebar.off') });
    renderArchivedList();
  }

  function renderArchivedList() {
    const list = $('archived-list');
    if (!state.archived.length) {
      list.innerHTML = '<p class="dialog-empty">' + t('sidebar.noArchived') + '</p>';
      return;
    }
    list.innerHTML = state.archived.map((session) => '<div class="dialog-row"><span title="' + esc(session.title || t('session.untitled')) + '">' + esc(session.title || t('session.untitled')) + '</span><button class="btn btn-sm" data-restore="' + esc(session.id) + '">' + t('sidebar.restore') + '</button><button class="btn btn-sm btn-ghost is-danger" data-delete="' + esc(session.id) + '">' + t('common.delete') + '</button></div>').join('');
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
      item.title = blocked ? t('sidebar.cannotArchive') : '';
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

  /** The sidebar's remembered width, or the default, within its limits. */
  function preferredSidebarWidth() {
    return Math.max(SIDEBAR_MIN_WIDTH, Math.min(view.paneWidths.sidebar || SIDEBAR_DEFAULT_WIDTH, SIDEBAR_MAX_WIDTH));
  }

  /** Room the sidebar may take: the panel less the docked inspector and the conversation's minimum. */
  function sidebarRoom() {
    const inspectorWidth = paneMode('inspector') === 'docked' ? INSPECTOR_WIDTH : 0;
    return ($('workbench').clientWidth || window.innerWidth) - inspectorWidth - CHAT_MIN_WIDTH;
  }

  /** The docked sidebar gets its preferred width, narrowed so the conversation keeps CHAT_MIN_WIDTH, never below its minimum; the inspector's width is fixed in app.css. */
  function applyPaneWidths() {
    if (paneMode('sidebar') !== 'docked') return;
    const width = Math.max(SIDEBAR_MIN_WIDTH, Math.min(preferredSidebarWidth(), sidebarRoom()));
    $('workbench').style.setProperty('--sidebar-width', width + 'px');
  }

  /** A drag or arrow key asked for this sidebar width: clamped like the docked width, it becomes the remembered width. */
  function resizeSidebar(wanted) {
    view.paneWidths.sidebar = Math.round(Math.max(SIDEBAR_MIN_WIDTH, Math.min(wanted, SIDEBAR_MAX_WIDTH, sidebarRoom())));
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
    if (pane === 'sidebar' && view.sessionView) return 'hidden';
    if (pane === 'inspector' && view.empty) return 'hidden';
    if (isDocked(pane)) return view.docked[pane] ? 'docked' : 'hidden';
    return view.overlay === pane ? 'overlay' : 'hidden';
  }

  /** Applies the layout of the current width: only where the panes sit changes, nothing is rebuilt. */
  function applyLayout() {
    const workbench = $('workbench');
    view.layout = layoutForWidth(workbench.clientWidth || window.innerWidth);
    if (view.overlay && isDocked(view.overlay)) view.overlay = null;
    if (view.overlay === 'inspector' && view.empty) view.overlay = null;

    workbench.classList.remove('full', 'wide', 'medium', 'narrow');
    workbench.classList.add(view.layout);
    workbench.classList.toggle('inspector-docked', paneMode('inspector') === 'docked');
    $('sidebar').dataset.mode = paneMode('sidebar');
    $('inspector').dataset.mode = paneMode('inspector');
    $('scrim').hidden = view.overlay === null;

    // An overlay is modal: the rest of the panel cannot be reached until it closes
    $('chat').inert = view.overlay !== null;
    $('stage-header').inert = view.overlay !== null;
    $('sidebar').inert = view.overlay === 'inspector';
    $('inspector').inert = view.overlay === 'sidebar';

    $('toggle-sidebar').setAttribute('aria-expanded', String(paneMode('sidebar') !== 'hidden'));
    $('toggle-sidebar').hidden = view.sessionView;
    $('toggle-inspector').hidden = view.empty;
    $('toggle-inspector').setAttribute('aria-expanded', String(paneMode('inspector') !== 'hidden'));
    applyPaneWidths();
    if (paneMode('inspector') !== 'hidden') renderInspector();
  }

  function firstFocusable(pane) {
    if (pane === 'sidebar') return $('new-session');
    return $('card-head-' + INSPECTOR_CARDS[0]);
  }

  function openPane(pane) {
    if (pane === 'inspector' && view.empty) return;
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
    if (pane === 'inspector' && view.empty) return;
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
    if (status === TOOL_COMPLETED) return t('inspector.toolDone');
    if (status === TOOL_FAILED) return t('inspector.toolFailed');
    return t('inspector.toolRunning');
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
    if (!tools.length) return '<p class="empty-note">' + t('inspector.noTools') + '</p>';

    return tools.map((entry) => {
      const expanded = expandedTools.has(entry.id);
      const body = expanded ? '<div class="tool-detail-body"><h4>' + t('inspector.arguments') + '</h4><pre class="pre">' + esc(prettyJson(entry.input) || t('inspector.noArguments')) + '</pre><h4>' + t('inspector.result') + '</h4>' + toolImages(entry) + '<pre class="pre">' + esc(entry.result || t('inspector.noResult')) + '</pre></div>' : '';
      const selected = view.selectedTool === entry.id ? ' selected' : '';
      return '<div class="tool-detail' + selected + '" id="tool-detail-' + entry.id + '"><button data-expand-tool="' + entry.id + '" aria-expanded="' + expanded + '" title="' + t('inspector.toolToggle') + '">' + toolIcon(entry.toolStatus, 14) + '<span class="tool-name">' + esc(entry.displayName) + '</span><span class="tool-summary">' + esc(entry.summary) + '</span><span class="tool-state">' + toolStateText(entry.toolStatus) + '</span></button>' + body + '</div>';
    }).join('');
  }

  function renderRunLog() {
    if (!state.runLog.length) return '<p class="empty-note">' + t('inspector.noLog') + '</p>';
    return state.runLog.map((item) => '<div class="log' + (item.warning ? ' warning' : '') + '"><time>' + clock(item.time, true) + '</time>' + esc(item.text) + '</div>').join('');
  }

  /** Token count in short form: 99.3k, 1M. */
  function shortTokens(count) {
    if (count >= TOKENS_PER_MILLION) return +(count / TOKENS_PER_MILLION).toFixed(1) + 'M';
    if (count >= TOKENS_PER_THOUSAND) return +(count / TOKENS_PER_THOUSAND).toFixed(1) + 'k';
    return String(count);
  }

  function configName(configId) {
    const option = state.config[configId];
    if (!option || !option.choices) return '—';
    const choice = option.choices.find((item) => item.value === option.current);
    return choice ? choice.name : '—';
  }

  function contextShare() {
    return state.contextSize > 0 ? Math.min(state.usedTokens / state.contextSize, 1) : null;
  }

  /**
   * One line of a card list. Leading mark: a line icon (icon) or a legend swatch (swatch); then the label, an optional
   * tag, the value and an optional trailing column (extra). The label carries the full text as a tooltip.
   */
  function kvRow(label, value, options) {
    const opts = options || {};
    let lead = '';
    if (opts.icon) lead = '<span class="lead">' + icons.svg(opts.icon, 16) + '</span>';
    else if (opts.swatch) lead = '<span class="swatch ' + opts.swatch + '"></span>';
    const tag = opts.tag ? '<span class="chip">' + esc(opts.tag) + '</span>' : '';
    const extra = opts.extra !== undefined ? '<span class="x">' + esc(opts.extra) + '</span>' : '';
    return '<li class="kv' + (opts.icon ? ' lit' : '') + (opts.warn ? ' warn' : '') + (opts.mono ? ' mono' : '') + '">' + lead + '<span class="k" title="' + esc(label) + '">' + esc(label) + '</span>' + tag + '<span class="v">' + esc(value) + '</span>' + extra + '</li>';
  }

  function percentText(share) {
    return (share * 100).toFixed(1) + '%';
  }

  /** Manual compaction (runtime /compact): greyed out with the reason while it cannot run. */
  function compactAction() {
    const label = state.compacting ? t('inspector.compacting') : t('inspector.compact');
    const title = state.canCompact ? t('command.compact') : state.compactReason;
    return '<div class="card-actions"><button class="btn btn-sm" data-compact' + (state.canCompact ? '' : ' disabled') + ' title="' + esc(title) + '">' + label + '</button></div>';
  }

  function renderContextCard() {
    const share = contextShare();
    if (share === null) return '<div class="context-head"><span>' + t('inspector.usageUnknown') + '</span></div><ul class="kv-list">' + kvRow(t('menu.model'), configName('model')) + kvRow(t('menu.effort'), configName('reasoning_effort')) + '</ul>' + compactAction();

    const free = Math.max(state.contextSize - state.usedTokens, 0);
    const parts = contextParts();
    let meter;
    let legend;
    if (parts) {
      meter = '<div class="meter">' + parts.map((part) => '<span class="' + part.key + '" style="width:' + part.tokens / state.contextSize * 100 + '%"></span>').join('') + '</div>';
      legend = parts.map((part) => kvRow(part.label, shortTokens(part.tokens), { swatch: part.key, extra: percentText(part.tokens / state.contextSize) })).join('');
    } else {
      const level = share >= USAGE_FULL_SHARE ? ' full' : share >= USAGE_HIGH_SHARE ? ' high' : '';
      meter = '<div class="meter"><span class="used' + level + '" style="width:' + share * 100 + '%"></span></div>';
      legend = kvRow(t('inspector.used'), shortTokens(state.usedTokens), { swatch: 'used' + level, extra: percentText(share) });
    }
    const note = parts ? ' title="' + t('inspector.partsTitle') + '"' : '';
    return '<div class="context-head"><span>' + t('inspector.usedShare', { percent: Math.round(share * 100) }) + '</span><span class="count">' + shortTokens(state.usedTokens) + ' / ' + shortTokens(state.contextSize) + '</span></div>'
      + meter
      + '<ul class="kv-list legend"' + note + '>' + legend + kvRow(t('inspector.free'), shortTokens(free), { swatch: 'free', extra: percentText(1 - share) }) + '</ul>'
      + '<div class="divider"></div><ul class="kv-list">' + kvRow(t('menu.model'), configName('model')) + kvRow(t('menu.effort'), configName('reasoning_effort')) + '</ul>'
      + compactAction();
  }

  /**
   * The used tokens split like the runtime's estimate (messages, tool definitions, system prompts), largest first as
   * in Claude's panel order; the estimate is shared out over the billed total so the parts add up to it. Null without a breakdown.
   */
  function contextParts() {
    const breakdown = state.contextBreakdown;
    if (!breakdown) return null;

    const estimated = breakdown.messages + breakdown.tools + breakdown.system;
    if (estimated <= 0 || state.usedTokens <= 0) return null;

    const scale = state.usedTokens / estimated;
    const tools = Math.round(breakdown.tools * scale);
    const system = Math.round(breakdown.system * scale);
    return [
      { key: 'messages', label: t('inspector.messages'), tokens: Math.max(state.usedTokens - tools - system, 0) },
      { key: 'tools', label: t('inspector.toolDefinitions'), tokens: tools },
      { key: 'system', label: t('inspector.systemPrompts'), tokens: system }
    ];
  }

  function externalIssueText(server) {
    if (server.issue === 'nameInvalid') return t('mcp.nameInvalid');
    if (server.issue === 'nameTaken') return t('mcp.nameTaken');
    if (server.issue === 'commandNotAbsolute') return t('mcp.commandNotAbsolute');
    if (server.issue === 'urlMissing') return t('mcp.urlMissing');
    if (server.issue === 'headerValueMissing') return t('mcp.headerValueMissing', { name: server.detail });
    return '';
  }

  /** Services the next session gets: the listening editor endpoint and the declarable external servers. */
  function mcpServiceCount() {
    if (!state.mcp) return 0;
    const endpoint = state.mcp.endpoint && state.mcp.endpoint.listening ? 1 : 0;
    return endpoint + state.mcp.servers.filter((server) => !server.issue).length;
  }

  function mcpHasProblem() {
    if (!state.mcp) return false;
    return !(state.mcp.endpoint && state.mcp.endpoint.listening) || state.mcp.servers.some((server) => server.issue);
  }

  /** A collapsible group inside a card: caret, label, count; the rows show while it is open. */
  function cardGroup(id, label, count, rows, empty) {
    const open = view.cardGroups.has(id);
    const body = open ? (rows ? '<ul class="kv-list nested">' + rows + '</ul>' : '<p class="empty-note nested">' + empty + '</p>') : '';
    return '<div class="card-group' + (open ? ' open' : '') + '"><button class="group-head" data-card-group="' + id + '" aria-expanded="' + open + '">' + icons.svg('caret', 14) + '<span>' + label + '</span><span class="c">' + count + '</span></button>' + body + '</div>';
  }

  function renderMcpCard() {
    const mcp = state.mcp;
    if (!mcp) return '<p class="empty-note">' + t('mcp.noStatus') + '</p>';

    const endpoint = mcp.endpoint || { name: '', listening: false, port: 0, tools: [] };
    const tools = endpoint.tools || [];
    const declared = mcp.servers.filter((server) => !server.issue).length;
    const skipped = mcp.servers.length - declared;
    const status = '<ul class="kv-list">'
      + kvRow(t('mcp.endpoint', { name: endpoint.name }), endpoint.listening ? t('mcp.port', { port: endpoint.port }) : t('mcp.notStarted'), { icon: 'monitor', warn: !endpoint.listening })
      + kvRow(t('mcp.servers'), mcp.servers.length ? t('mcp.declared', { count: declared }) + (skipped ? t('mcp.skippedCount', { count: skipped }) : '') : t('mcp.notConfigured'), { icon: 'globe', warn: skipped > 0 })
      + '</ul>';

    const disabled = tools.filter((tool) => !tool.enabled).length;
    const toolRows = tools.map((tool) => kvRow(tool.name, tool.enabled ? '' : t('mcp.off'), { tag: tool.readOnly ? t('mcp.read') : t('mcp.write'), mono: true })).join('');
    const toolsetRows = mcp.toolsets.map((toolset) => {
      let value = t('mcp.toolCount', { count: toolset.toolCount });
      if (toolset.blocked) value = t('mcp.blocked');
      else if (!toolset.enabled) value = t('mcp.off');
      return kvRow(toolset.name, value, { mono: true });
    }).join('');
    const serverRows = mcp.servers.map((server) => kvRow(server.name || t('mcp.unnamed'), server.issue ? t('mcp.skipped', { reason: externalIssueText(server) }) : t('mcp.declaredOne'), { tag: server.stdio ? 'stdio' : 'http', warn: !!server.issue, mono: true })).join('');

    return status
      + cardGroup('mcp-tools', t('mcp.editorTools'), tools.length + (disabled ? t('mcp.disabledCount', { count: disabled }) : ''), toolRows, t('mcp.noEditorTools'))
      + cardGroup('mcp-toolsets', t('mcp.toolsets'), mcp.toolsets.length, toolsetRows, t('mcp.noToolsets'))
      + cardGroup('mcp-servers', t('mcp.servers'), mcp.servers.length, serverRows, t('mcp.noServers'))
      + '<p class="fine">' + t('mcp.serversNote') + '</p>';
  }

  function cardSummary(card) {
    if (card === 'context') {
      const share = contextShare();
      return share === null ? '—' : Math.round(share * 100) + '%';
    }
    if (card === 'memory') return state.memory ? t('inspector.memorySummary', { count: state.memory.items.length, used: memoryKb(state.memory.bytes), max: memoryKb(state.memory.maxBytes) }) : '—';
    if (card === 'mcp') return state.mcp ? t('inspector.mcpSummary', { count: mcpServiceCount() }) : '—';
    if (card === 'tools') {
      const tools = state.order.map((id) => state.entries.get(id)).filter((entry) => entry && entry.kind === 'toolCall');
      const failed = tools.filter((entry) => entry.toolStatus === TOOL_FAILED).length;
      return t('inspector.toolsSummary', { count: tools.length }) + (failed ? t('inspector.failedSummary', { count: failed }) : '');
    }
    const warnings = state.runLog.filter((item) => item.warning).length;
    return t('inspector.logSummary', { count: state.runLog.length }) + (warnings ? t('inspector.warningSummary', { count: warnings }) : '');
  }

  function cardWarns(card) {
    if (card === 'context') {
      const share = contextShare();
      return share !== null && share >= USAGE_HIGH_SHARE;
    }
    if (card === 'memory') return !!state.memory && state.memory.bytes >= state.memory.maxBytes * MEMORY_WARN_SHARE;
    if (card === 'mcp') return mcpHasProblem();
    if (card === 'tools') return state.order.some((id) => { const entry = state.entries.get(id); return entry && entry.kind === 'toolCall' && entry.toolStatus === TOOL_FAILED; });
    return state.runLog.some((item) => item.warning);
  }

  function renderCardBody(card) {
    if (card === 'context') return renderContextCard();
    if (card === 'memory') return renderMemoryCard();
    if (card === 'mcp') return renderMcpCard();
    if (card === 'tools') return renderToolDetails();
    return renderRunLog();
  }

  function memoryKb(bytes) {
    return (bytes / 1024).toFixed(1).replace(/\.0$/, '');
  }

  /** Memory card: every remembered line with its date and a delete button, then the file. Changes reach the model at its next request. */
  function renderMemoryCard() {
    const items = state.memory ? state.memory.items : [];
    const list = items.length
      ? '<ul class="memory-list">' + items.map((item, index) => '<li><span class="memory-text">' + esc(item.text) + (item.date ? '<small>' + esc(item.date) + '</small>' : '') + '</span><button class="btn btn-icon btn-sm memory-forget" data-forget-memory="' + index + '" title="' + t('memory.forget') + '" aria-label="' + t('memory.forget') + '">' + icons.svg('close', 14) + '</button></li>').join('') + '</ul>'
      : '<p class="empty-note">' + t('memory.empty') + '</p>';
    return list + '<div class="memory-foot"><span>' + t('memory.nextRequest') + '</span><button class="btn btn-link" id="memory-open">' + t('memory.openFile') + '</button></div>';
  }

  /** Opens the inspector on the memory card. */
  function showMemoryCard() {
    view.cards.memory = true;
    openPane('inspector');
    renderInspector(['memory']);
    $('card-head-memory').scrollIntoView({ block: 'nearest' });
  }

  /** Updates the given cards (all by default): header state and summary always, the body only while expanded. */
  function renderInspector(cards) {
    for (const card of cards || INSPECTOR_CARDS) {
      const expanded = view.cards[card];
      const head = $('card-head-' + card);
      head.setAttribute('aria-expanded', String(expanded));
      head.parentElement.classList.toggle('open', expanded);

      const summary = $('card-summary-' + card);
      summary.textContent = cardSummary(card);
      summary.classList.toggle('warn', cardWarns(card));

      const body = $('card-body-' + card);
      body.className = 'icard-body' + (card === 'tools' ? ' is-flush is-wide' : card === 'memory' || card === 'log' ? ' is-flush' : '');
      body.hidden = !expanded;
      body.innerHTML = expanded ? renderCardBody(card) : '';
    }
  }

  function toggleCard(card) {
    view.cards[card] = !view.cards[card];
    renderInspector([card]);
  }

  /** Opens the inspector on the tool details of one call (a group opens on its first call). */
  function inspectTool(id) {
    view.selectedTool = id;
    expandedTools.add(id);
    view.cards.tools = true;
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
    return session && session.title ? session.title : t('session.untitled');
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
    $('delete-text').textContent = t('dialog.delete.text', { name: sessionTitle(id) });
    openDialog($('delete-dialog'), sessionButton(id));
  }

  function saveRename() {
    const name = $('rename-input').value.trim();
    if (!name || name.length > state.renameMaxChars) {
      $('rename-error').textContent = t('dialog.rename.error', { count: state.renameMaxChars });
      $('rename-error').hidden = false;
      return;
    }
    call('renamesession', view.targetSessionId, name);
    $('rename-dialog').close();
  }

  // ---------- Editor entry points ----------

  const STATE_KEYS = ['status', 'connected', 'prompting', 'stopping', 'busy', 'runtimeLost', 'startIssue', 'modelSetup', 'resumeFailed', 'needsVerify', 'animateMark', 'usedTokens', 'contextSize', 'contextBreakdown', 'config', 'sessions', 'archived', 'currentSessionId', 'currentTitle', 'canArchiveCurrent', 'renameMaxChars', 'permissionMode', 'shellEnabled', 'templates', 'runLog', 'pendingPermission', 'pendingWrite', 'pendingQuestion', 'queue', 'canRetry', 'mcp', 'canEditHistory', 'editHistoryReason', 'todos', 'compacting', 'canCompact', 'compactReason', 'planMode', 'writeMode', 'canSetPlanMode', 'planModeReason', 'pendingPlanReview', 'canAttachImages', 'attachImagesReason', 'memory'];

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
    if ('animateMark' in message) {
      markPlayer.setEnabled(state.animateMark);
      warpPlayer.setEnabled(state.animateMark);
      document.documentElement.classList.toggle('motion-off', !state.animateMark);
    }
    if ('fonts' in message) applyFonts(message.fonts || {});
    if ('localImageRoot' in message) markdown.setLocalImageRoot(message.localImageRoot);

    // A reopened conversation fetches its thumbnails again, so changed assets show their current look
    if (reset) {
      imageStates.clear();
      requestedThumbnails.clear();
      thumbnailLimits.clear();
      choiceDrafts.clear();
    }
    if ('fontSize' in message) {
      document.documentElement.style.setProperty('--chat-font-size', message.fontSize + 'px');
      growDraft();
    }
    if ('wideLayout' in message) document.body.classList.toggle('wide-content', message.wideLayout === true);
    if ('viewMode' in message) {
      // A session tab shows one session: no session list, and the shortcuts that act on the list do nothing
      view.sessionView = message.viewMode === 'session';
      document.body.classList.toggle('session-view', view.sessionView);
      applyLayout();
    }
    if ('sidebarWidth' in message) {
      view.paneWidths.sidebar = message.sidebarWidth;
      view.paneWidths.inspector = message.inspectorWidth;
      applyPaneWidths();
    }
    if (switchedSession) {
      expandedTools.clear();
      view.selectedTool = null;
      if (!reset) switchDraft(previousSessionId, message.draft);
    }

    // The page (re)loaded, for example a session moved into a tab: the draft the editor kept for this session comes back
    if (reset && 'draft' in message) loadDraft(message.draft);

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
      renderWorking();
    }
    if ('status' in message || 'canRetry' in message || 'startIssue' in message || 'resumeFailed' in message || 'modelSetup' in message) {
      renderHeader();
      renderBanner();
      renderSetup();
    }
    if ('queue' in message || 'busy' in message || 'runtimeLost' in message) renderQueue();
    if ('todos' in message || 'prompting' in message) renderTodos();
    if ('sessions' in message || 'status' in message) {
      renderSidebar();
      renderTitle();

      // The menu's session may be gone (deleted, archived) or running now
      closeSessionMenu(false);
    }
    if ('pendingPermission' in message || 'pendingWrite' in message || 'pendingQuestion' in message || 'pendingPlanReview' in message) renderDecision();
    renderComposer();
    refreshHistoryActions();

    // After a rewind the message's text comes back into the input, once the new session is shown
    if ('restoreDraft' in message) {
      setDraft(message.restoreDraft);
      draftInput.focus();
    }

    const inspectorVisible = paneMode('inspector') !== 'hidden';
    if (inspectorVisible) {
      const cards = [];
      if ('usedTokens' in message || 'contextSize' in message || 'contextBreakdown' in message || 'config' in message || 'compacting' in message || 'canCompact' in message) cards.push('context');
      if ('memory' in message) cards.push('memory');
      if ('mcp' in message) cards.push('mcp');
      if (toolsChanged) cards.push('tools');
      if ('runLog' in message) cards.push('log');
      if (cards.length) renderInspector(cards);
    }

    requestThumbnails();
    if ('prompting' in message) updateBackBottom();
  }

  /** A font stack from Editor Preferences: the names that pass FONT_NAME, quoted, in front of the fallback stack. */
  function fontStack(names, fallback) {
    const valid = String(names || '').split(',').map((name) => name.trim()).filter((name) => FONT_NAME.test(name)).slice(0, MAX_FONT_NAMES);
    return valid.length ? valid.map((name) => '"' + name + '"').join(',') + ',' + fallback : fallback;
  }

  /** Fonts chosen in Editor Preferences; empty values keep the defaults, and the chat font follows the UI font unless set. */
  function applyFonts(fonts) {
    const root = document.documentElement.style;
    root.setProperty('--font-ui', fontStack(fonts.ui, FONT_UI));
    root.setProperty('--font-chat', fontStack(fonts.chat, 'var(--font-ui)'));
    root.setProperty('--font-mono', fontStack(fonts.mono, FONT_MONO));
    growDraft();
  }

  function startNewSession() {
    if (state.currentSessionId) view.switchingSession = true;
    call('newconversation');
    if (view.overlay) closeOverlay();
    draftInput.focus();
  }

  /** Panel shortcuts, matched by the editor against their (rebindable) chords and forwarded here. */
  function runCommand(name) {
    if (document.querySelector('dialog[open]')) return;

    if (view.sessionView && (name === 'toggleSidebar' || name === 'newSession' || name === 'searchSessions')) return;

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
        if (!$('composer').hidden) draftInput.focus();
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
    onImages(images) {
      addImages(images || []);
    },
    onReferenceResults: onReferenceResults,
    onReferenceValidity: onReferenceValidity,
    onThumbnails: onThumbnails,
    onChoiceRejected: onChoiceRejected,
    onSubmitResult: onSubmitResult,
    onQueueItemTaken: onQueueItemTaken,
    onDrawerState(open) {
      document.body.classList.toggle('drawer-open', open);
    },
    focusDraft() {
      if (!$('composer').hidden) draftInput.focus();
    }
  };

  // ---------- Page events ----------

  /** Keys app.js takes before the input (composer-editor.js handles lists, Tab and Shift+Enter): the menu, history and sending. */
  function onDraftKey(event) {
    // An open menu takes the arrows, Enter and Esc first; inserting never sends
    if (menu.mode) {
      if (event.key === 'Escape') {
        event.stopPropagation();
        closeMenu();
        return true;
      }
      if (menu.items.length && (event.key === 'ArrowDown' || event.key === 'ArrowUp')) {
        moveMenu(event.key === 'ArrowDown' ? 1 : -1);
        return true;
      }
      if (menu.items.length && event.key === 'Enter') {
        chooseMenu(menu.index);
        return true;
      }
    }

    if ((event.key === 'ArrowUp' || event.key === 'ArrowDown') && !event.shiftKey && !event.ctrlKey && !event.altKey) return browseHistory(event.key === 'ArrowUp' ? -1 : 1);

    if (event.key !== 'Enter' || event.shiftKey || event.ctrlKey || event.altKey) return false;
    submitDraft();
    return true;
  }

  function bindComposer() {
    draftInput = window.ShuttlegentComposerEditor.create($('draft-host'), {
      keydown: onDraftKey,
      change: () => {
        growDraft();

        // Typing leaves history browsing and drops a command's refusal; the menu follows the token at the caret
        draftState.historyIndex = -1;
        hideCommandNote();
        updateMenu();
        scheduleDraftSave();
      },
      selection: () => {
        if (!draftInput.composing) updateMenu();
      },
      blur: () => window.setTimeout(() => {
        if (!draftInput.hasFocus()) closeMenu();
        saveDraftNow(state.currentSessionId);
      }, 0),
      paste: pasteImages
    });

    // A click on the input's padding still puts the caret in it
    $('draft-host').addEventListener('mousedown', (event) => {
      if (event.target !== $('draft-host')) return;
      event.preventDefault();
      draftInput.focus();
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
    bindImages();

    $('todos').addEventListener('click', (event) => {
      if (!event.target.closest('[data-todos-toggle]')) return;
      const todos = state.todos || [];
      const allDone = todos.length > 0 && todos.every((todo) => todo.status === 'completed');
      const folded = view.todosFolded !== null ? view.todosFolded : allDone && !state.prompting;
      view.todosFolded = !folded;
      renderTodos();
    });

    $('queue').addEventListener('click', (event) => {
      if (event.target.closest('#resume-queue')) {
        call('resumequeue');
        return;
      }
      const take = event.target.closest('[data-take-queued]');
      if (take) {
        call('takequeued', Number(take.dataset.takeQueued), draftInput.value, draftReferencesJson());
        return;
      }
      const remove = event.target.closest('[data-remove-queued]');
      if (remove) call('removequeued', Number(remove.dataset.removeQueued));
    });

    $('send').addEventListener('click', submitDraft);
    $('stop').addEventListener('click', () => {
      if (!state.stopping) call('cancel');
    });

    // Object links inside a plan under review work as in the conversation
    $('decision').addEventListener('click', (event) => {
      const link = event.target.closest('[data-obj-path]');
      if (!link) return;
      event.preventDefault();
      revealObject(link, false);
    });
    $('decision').addEventListener('dblclick', (event) => {
      const link = event.target.closest('[data-obj-path]');
      if (link) revealObject(link, true);
    });
  }

  /** The composer floats over the conversation's bottom: its height pads the conversation, and the end stays in view as it grows. */
  function trackComposerHeight() {
    const column = document.querySelector('.composer-column');
    const observer = new ResizeObserver(() => {
      const following = isAtEnd();
      $('chat').style.setProperty('--composer-height', column.offsetHeight + 'px');
      if (following) $('conversation').scrollTop = $('conversation').scrollHeight;
    });
    observer.observe(column);
  }

  function bindConversation() {
    trackComposerHeight();
    $('back-bottom').addEventListener('click', scrollToEnd);
    $('conversation').addEventListener('scroll', updateBackBottom);

    $('messages').addEventListener('input', (event) => {
      const card = event.target.closest('[data-choice]');
      if (!card) return;

      const draft = choiceDraft(Number(card.dataset.choice));
      if (event.target.dataset.choiceNote !== undefined) draft.notes[event.target.dataset.choiceNote] = event.target.value;
      else if (event.target.dataset.choiceOverall !== undefined) draft.overall = event.target.value;
    });
    $('messages').addEventListener('keydown', (event) => {
      // Ctrl+Enter in a choice card's note fields submits the card
      const field = event.target.closest('[data-choice] textarea');
      if (field && event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        submitChoice(Number(field.closest('[data-choice]').dataset.choice));
        return;
      }

      const option = event.target.closest('[data-choice-option]');
      if (!option || event.target !== option || (event.key !== ' ' && event.key !== 'Enter')) return;

      event.preventDefault();
      const card = option.closest('[data-choice]');
      const optionId = option.dataset.choiceOption;
      toggleChoiceOption(card, optionId);
      const again = [...document.querySelectorAll('[data-choice="' + card.dataset.choice + '"] [data-choice-option]')].find((item) => item.dataset.choiceOption === optionId);
      if (again) again.focus();
    });

    $('messages').addEventListener('dblclick', (event) => {
      const picture = event.target.closest('.choice-image img');
      if (picture) {
        openImageViewer(picture.src);
        return;
      }

      const link = event.target.closest('[data-obj-path]');
      if (link) revealObject(link, true);
    });

    $('working').addEventListener('click', (event) => {
      const run = event.target.closest('[data-run]');
      if (run) {
        const id = Number(run.dataset.run);
        if (expandedRuns.has(id)) expandedRuns.delete(id);
        else expandedRuns.add(id);
        renderWorking();
        return;
      }
      const thought = event.target.closest('[data-thought]');
      if (thought) {
        const id = Number(thought.dataset.thought);
        if (expandedThoughts.has(id)) expandedThoughts.delete(id);
        else expandedThoughts.add(id);
        renderWorking();
        return;
      }
      const tool = event.target.closest('[data-tool]');
      if (tool) inspectTool(Number(tool.dataset.tool));
    });

    $('messages').addEventListener('click', (event) => {
      const choiceSubmit = event.target.closest('[data-choice-submit]');
      if (choiceSubmit) {
        submitChoice(Number(choiceSubmit.dataset.choiceSubmit));
        return;
      }

      const choiceNoteOpen = event.target.closest('[data-choice-note-open]');
      if (choiceNoteOpen) {
        const entryId = Number(choiceNoteOpen.closest('[data-choice]').dataset.choice);
        const optionId = choiceNoteOpen.dataset.choiceNoteOpen;
        choiceDraft(entryId).open.push(optionId);
        redrawChoice(entryId);
        const field = [...document.querySelectorAll('[data-choice="' + entryId + '"] [data-choice-note]')].find((item) => item.dataset.choiceNote === optionId);
        if (field) field.focus();
        return;
      }

      // A click on a choice card's option picks it; its fields and links keep their own clicks, and a double click opens the picture
      const choiceOption = event.target.closest('[data-choice-option]');
      if (choiceOption && !event.target.closest('textarea, button, a')) {
        toggleChoiceOption(choiceOption.closest('[data-choice]'), choiceOption.dataset.choiceOption);
        return;
      }

      const moreThumbnails = event.target.closest('[data-more-thumbnails]');
      if (moreThumbnails) {
        showMoreThumbnails(Number(moreThumbnails.dataset.moreThumbnails));
        return;
      }

      const viewImage = event.target.closest('[data-view-image]');
      if (viewImage) {
        openImageViewer(viewImage.dataset.viewImage);
        return;
      }

      const objectLink = event.target.closest('[data-obj-path]');
      if (objectLink) {
        event.preventDefault();
        revealObject(objectLink, false);
        return;
      }

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

      const run = event.target.closest('[data-run]');
      if (run) {
        const id = Number(run.dataset.run);
        if (expandedRuns.has(id)) expandedRuns.delete(id);
        else expandedRuns.add(id);
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

      const undoMemory = event.target.closest('[data-undo-memory]');
      if (undoMemory) {
        undoMemory.disabled = true;
        call('undomemory', Number(undoMemory.dataset.undoMemory));
        return;
      }

      if (event.target.closest('[data-show-memory]')) {
        showMemoryCard();
        return;
      }

      const copyUser = event.target.closest('[data-user-copy]');
      if (copyUser) {
        const entry = state.entries.get(Number(copyUser.dataset.userCopy));
        if (entry) call('copytext', entry.text || '');
        copyUser.innerHTML = icons.svg('check', 14);
        setTimeout(() => { copyUser.innerHTML = icons.svg('copy', 14); }, COPIED_MS);
        return;
      }

      const rewind = event.target.closest('[data-user-rewind]');
      if (rewind) {
        if (state.canEditHistory) openRewind(Number(rewind.dataset.userRewind), rewind);
        return;
      }

      const branch = event.target.closest('[data-user-branch]');
      if (branch) {
        if (state.canEditHistory) call('branchfrom', Number(branch.dataset.userBranch));
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

    // A row pressed and moved past the threshold is handed to the editor, which turns it into a session tab and starts a native
    // dock tab drag while the button is still down; the click that follows the release is swallowed
    let sessionPress = null;
    let swallowSessionClick = false;
    $('sessions').addEventListener('pointerdown', (event) => {
      const session = event.button === 0 ? event.target.closest('[data-session]') : null;
      sessionPress = session ? { id: session.dataset.session, x: event.clientX, y: event.clientY } : null;
      swallowSessionClick = false;
    });
    $('sessions').addEventListener('pointermove', (event) => {
      if (!sessionPress || (event.buttons & 1) === 0) {
        sessionPress = null;
        return;
      }
      if (Math.hypot(event.clientX - sessionPress.x, event.clientY - sessionPress.y) < SESSION_DRAG_THRESHOLD) return;

      call('beginsessiondrag', sessionPress.id);
      sessionPress = null;
      swallowSessionClick = true;
    });
    $('sessions').addEventListener('pointerup', () => { sessionPress = null; });

    $('sessions').addEventListener('click', (event) => {
      if (swallowSessionClick) {
        swallowSessionClick = false;
        return;
      }

      const session = event.target.closest('[data-session]');
      if (!session) return;

      // A session shown in a session tab stays there: the editor brings that tab to the front
      const held = findSession(session.dataset.session);
      if (held && held.inTab) {
        call('opensession', session.dataset.session);
        if (view.overlay) closeOverlay();
        return;
      }

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

    $('cards').addEventListener('click', (event) => {
      const compact = event.target.closest('[data-compact]');
      if (compact) {
        if (state.canCompact) call('compactcontext');
        return;
      }

      const head = event.target.closest('[data-card]');
      if (head) {
        toggleCard(head.dataset.card);
        return;
      }

      const group = event.target.closest('[data-card-group]');
      if (group) {
        const id = group.dataset.cardGroup;
        if (view.cardGroups.has(id)) view.cardGroups.delete(id);
        else view.cardGroups.add(id);
        renderInspector(['mcp']);
        document.querySelector('[data-card-group="' + id + '"]').focus();
        return;
      }

      const toggle = event.target.closest('[data-expand-tool]');
      if (!toggle) return;

      const id = Number(toggle.dataset.expandTool);
      if (expandedTools.has(id)) expandedTools.delete(id);
      else expandedTools.add(id);
      renderInspector(['tools']);
      $('tool-detail-' + id).querySelector('button').focus();
    });

    window.addEventListener('resize', () => {
      closeSessionMenu(false);
      applyLayout();
    });
  }

  /** The grip on the docked sidebar's inner edge: drag to resize, arrow keys step, double click restores the default; the width is saved when a change ends. */
  function bindResizers() {
    const grip = $('sidebar-resizer');

    grip.addEventListener('pointerdown', (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      grip.setPointerCapture(event.pointerId);
      grip.classList.add('dragging');
      document.body.classList.add('resizing');
    });
    grip.addEventListener('pointermove', (event) => {
      if (!grip.hasPointerCapture(event.pointerId)) return;
      resizeSidebar(event.clientX - $('sidebar').getBoundingClientRect().left);
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
      view.paneWidths.sidebar = 0;
      applyPaneWidths();
      savePaneWidths();
    });
    grip.addEventListener('keydown', (event) => {
      if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
      event.preventDefault();
      resizeSidebar($('sidebar').offsetWidth + (event.key === 'ArrowRight' ? RESIZE_KEY_STEP : -RESIZE_KEY_STEP));
      savePaneWidths();
    });
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
    $('rewind-confirm').addEventListener('click', () => {
      call('rewindto', view.rewindEntryId);
      $('rewind-dialog').close();
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

      // Number keys pick a question's option, unless the own-answer field is being typed in
      const question = currentDecision();
      if (question && question.kind === 'question' && /^[1-9]$/.test(event.key) && !event.ctrlKey && !event.altKey && !event.metaKey && document.activeElement !== $('question-other')) {
        const option = question.request.options[Number(event.key) - 1];
        if (option) {
          event.preventDefault();
          answerQuestion(option);
        }
        return;
      }

      // Esc closes the topmost layer first: an overlay pane, then the decision card (rejects this request only)
      if (event.key === 'Escape') {
        if (view.overlay) {
          event.preventDefault();
          closeOverlay();
          return;
        }
        // A plan is only sent back on purpose, with the button: Esc would throw away the review
        const decision = currentDecision();
        if (decision && decision.kind !== 'plan') {
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
    // The placeholder becomes the icon itself, so layout rules can address the svg as a direct child
    for (const element of document.querySelectorAll('[data-icon]')) {
      element.outerHTML = icons.svg(element.dataset.icon, Number(element.dataset.size || 16));
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
  warpPlayer.attach($('chat'));

  // Whether CEF reports a hidden panel tab decides if the shell must pause the mark itself
  document.addEventListener('visibilitychange', () => console.log('Shuttlegent page visibility: ' + document.visibilityState));
  bindComposer();
  bindConversation();
  bindObjectDrag();
  bindPickers();
  bindMemory();
  window.setInterval(refreshMessageTimes, TIME_REFRESH_MS);
  window.setInterval(refreshElapsed, STEP_TICK_MS);
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
