/*
 * Markdown for assistant replies, rendered to HTML.
 * Blocks: headings, nested lists, quotes, fenced code, rules, paragraphs; tables stay as monospaced source.
 * Inline: code, bold, italic, links (only http(s) become links). Tolerates unclosed structures while a reply streams.
 * Rules follow Shuttlegent.md section 14.3.2.
 */
(function () {
  'use strict';

  const MAX_HEADING_LEVEL = 6;
  const MIN_RULE_MARKS = 3;
  const SPACES_PER_LIST_LEVEL = 2;
  const MAX_LIST_LEVEL = 4;
  const TAB_WIDTH = 4;
  const LINK_SCHEMES = ['https://', 'http://'];

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
    } else {
      const ordered = /^(\d+)[.)]\s/.exec(rest);
      if (!ordered) return null;
      marker = ordered[1] + '.';
      text = rest.slice(ordered[0].length).trim();
    }
    return { depth: Math.min(Math.floor(leadingSpaces(line) / SPACES_PER_LIST_LEVEL), MAX_LIST_LEVEL), marker: marker, text: text };
  }

  function startsBlock(line) {
    const trimmed = line.trim();
    if (!trimmed) return false;
    return isFence(trimmed) || isRule(trimmed) || parseHeading(trimmed) !== null || parseListItem(line) !== null || trimmed[0] === '>' || trimmed[0] === '|';
  }

  function isOpenableLink(url) {
    return LINK_SCHEMES.some((scheme) => url.startsWith(scheme));
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
          out += '<code>' + escapeHtml(text.slice(index + 1, end)) + '</code>';
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
          out += isOpenableLink(url) ? '<a href="' + escapeHtml(url) + '" title="' + escapeHtml(url) + '">' + escapeHtml(label) + '</a>' : escapeHtml(label);
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
  function codeBlock(language, source, codeIndex) {
    const label = language ? '<span class="code-language">' + escapeHtml(language) + '</span>' : '<span></span>';
    return '<div class="code-block"><div class="code-head">' + label + '<button class="icon small" data-copy-code="' + codeIndex + '" title="复制代码" aria-label="复制代码">' + window.ShuttlegentIcons.svg('copy', 14) + '</button></div><pre><code>' + window.ShuttlegentHighlight.highlight(language, source) + '</code></pre></div>';
  }

  /** Returns { html, codes }: codes holds the source of each code block, indexed by data-copy-code. */
  function render(source) {
    const lines = source.split(/\r?\n/);
    const parts = [];
    const codes = [];
    let paragraph = [];

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
        parts.push(mark === '>' ? '<blockquote>' + multiline(blockLines.join('\n')) + '</blockquote>' : '<pre class="md-table">' + escapeHtml(blockLines.join('\n')) + '</pre>');
        continue;
      }

      const item = parseListItem(line);
      if (item) {
        // Indented lines that start no block of their own continue the item
        let text = item.text;
        while (index + 1 < lines.length && leadingSpaces(lines[index + 1]) > 0 && lines[index + 1].trim() && !startsBlock(lines[index + 1])) {
          ++index;
          text += '\n' + lines[index].trim();
        }
        parts.push('<div class="md-item" style="--depth:' + item.depth + '"><span class="md-marker">' + escapeHtml(item.marker) + '</span><span>' + multiline(text) + '</span></div>');
      }
    }
    closeParagraph();
    return { html: parts.join(''), codes: codes };
  }

  window.ShuttlegentMarkdown = { render: render, escapeHtml: escapeHtml };
})();
