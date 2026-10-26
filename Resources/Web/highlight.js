/*
 * Syntax colouring for fenced code blocks in replies: one linear scan per block, no library.
 * Languages: C++, AngelScript, C#, JavaScript/TypeScript, Python, JSON, ini, PowerShell, batch, shell; any other block stays plain.
 * Token classes are tk-*; their colours are the --code-* tokens in tokens.css.
 */
(function () {
  'use strict';

  function words(text) {
    return new Set(text.split(' '));
  }

  const C_KEYWORDS = 'if else for while do switch case default break continue return goto try catch throw new delete sizeof typeof static const constexpr consteval inline virtual override final public private protected class struct union enum namespace using typedef template typename operator friend explicit extern mutable volatile auto register noexcept static_assert static_cast dynamic_cast const_cast reinterpret_cast co_await co_return co_yield decltype';
  const C_TYPES = 'void bool char wchar_t char16_t char32_t short int long float double signed unsigned size_t int8 int16 int32 int64 uint8 uint16 uint32 uint64 TCHAR ANSICHAR UTF8CHAR';

  const LANGUAGES = {
    cpp: {
      lineComments: ['//'],
      blockComments: [['/*', '*/']],
      quotes: '"\'',
      keywords: words(C_KEYWORDS),
      types: words(C_TYPES),
      literals: words('true false nullptr NULL this'),
      preprocessor: true,
      unrealNames: true,
      macros: true
    },
    angelscript: {
      lineComments: ['//'],
      blockComments: [['/*', '*/']],
      quotes: '"\'',
      keywords: words('if else for while do switch case default break continue return class struct enum namespace import from const private protected public property override final mixin delegate event access default local auto cast funcdef interface shared external abstract'),
      types: words('void bool int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float float32 float64 double FString FName FText'),
      literals: words('true false nullptr null this Super'),
      preprocessor: true,
      unrealNames: true,
      macros: true
    },
    csharp: {
      lineComments: ['//'],
      blockComments: [['/*', '*/']],
      quotes: '"\'',
      keywords: words('if else for foreach in while do switch case default break continue return try catch finally throw new using namespace class struct interface enum record public private protected internal static readonly const virtual override abstract sealed partial async await var get set init is as typeof nameof where yield ref out params lock base'),
      types: words('void bool byte sbyte char short ushort int uint long ulong float double decimal string object dynamic'),
      literals: words('true false null this'),
      preprocessor: true,
      macros: false
    },
    javascript: {
      lineComments: ['//'],
      blockComments: [['/*', '*/']],
      quotes: '"\'`',
      multilineQuotes: '`',
      keywords: words('if else for while do switch case default break continue return try catch finally throw new delete typeof instanceof in of function class extends const let var async await yield import export from as static get set interface type enum implements declare readonly keyof namespace'),
      types: words('string number boolean any unknown never void object bigint symbol Array Map Set Promise Record'),
      literals: words('true false null undefined this super NaN Infinity')
    },
    python: {
      lineComments: ['#'],
      quotes: '"\'',
      tripleQuotes: true,
      keywords: words('if elif else for while break continue return def class import from as try except finally raise with yield lambda pass global nonlocal assert del in is not and or async await match case'),
      types: words('int float str bool bytes list dict set tuple object type'),
      literals: words('True False None self cls'),
      decorators: true
    },
    json: {
      lineComments: ['//'],
      blockComments: [['/*', '*/']],
      quotes: '"',
      literals: words('true false null'),
      keys: true
    },
    ini: {
      startComments: [';', '#'],
      quotes: '"',
      sections: true,
      assignments: true,
      literals: words('True False true false')
    },
    powershell: {
      lineComments: ['#'],
      blockComments: [['<#', '#>']],
      quotes: '"\'',
      keywords: words('if elseif else for foreach in while do until switch break continue return function param begin process end try catch finally throw trap exit filter class enum using'),
      literals: words('$true $false $null'),
      caseInsensitive: true,
      rawQuotes: true,
      variables: true,
      cmdlets: true,
      parameters: true
    },
    batch: {
      startComments: ['::', 'rem ', 'REM ', '@rem ', '@REM '],
      quotes: '"',
      keywords: words('if else for in do goto call set setlocal endlocal echo exit not exist defined errorlevel pushd popd shift start'),
      caseInsensitive: true,
      rawQuotes: true,
      percentVariables: true
    },
    shell: {
      lineComments: ['#'],
      quotes: '"\'',
      keywords: words('if then else elif fi for in do done while until case esac function return break continue local export readonly source exit set unset shift'),
      variables: true,
      parameters: true
    }
  };

  const ALIASES = {
    cpp: 'cpp', 'c++': 'cpp', c: 'cpp', h: 'cpp', hpp: 'cpp', cc: 'cpp', cxx: 'cpp', inl: 'cpp', hlsl: 'cpp', usf: 'cpp', ush: 'cpp',
    as: 'angelscript', angelscript: 'angelscript',
    cs: 'csharp', csharp: 'csharp', 'c#': 'csharp',
    js: 'javascript', javascript: 'javascript', mjs: 'javascript', jsx: 'javascript', ts: 'javascript', typescript: 'javascript', tsx: 'javascript',
    py: 'python', python: 'python',
    json: 'json', jsonc: 'json', json5: 'json',
    ini: 'ini', cfg: 'ini', conf: 'ini', toml: 'ini', properties: 'ini',
    ps1: 'powershell', psm1: 'powershell', powershell: 'powershell', pwsh: 'powershell', ps: 'powershell',
    bat: 'batch', cmd: 'batch', batch: 'batch',
    sh: 'shell', bash: 'shell', shell: 'shell', zsh: 'shell', console: 'shell'
  };

  const NUMBER = /0[xX][\da-fA-F']+[uUlL]*|(?:\d[\d']*\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFuUlLdDmM]*/y;
  const IDENTIFIER = /[A-Za-z_][\w]*/y;
  const CMDLET = /[A-Za-z]+-[A-Za-z][\w]*/y;
  const VARIABLE = /\$(?:\{[^}\n]*\}|[\w:]+|[?$@#*!0-9])/y;
  const PERCENT_VARIABLE = /%~?[\w]*%?|![\w]+!/y;
  const PARAMETER = /--?[A-Za-z][\w-]*/y;

  // Unreal type prefixes: UObject, AActor, FStruct, TTemplate, EEnum, IInterface followed by a capital
  const UNREAL_TYPE = /^[UAFTEI][A-Z][A-Za-z0-9]/;

  // ALL_CAPS identifiers in C-like code are macros (UPROPERTY, TEXT, UE_LOG)
  const MACRO = /^[A-Z][A-Z0-9_]+$/;

  function matchAt(pattern, source, index) {
    pattern.lastIndex = index;
    const match = pattern.exec(source);
    return match ? match[0] : '';
  }

  function isWordChar(char) {
    return !!char && /[\w$]/.test(char);
  }

  function lineEnd(source, index) {
    const end = source.indexOf('\n', index);
    return end < 0 ? source.length : end;
  }

  function startsWithAny(source, index, prefixes) {
    return (prefixes || []).find((prefix) => source.startsWith(prefix, index)) || null;
  }

  /** End of a string opened at index, past its closing quote; a single-line string also stops at the line end. */
  function stringEnd(source, index, quote, multiline, rawQuotes) {
    let at = index + quote.length;
    while (at < source.length) {
      if (source[at] === '\\' && !rawQuotes) {
        at += 2;
        continue;
      }
      if (source.startsWith(quote, at)) return at + quote.length;
      if (source[at] === '\n' && !multiline) return at;
      ++at;
    }
    return source.length;
  }

  function nextNonSpace(source, index) {
    let at = index;
    while (at < source.length && (source[at] === ' ' || source[at] === '\t')) ++at;
    return source[at];
  }

  function classifyWord(spec, word, source, end) {
    const key = spec.caseInsensitive ? word.toLowerCase() : word;
    if (spec.keywords && spec.keywords.has(key)) return 'keyword';
    if (spec.literals && spec.literals.has(word)) return 'literal';
    if (spec.types && spec.types.has(word)) return 'type';
    if (spec.macros && MACRO.test(word)) return 'meta';
    if (spec.unrealNames && UNREAL_TYPE.test(word)) return 'type';
    if (nextNonSpace(source, end) === '(') return 'func';
    return '';
  }

  /** Splits source into [className, text] runs; className '' is plain text. */
  function tokenize(spec, source) {
    const runs = [];
    let plainStart = 0;
    let index = 0;

    // Only spaces so far on the current line
    let lineStart = true;

    function emit(className, end) {
      if (index > plainStart) runs.push(['', source.slice(plainStart, index)]);
      runs.push([className, source.slice(index, end)]);
      index = end;
      plainStart = end;
      lineStart = false;
    }

    while (index < source.length) {
      const char = source[index];
      const previous = source[index - 1];

      if (lineStart && (startsWithAny(source, index, spec.startComments) || (spec.preprocessor && char === '#'))) {
        emit(spec.preprocessor && char === '#' ? 'meta' : 'comment', lineEnd(source, index));
        continue;
      }
      if (lineStart && spec.sections && char === '[') {
        emit('keyword', lineEnd(source, index));
        continue;
      }
      if (lineStart && spec.assignments && char !== '\n' && char !== ' ' && char !== '\t') {
        const equals = source.indexOf('=', index);
        if (equals > index && equals < lineEnd(source, index)) {
          emit('key', equals);
          continue;
        }
      }

      const block = (spec.blockComments || []).find((pair) => source.startsWith(pair[0], index));
      if (block) {
        const close = source.indexOf(block[1], index + block[0].length);
        emit('comment', close < 0 ? source.length : close + block[1].length);
        continue;
      }

      // A shell # inside a word ($#, a#b) is not a comment
      const lineComment = startsWithAny(source, index, spec.lineComments);
      if (lineComment && !(lineComment === '#' && isWordChar(previous) && !spec.decorators)) {
        emit('comment', lineEnd(source, index));
        continue;
      }

      if (spec.tripleQuotes && (source.startsWith('"""', index) || source.startsWith("'''", index))) {
        emit('string', stringEnd(source, index, source.slice(index, index + 3), true));
        continue;
      }
      if (spec.quotes && spec.quotes.includes(char)) {
        const end = stringEnd(source, index, char, !!spec.multilineQuotes && spec.multilineQuotes.includes(char), !!spec.rawQuotes);
        emit(spec.keys && nextNonSpace(source, end) === ':' ? 'key' : 'string', end);
        continue;
      }

      if (spec.decorators && char === '@' && !isWordChar(previous)) {
        const name = matchAt(IDENTIFIER, source, index + 1);
        if (name) {
          emit('meta', index + 1 + name.length);
          continue;
        }
      }
      if (spec.variables && char === '$') {
        const variable = matchAt(VARIABLE, source, index);
        if (variable) {
          emit(spec.literals && spec.literals.has(variable.toLowerCase()) ? 'literal' : 'variable', index + variable.length);
          continue;
        }
      }
      if (spec.percentVariables && (char === '%' || char === '!')) {
        const variable = matchAt(PERCENT_VARIABLE, source, index);
        if (variable.length > 1) {
          emit('variable', index + variable.length);
          continue;
        }
      }
      if (spec.parameters && char === '-' && (previous === ' ' || previous === '\t')) {
        const parameter = matchAt(PARAMETER, source, index);
        if (parameter) {
          emit('type', index + parameter.length);
          continue;
        }
      }

      if (!isWordChar(previous) && /[\d.]/.test(char)) {
        const number = matchAt(NUMBER, source, index);
        if (number && /\d/.test(number)) {
          emit('number', index + number.length);
          continue;
        }
      }

      if (!isWordChar(previous) && /[A-Za-z_]/.test(char)) {
        const cmdlet = spec.cmdlets ? matchAt(CMDLET, source, index) : '';
        if (cmdlet) {
          emit('func', index + cmdlet.length);
          continue;
        }

        const word = matchAt(IDENTIFIER, source, index);
        const className = classifyWord(spec, word, source, index + word.length);
        if (className) emit(className, index + word.length);
        else index += word.length;
        lineStart = false;
        continue;
      }

      if (char === '\n') lineStart = true;
      else if (char !== ' ' && char !== '\t') lineStart = false;
      ++index;
    }
    if (index > plainStart) runs.push(['', source.slice(plainStart, index)]);
    return runs;
  }

  /** HTML for a code block's source: coloured spans for a known language, escaped plain text otherwise. */
  function highlight(language, source) {
    const escapeHtml = window.ShuttlegentMarkdown.escapeHtml;
    const spec = LANGUAGES[ALIASES[(language || '').toLowerCase()]];
    if (!spec) return escapeHtml(source);

    return tokenize(spec, source).map((run) => (run[0] ? '<span class="tk-' + run[0] + '">' + escapeHtml(run[1]) + '</span>' : escapeHtml(run[1]))).join('');
  }

  window.ShuttlegentHighlight = { highlight: highlight };
})();
