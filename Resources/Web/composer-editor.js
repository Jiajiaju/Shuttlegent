/*
 * The message input: a Tiptap editor (vendor/tiptap.js) that shows Markdown formatting while typing, as Claude's input does,
 * and hands app.js a textarea-like object whose value is Markdown.
 * Typing "- ", "1. ", "- [ ] ", "> ", "# " or "```" starts that block; **bold**, *italic* and `code` format once closed.
 * Keys: Shift+Enter continues a list (an empty item ends it) or breaks the line; Tab / Shift+Tab nest and lift list items and never
 * move focus; Enter and the arrows go to app.js first (send, menu, history). Pasted text is read as Markdown.
 * Markdown out is written here, not by Tiptap's serializer, so text goes out exactly as typed (BP_Door_01 stays unescaped).
 */
(function () {
  'use strict';

  // Spaces Tab types outside a list
  const TAB_TEXT = '  ';
  // Indent of a nested bullet list under its item; ordered items indent by their marker's width so the nesting is CommonMark
  const BULLET_INDENT = '  ';

  // ---------- Markdown out ----------

  /** Wraps text in the Markdown of its marks; code goes innermost, with enough backticks to hold any inside it. */
  function markText(node) {
    let text = node.text || '';
    const marks = (node.marks || []).map((mark) => mark.type);
    if (marks.includes('code')) {
      const fence = text.includes('`') ? '``' : '`';
      text = fence + (text.startsWith('`') ? ' ' : '') + text + (text.endsWith('`') ? ' ' : '') + fence;
    }
    if (marks.includes('strike')) text = '~~' + text + '~~';
    if (marks.includes('italic')) text = '*' + text + '*';
    if (marks.includes('bold')) text = '**' + text + '**';
    const link = (node.marks || []).find((mark) => mark.type === 'link');
    if (link) text = '[' + text + '](' + link.attrs.href + ')';
    return text;
  }

  function inlineText(content) {
    return (content || []).map((node) => node.type === 'hardBreak' ? '\n' : markText(node)).join('');
  }

  /** Puts a prefix in front of the first line and an indent of the same width in front of the others. */
  function prefixLines(lines, first, rest) {
    return lines.map((line, index) => (index === 0 ? first : line ? rest : '') + line);
  }

  /** Lines of a list: each item's marker before its first block, the rest of the item indented under it. */
  function listLines(node) {
    const start = node.attrs && node.attrs.start ? node.attrs.start : 1;
    const lines = [];
    (node.content || []).forEach((item, index) => {
      let marker = '- ';
      if (node.type === 'orderedList') marker = (start + index) + '. ';
      if (node.type === 'taskList') marker = item.attrs && item.attrs.checked ? '- [x] ' : '- [ ] ';
      const indent = node.type === 'orderedList' ? ' '.repeat(marker.length) : BULLET_INDENT;
      lines.push(...prefixLines(blockLines(item.content || [], true), marker, indent));
    });
    return lines;
  }

  /** Lines of a run of blocks; blocks in a list item sit tight, others are separated by a blank line. */
  function blockLines(blocks, tight) {
    const lines = [];
    blocks.forEach((block, index) => {
      if (index > 0 && !tight) lines.push('');
      switch (block.type) {
        case 'paragraph': {
          lines.push(...inlineText(block.content).split('\n'));
          break;
        }
        case 'heading': {
          lines.push('#'.repeat(block.attrs.level) + ' ' + inlineText(block.content));
          break;
        }
        case 'blockquote': {
          lines.push(...blockLines(block.content || [], false).map((line) => line ? '> ' + line : '>'));
          break;
        }
        case 'codeBlock': {
          lines.push('```' + (block.attrs && block.attrs.language ? block.attrs.language : ''), ...inlineText(block.content).split('\n'), '```');
          break;
        }
        case 'horizontalRule': {
          lines.push('---');
          break;
        }
        case 'bulletList':
        case 'orderedList':
        case 'taskList': {
          lines.push(...listLines(block));
          break;
        }
        default: {
          lines.push(inlineText(block.content));
        }
      }
    });
    return lines;
  }

  /** Markdown of the editor's document (getJSON()); empty paragraphs at the ends drop off. */
  function toMarkdown(doc) {
    return blockLines(doc.content || [], false).join('\n').replace(/^\n+|\s+$/g, '');
  }

  /**
   * The input has no link or image node, so pasted [label](url) and ![caption](src) would keep only their label. Escaping the
   * opening bracket keeps them as plain text, and toMarkdown sends them on exactly as pasted.
   */
  function keepLinkSyntax(text) {
    return text.replace(/(!?)\[([^\]\n]*)\]\(/g, '$1\\[$2](');
  }

  // ---------- Editor ----------

  /**
   * Builds the input in host. hooks: keydown(event) → true when app.js handled the key; change(); selection(); blur();
   * paste(event) → true when app.js took the paste (images).
   */
  function create(host, hooks) {
    const tiptap = window.ShuttlegentTiptap;
    let editor = null;

    const Keys = tiptap.Extension.create({
      name: 'composerKeys',
      // Ahead of the list extensions' own Tab handling, which lets Tab through when an item cannot nest
      priority: 1000,
      addKeyboardShortcuts() {
        return {
          // Shift+Enter is the plain editor's Enter: new line in code, next list item (an empty one leaves the list), new paragraph
          'Shift-Enter': () => this.editor.commands.first(({ commands }) => [
            () => commands.newlineInCode(),
            () => commands.splitListItem('taskItem'),
            () => commands.splitListItem('listItem'),
            () => commands.createParagraphNear(),
            () => commands.liftEmptyBlock(),
            () => commands.splitBlock()
          ]),
          'Tab': () => {
            const current = this.editor;
            if (current.commands.sinkListItem('taskItem') || current.commands.sinkListItem('listItem')) return true;
            if (current.isActive('listItem') || current.isActive('taskItem')) return true;
            return current.commands.insertContent(TAB_TEXT);
          },
          'Shift-Tab': () => {
            const current = this.editor;
            if (!current.commands.liftListItem('taskItem')) current.commands.liftListItem('listItem');
            return true;
          }
        };
      }
    });

    editor = new tiptap.Editor({
      element: host,
      extensions: [
        tiptap.StarterKit.configure({ link: false, underline: false, dropcursor: false, trailingNode: false }),
        tiptap.TaskList,
        tiptap.TaskItem.configure({ nested: true }),
        tiptap.Markdown,
        Keys
      ],
      editorProps: {
        attributes: { id: 'draft', role: 'textbox', 'aria-multiline': 'true', 'aria-label': '消息', 'aria-autocomplete': 'list', 'aria-controls': 'suggestions' },
        // app.js sees every key first, except while an input method composes
        handleKeyDown: (view, event) => !view.composing && hooks.keydown(event),
        handlePaste: (view, event) => {
          if (hooks.paste(event)) return true;
          const text = event.clipboardData ? event.clipboardData.getData('text/plain') : '';
          if (!text || editor.isActive('codeBlock')) return false;
          editor.commands.insertContent(keepLinkSyntax(text), { contentType: 'markdown' });
          return true;
        }
      },
      onUpdate: () => {
        refreshEmpty();
        if (!editor.view.composing) hooks.change();
      },
      onSelectionUpdate: () => hooks.selection(),
      onBlur: () => hooks.blur()
    });

    /**
     * The placeholder shows only while the input holds one empty paragraph and no input method is composing. Read from the DOM as
     * well as the document, because an input method's text can be on screen before it reaches the document.
     */
    function refreshEmpty() {
      const dom = editor.view.dom;
      const blank = editor.isEmpty && !dom.textContent && dom.childElementCount <= 1 && (!dom.firstElementChild || dom.firstElementChild.tagName === 'P');
      host.classList.toggle('is-empty', blank && !editor.view.composing);
    }

    const dom = editor.view.dom;
    dom.addEventListener('compositionstart', () => host.classList.remove('is-empty'));
    dom.addEventListener('input', refreshEmpty);
    dom.addEventListener('compositionend', () => {
      // The editor takes the composed text in after this event
      window.setTimeout(() => {
        refreshEmpty();
        hooks.change();
      }, 0);
    });
    refreshEmpty();

    function setMarkdown(markdown) {
      editor.commands.setContent(keepLinkSyntax(markdown || ''), { contentType: 'markdown', emitUpdate: false });
      refreshEmpty();

      // The caret goes to the end; focus moves only if the input had it
      if (editor.isFocused) editor.commands.focus('end', { scrollIntoView: true });
      else editor.commands.setTextSelection(editor.state.doc.content.size);
    }

    /** The current block's text from its start to the caret, for the @ and / tokens. */
    function textBeforeCaret() {
      const selection = editor.state.selection;
      const from = selection.$from;
      return editor.state.doc.textBetween(from.start(), selection.from, '\n', '\n');
    }

    /** Replaces the count characters before the caret with Markdown (a template body) or plain text. */
    function replaceBeforeCaret(count, markdown) {
      const to = editor.state.selection.from;
      const chain = editor.chain().focus().deleteRange({ from: to - count, to: to });
      if (!markdown) chain.run();
      else if (/[\n*`#>[\]_~-]|^\d+\./.test(markdown)) chain.insertContent(keepLinkSyntax(markdown), { contentType: 'markdown' }).run();
      else chain.insertContent(markdown).run();
    }

    return {
      element: editor.view.dom,
      get value() { return toMarkdown(editor.getJSON()); },
      set value(markdown) { setMarkdown(markdown); },
      set placeholder(text) { host.dataset.placeholder = text; },
      get composing() { return editor.view.composing; },
      focus() { editor.commands.focus(); },
      hasFocus() { return editor.isFocused; },
      selectionCollapsed() { return editor.state.selection.empty; },
      textBeforeCaret: textBeforeCaret,
      replaceBeforeCaret: replaceBeforeCaret
    };
  }

  window.ShuttlegentComposerEditor = { create: create, toMarkdown: toMarkdown };
})();
