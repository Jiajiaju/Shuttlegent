/*
 * Markdown for assistant replies, rendered to HTML.
 * Blocks: headings, nested lists, quotes, fenced code, rules, paragraphs, tables (inline Markdown in cells; monospaced source until the separator row arrives).
 * Inline: code, bold, italic, links (http(s) open in the system browser; ue-asset:/ue-actor: and `/Game/...` code become editor object links). Tolerates unclosed structures while a reply streams.
 * Rules follow Shuttlegent.md section 14.3.2.
 */
(function () {
  'use strict';

  const MAX_HEADING_LEVEL = 6;
  const MIN_RULE_MARKS = 3;
  // Deepest list nesting shown; deeper items stay at this level
  const MAX_LIST_LEVEL = 4;
  // "[ ] " or "[x] " after a bullet makes a task item
  const TASK_BOX = /^\[([ xX])\]\s+/;
  const TAB_WIDTH = 4;
  // One cell of a table's separator row: ---, :---, ---:, :---:
  const TABLE_SEPARATOR_CELL = /^:?-{3,}:?$/;
  const LINK_SCHEMES = ['https://', 'http://'];
  // Editor object links (feature plan C6): [label](ue-asset:<object path>) and [label](ue-actor:<object path>)
  const OBJECT_SCHEMES = { 'ue-asset:': 'asset', 'ue-actor:': 'actor' };
  // Inline code holding a whole content path, e.g. `/Game/Maps/BP_Door`, also becomes an asset link
  const CONTENT_PATH = /^\/Game\/[^\s'"`<>()|]+$/;
  const OBJECT_ICON = { asset: 'file', actor: 'locate' };
  const OBJECT_ICON_SIZE = 12;
  const OBJECT_HINT = { asset: '资产 · 单击在内容浏览器中定位，双击打开', actor: 'Actor · 单击在关卡中选中，双击让视口移到它' };

  function escapeHtml(text) {
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  function isWordChar(char) {
    return /[\p{L}\p{N}_]/u.test(char);
  }

  function isFence(trimmed) {
    return trimmed.startsWith('```') || trimmed.startsWith('~~~');
  }

  function isRule(trimmed) {
    if (trimmed.length < MIN_RULE_MARKS) return false;
    const first = trimmed[0];
    if (first !== '-' && first !== '*' && first !== '_') return false;
    for (const char of trimmed) {
      if (char !== first) return false;
    }
    return true;
  }

  function parseHeading(trimmed) {
    let level = 0;
    while (level < trimmed.length && trimmed[level] === '#') {
      ++level;
    }
    if (level === 0 || level > MAX_HEADING_LEVEL || level >= trimmed.length || !/\s/.test(trimmed[level])) return null;
    return { level: level, text: trimmed.slice(level).trim() };
  }

  function leadingSpaces(line) {
    let spaces = 0;
    for (const char of line) {
      if (char === ' ') spaces += 1;
      else if (char === '\t') spaces += TAB_WIDTH;
      else break;
    }
    return spaces;
  }

  function parseListItem(line) {
    const rest = line.trimStart();
    if (rest.length < 2) return null;

    let marker = null;
    let text = null;
    const first = rest[0];
    if ((first === '-' || first === '*' || first === '+') && /\s/.test(rest[1])) {
      marker = '•';
      text = rest.slice(2).trim();

      // A task item shows its box instead of the bullet
      const task = TASK_BOX.exec(text);
      if (task) {
        marker = task[1] === ' ' ? '☐' : '☑';
        text = text.slice(task[0].length);
      }
    } else {
      const ordered = /^(\d+)[.)]\s/.exec(rest);
      if (!ordered) return null;
      marker = ordered[1] + '.';
      text = rest.slice(ordered[0].length).trim();
    }
    return { indent: leadingSpaces(line), marker: marker, text: text };
  }

  function startsBlock(line) {
    const trimmed = line.trim();
    if (!trimmed) return false;
    return isFence(trimmed) || isRule(trimmed) || parseHeading(trimmed) !== null || parseListItem(line) !== null || trimmed[0] === '>' || trimmed[0] === '|';
  }

  function isOpenableLink(url) {
    return LINK_SCHEMES.some((scheme) => url.startsWith(scheme));
  }

  /** A package path (/Game/Dir/Name) gets its object name appended, so every link to one asset carries the same path. */
  function assetObjectPath(path) {
    const name = path.slice(path.lastIndexOf('/') + 1);
    return name.includes('.') ? path : path + '.' + name;
  }

  /** { kind, path } for an editor object link target, or null. */
  function parseObjectLink(url) {
    for (const scheme in OBJECT_SCHEMES) {
      if (!url.startsWith(scheme)) continue;
      const kind = OBJECT_SCHEMES[scheme];
      const path = url.slice(scheme.length).trim();
      if (!path.startsWith('/')) return null;
      return { kind: kind, path: kind === 'asset' ? assetObjectPath(path) : path };
    }
    return null;
  }

  /** labelHtml is already escaped. The page resolves clicks and greys out objects that no longer exist. */
  function objectLink(target, labelHtml) {
    const title = OBJECT_HINT[target.kind] + '\n' + target.path;
    return '<a class="obj-link" href="#" data-obj-kind="' + target.kind + '" data-obj-path="' + escapeHtml(target.path) + '" title="' + escapeHtml(title) + '">' + window.ShuttlegentIcons.svg(OBJECT_ICON[target.kind], OBJECT_ICON_SIZE) + labelHtml + '</a>';
  }

  function inline(text) {
    let out = '';
    let index = 0;
    while (index < text.length) {
      const char = text[index];

      // `code`
      if (char === '`') {
        const end = text.indexOf('`', index + 1);
        if (end > index) {
          const source = text.slice(index + 1, end);
          const code = '<code>' + escapeHtml(source) + '</code>';
          out += CONTENT_PATH.test(source) ? objectLink({ kind: 'asset', path: assetObjectPath(source) }, code) : code;
          index = end + 1;
          continue;
        }
      }

      // **bold** or __bold__
      const pair = text.substr(index, 2);
      if (pair === '**' || pair === '__') {
        const end = text.indexOf(pair, index + 2);
        if (end > index + 2) {
          out += '<strong>' + inline(text.slice(index + 2, end)) + '</strong>';
          index = end + 2;
          continue;
        }
      }

      // *italic* or _italic_ (an underscore inside a word stays literal)
      const italicOpen = (char === '*' || (char === '_' && (index === 0 || !isWordChar(text[index - 1])))) && index + 1 < text.length && !/\s/.test(text[index + 1]);
      if (italicOpen) {
        const end = text.indexOf(char, index + 1);
        const closed = end > index + 1 && (char === '*' || end + 1 >= text.length || !isWordChar(text[end + 1]));
        if (closed) {
          out += '<em>' + escapeHtml(text.slice(index + 1, end)) + '</em>';
          index = end + 1;
          continue;
        }
      }

      // [text](url)
      if (char === '[') {
        const middle = text.indexOf('](', index + 1);
        const end = middle >= 0 ? text.indexOf(')', middle + 2) : -1;
        if (end >= 0) {
          const label = text.slice(index + 1, middle);
          const url = text.slice(middle + 2, end).trim();
          const target = parseObjectLink(url);
          if (target) out += objectLink(target, escapeHtml(label));
          else out += isOpenableLink(url) ? '<a href="' + escapeHtml(url) + '" title="' + escapeHtml(url) + '">' + escapeHtml(label) + '</a>' : escapeHtml(label);
          index = end + 1;
          continue;
        }
      }

      out += escapeHtml(char);
      ++index;
    }
    return out;
  }

  function multiline(text) {
    return text.split('\n').map(inline).join('<br>');
  }

  /** Code block with a copy button; the button carries the block index so the page can copy its source. The source is coloured by its fence language (highlight.js). */
  /** Cells of one table row: splits on |, except inside `code` or when escaped as \|; the outer pipes are optional. */
  function tableCells(row) {
    const cells = [];
    let cell = '';
    let inCode = false;
    for (let index = 0; index < row.length; ++index) {
      const char = row[index];
      if (char === '\\' && row[index + 1] === '|') {
        cell += '|';
        ++index;
        continue;
      }
      if (char === '`') inCode = !inCode;
      if (char === '|' && !inCode) {
        cells.push(cell.trim());
        cell = '';
        continue;
      }
      cell += char;
    }
    cells.push(cell.trim());

    if (row.startsWith('|')) cells.shift();
    if (row.endsWith('|') && !row.endsWith('\\|') && cells.length) cells.pop();
    return cells;
  }

  /** The column alignments of a separator row (|---|:--:|), or null when the row is not one. */
  function tableAlignments(row) {
    const cells = tableCells(row);
    if (!cells.length || !cells.every((cell) => TABLE_SEPARATOR_CELL.test(cell))) return null;
    return cells.map((cell) => {
      const left = cell.startsWith(':');
      const right = cell.endsWith(':');
      if (left && right) return 'center';
      return right ? 'right' : '';
    });
  }

  /** A real table with inline Markdown in each cell; without a separator row yet (reply still streaming) the source stays monospaced. */
  function table(rows) {
    const alignments = rows.length >= 2 ? tableAlignments(rows[1]) : null;
    if (!alignments) return '<pre class="md-table">' + escapeHtml(rows.join('\n')) + '</pre>';

    const columns = Math.max(alignments.length, tableCells(rows[0]).length);
    const renderRow = (row, tag) => {
      const cells = tableCells(row);
      let html = '';
      for (let column = 0; column < columns; ++column) {
        const align = alignments[column] ? ' style="text-align:' + alignments[column] + '"' : '';
        html += '<' + tag + align + '>' + inline(cells[column] || '') + '</' + tag + '>';
      }
      return '<tr>' + html + '</tr>';
    };
    const body = rows.slice(2).map((row) => renderRow(row, 'td')).join('');
    return '<div class="md-table-wrap"><table class="md-grid"><thead>' + renderRow(rows[0], 'th') + '</thead><tbody>' + body + '</tbody></table></div>';
  }

  function codeBlock(language, source, codeIndex) {
    const label = language ? '<span class="code-language">' + escapeHtml(language) + '</span>' : '<span></span>';
    return '<div class="code-block"><div class="code-head">' + label + '<button class="btn btn-icon btn-sm" data-copy-code="' + codeIndex + '" title="复制代码" aria-label="复制代码">' + window.ShuttlegentIcons.svg('copy', 14) + '</button></div><pre><code>' + window.ShuttlegentHighlight.highlight(language, source) + '</code></pre></div>';
  }

  /** Returns { html, codes }: codes holds the source of each code block, indexed by data-copy-code. */
  function render(source) {
    const lines = source.split(/\r?\n/);
    const parts = [];
    const codes = [];
    let paragraph = [];
    // Indents of the list items the current item may nest under; any other block ends the list
    let listIndents = [];

    function closeParagraph() {
      if (!paragraph.length) return;
      parts.push('<p>' + multiline(paragraph.join('\n')) + '</p>');
      paragraph = [];
    }

    for (let index = 0; index < lines.length; ++index) {
      const line = lines[index];
      const trimmed = line.trim();

      const paragraphLine = trimmed !== '' && !startsBlock(line);
      if (!paragraphLine) closeParagraph();
      if (trimmed && !parseListItem(line)) listIndents = [];
      if (paragraphLine) {
        paragraph.push(line.trimEnd());
        continue;
      }
      if (!trimmed) continue;

      if (isFence(trimmed)) {
        // An unclosed fence (reply still streaming) runs to the end
        const codeLines = [];
        let close = index + 1;
        while (close < lines.length && !isFence(lines[close].trim())) {
          codeLines.push(lines[close]);
          ++close;
        }
        codes.push(codeLines.join('\n'));
        parts.push(codeBlock(trimmed.slice(3).trim(), codes[codes.length - 1], codes.length - 1));
        index = close;
        continue;
      }

      const heading = parseHeading(trimmed);
      if (heading) {
        parts.push('<h' + Math.min(heading.level, 4) + ' class="md-h' + Math.min(heading.level, 3) + '">' + inline(heading.text) + '</h' + Math.min(heading.level, 4) + '>');
        continue;
      }

      if (isRule(trimmed)) {
        parts.push('<hr>');
        continue;
      }

      if (trimmed[0] === '>' || trimmed[0] === '|') {
        const mark = trimmed[0];
        const blockLines = [];
        while (index < lines.length) {
          const current = lines[index].trim();
          if (!current || current[0] !== mark) break;
          blockLines.push(mark === '>' ? current.slice(1).trimStart() : current);
          ++index;
        }
        --index;
        parts.push(mark === '>' ? '<blockquote>' + multiline(blockLines.join('\n')) + '</blockquote>' : table(blockLines));
        continue;
      }

      const item = parseListItem(line);
      if (item) {
        // Nesting follows the parents' indents, so 2-space and 3-space (after "1. ") sub-lists both read as one level deeper
        while (listIndents.length && listIndents[listIndents.length - 1] >= item.indent) listIndents.pop();
        const depth = Math.min(listIndents.length, MAX_LIST_LEVEL);
        listIndents.push(item.indent);

        // Indented lines that start no block of their own continue the item
        let text = item.text;
        while (index + 1 < lines.length && leadingSpaces(lines[index + 1]) > 0 && lines[index + 1].trim() && !startsBlock(lines[index + 1])) {
          ++index;
          text += '\n' + lines[index].trim();
        }
        parts.push('<div class="md-item" style="--depth:' + depth + '"><span class="md-marker">' + escapeHtml(item.marker) + '</span><span>' + multiline(text) + '</span></div>');
      }
    }
    closeParagraph();
    return { html: parts.join(''), codes: codes };
  }

  window.ShuttlegentMarkdown = { render: render, escapeHtml: escapeHtml };
})();
