/*
 * Markdown for assistant replies, rendered to HTML.
 * Blocks: headings, nested lists, quotes, fenced code, rules, paragraphs, tables (inline Markdown in cells; monospaced source until the separator row arrives).
 * Inline: code, bold, italic, links (http(s) open in the system browser; ue-asset:/ue-actor: and `/Game/...` code become editor object links),
 * images ![caption](src) from three sources only: ue-asset: thumbnails, local images under Saved/Shuttlegent/, and http(s); any other src stays as text.
 * A paragraph holding nothing but images lays them out in a wrapping row. Tolerates unclosed structures while a reply streams.
 */
(function () {
  'use strict';

  const t = window.ShuttlegentI18n.t;

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
  const OBJECT_HINT = { asset: t('markdown.assetHint'), actor: t('markdown.actorHint') };

  // Images: asset thumbnail sides the reply may ask for (ue-asset:<path>?size=N) and the default; asset thumbnails a reply shows at
  // first and per "show more" (the rest stay object links until then)
  const THUMBNAIL_SIZES = [64, 128, 256];
  const DEFAULT_THUMBNAIL_SIZE = 128;
  const THUMBNAIL_PAGE = 24;
  const THUMBNAIL_SIZE_PARAM = /\?size=(\d+)$/;
  // Local images: the folder a reply may name relative to the project, and the file types shown
  const LOCAL_IMAGE_PREFIX = 'saved/shuttlegent/';
  const LOCAL_IMAGE_TYPE = /\.(png|jpe?g|webp|gif)$/i;
  const FILE_URL_PREFIX = 'file:///';
  // A line made only of images (a paragraph of such lines becomes a row of figures)
  const IMAGE_ONLY_LINE = /^(\s*!\[[^\]]*\]\([^)\s]+\)\s*)+$/;
  const IMAGE_TEXT = { loading: t('markdown.imageLoading'), none: t('markdown.imageNone'), failed: t('markdown.imageFailed') };

  // Set by the page: the file:/// URL of Saved/Shuttlegent/ (ends with /), and what it knows of an image — a src, false when it has none, undefined when not yet known
  let localImageRoot = '';
  let imageLookup = () => undefined;
  // The reply being rendered: asset thumbnails it may show, those placed so far, and those left as links over the limit
  let thumbnailLimit = THUMBNAIL_PAGE;
  let replyThumbnails = 0;
  let hiddenThumbnails = 0;

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

  /** The file URL of a local image under Saved/Shuttlegent/, written relative to the project or as an absolute path; '' for anything else. */
  function localImageUrl(src) {
    if (!localImageRoot || !LOCAL_IMAGE_TYPE.test(src)) return '';

    let path = src.replace(/\\/g, '/');
    if (path.toLowerCase().startsWith(FILE_URL_PREFIX)) path = path.slice(FILE_URL_PREFIX.length);
    if (path.split('/').some((part) => part === '..' || part === '.')) return '';

    const rootPath = localImageRoot.slice(FILE_URL_PREFIX.length).toLowerCase();
    const lower = path.toLowerCase();
    let rest = '';
    if (lower.startsWith(LOCAL_IMAGE_PREFIX)) rest = path.slice(LOCAL_IMAGE_PREFIX.length);
    else if (lower.startsWith(rootPath)) rest = path.slice(rootPath.length);

    // An escaped character could spell .. once the browser decodes the URL
    if (!rest || /[?#%]/.test(rest)) return '';
    return localImageRoot + rest;
  }

  /** { kind: 'asset' | 'local' | 'web', key, size } for an image source the panel shows, or null. key is the asset object path or the image URL. */
  function parseImage(src) {
    const assetScheme = 'ue-asset:';
    if (src.startsWith(assetScheme)) {
      let path = src.slice(assetScheme.length).trim();
      let size = DEFAULT_THUMBNAIL_SIZE;
      const sized = THUMBNAIL_SIZE_PARAM.exec(path);
      if (sized) {
        path = path.slice(0, sized.index);
        if (THUMBNAIL_SIZES.includes(Number(sized[1]))) size = Number(sized[1]);
      }
      if (!/^\/[^\s'"`<>()|?#]+$/.test(path)) return null;
      return { kind: 'asset', key: assetObjectPath(path), size: size };
    }
    if (isOpenableLink(src)) return /[\s'"<>`]/.test(src) ? null : { kind: 'web', key: src };

    const url = localImageUrl(src);
    return url ? { kind: 'local', key: url } : null;
  }

  /**
   * The picture inside a figure. known is what the page knows of the image: a src, false when it has none, undefined when not yet known
   * (only an asset thumbnail waits; local and web images load straight from their URL). Without a picture a grey placeholder says why.
   */
  function imageFrame(kind, key, known) {
    const src = known === undefined && kind !== 'asset' ? key : known;
    if (src) {
      const web = kind === 'web' ? ' referrerpolicy="no-referrer" loading="lazy"' : '';
      return '<img src="' + escapeHtml(src) + '" alt="" data-view-image="' + escapeHtml(src) + '"' + web + '>';
    }

    let text = IMAGE_TEXT.failed;
    if (src === undefined) text = IMAGE_TEXT.loading;
    else if (kind === 'asset') text = IMAGE_TEXT.none;
    const link = kind === 'web' ? '<a href="' + escapeHtml(key) + '" title="' + escapeHtml(key) + '">' + escapeHtml(key) + '</a>' : '';
    return '<span class="md-figure-empty' + (src === undefined ? ' loading' : '') + '"><span>' + text + '</span>' + link + '</span>';
  }

  /** The picture frame for an image source outside a reply (choice card options): the same frame a figure has, or '' when src is not shown. */
  function imageFrameFor(src) {
    const target = parseImage(String(src || '').trim());
    if (!target) return '';
    return '<span class="md-figure-frame" data-img-kind="' + target.kind + '" data-img-key="' + escapeHtml(target.key) + '">' + imageFrame(target.kind, target.key, imageLookup(target.kind, target.key)) + '</span>';
  }

  /** ![caption](src): a figure with its caption underneath (an asset's caption is its object link); a src the panel does not show stays as text. */
  function image(caption, src, source) {
    const target = parseImage(src);
    if (!target) return escapeHtml(source);

    let label = caption.trim() ? escapeHtml(caption.trim()) : '';
    if (target.kind === 'asset') {
      const objectHtml = objectLink({ kind: 'asset', path: target.key }, label || escapeHtml(target.key.slice(target.key.lastIndexOf('.') + 1)));
      if (replyThumbnails >= thumbnailLimit) {
        ++hiddenThumbnails;
        return '<span class="md-figure-overflow">' + objectHtml + '</span>';
      }
      ++replyThumbnails;
      label = objectHtml;
    }

    const size = target.kind === 'asset' ? ' style="--thumb:' + target.size + 'px"' : '';
    const frame = '<span class="md-figure-frame" data-img-kind="' + target.kind + '" data-img-key="' + escapeHtml(target.key) + '">' + imageFrame(target.kind, target.key, imageLookup(target.kind, target.key)) + '</span>';
    return '<span class="md-figure md-figure-' + target.kind + '"' + size + '>' + frame + (label ? '<span class="md-figure-caption">' + label + '</span>' : '') + '</span>';
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

      // ![caption](src)
      if (char === '!' && text[index + 1] === '[') {
        const middle = text.indexOf('](', index + 2);
        const end = middle >= 0 ? text.indexOf(')', middle + 2) : -1;
        if (end >= 0) {
          out += image(text.slice(index + 2, middle), text.slice(middle + 2, end).trim(), text.slice(index, end + 1));
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
    return '<div class="code-block"><div class="code-head">' + label + '<button class="btn btn-icon btn-sm" data-copy-code="' + codeIndex + '" title="' + t('markdown.copyCode') + '" aria-label="' + t('markdown.copyCode') + '">' + window.ShuttlegentIcons.svg('copy', 14) + '</button></div><pre><code>' + window.ShuttlegentHighlight.highlight(language, source) + '</code></pre></div>';
  }

  /**
   * Returns { html, codes, hiddenThumbnails }: codes holds the source of each code block, indexed by data-copy-code; hiddenThumbnails
   * counts the asset images left as links because the reply already shows thumbnailMax (THUMBNAIL_PAGE by default) of them.
   */
  function render(source, thumbnailMax) {
    const lines = source.split(/\r?\n/);
    const parts = [];
    const codes = [];
    let paragraph = [];
    // Indents of the list items the current item may nest under; any other block ends the list
    let listIndents = [];
    thumbnailLimit = thumbnailMax || THUMBNAIL_PAGE;
    replyThumbnails = 0;
    hiddenThumbnails = 0;

    function closeParagraph() {
      if (!paragraph.length) return;
      // Images the panel does not show stay text, so such a paragraph only becomes a row when every image in it is shown
      const counted = replyThumbnails;
      const hidden = hiddenThumbnails;
      const row = paragraph.every((line) => IMAGE_ONLY_LINE.test(line)) ? paragraph.map(inline).join('') : '';
      if (row && !row.includes('![')) {
        parts.push('<div class="md-figures">' + row + '</div>');
      } else {
        replyThumbnails = counted;
        hiddenThumbnails = hidden;
        parts.push('<p>' + multiline(paragraph.join('\n')) + '</p>');
      }
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
    return { html: parts.join(''), codes: codes, hiddenThumbnails: hiddenThumbnails };
  }

  window.ShuttlegentMarkdown = {
    render: render,
    escapeHtml: escapeHtml,
    thumbnailPage: THUMBNAIL_PAGE,
    imageFrame: imageFrame,
    imageFrameFor: imageFrameFor,
    setLocalImageRoot(url) {
      localImageRoot = url && !url.endsWith('/') ? url + '/' : (url || '');
    },
    setImageLookup(lookup) {
      imageLookup = lookup;
    }
  };
})();
