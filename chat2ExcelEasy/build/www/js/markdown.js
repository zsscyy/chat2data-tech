/* ==========================================================================
   Chat2Excel Markdown 渲染器
   - 支持标题、段落、列表、引用、表格、分割线、行内代码、链接、粗体、斜体
   - 支持代码块语法高亮（无需外部依赖）
   - 流式场景下允许未闭合的代码块
   ========================================================================== */

(function (global) {
    'use strict';

    /* ------------------------------ 基础工具 ------------------------------ */

    var ESCAPE_MAP = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#39;'
    };

    function escapeHtml(text) {
        return String(text == null ? '' : text).replace(/[&<>"']/g, function (ch) {
            return ESCAPE_MAP[ch];
        });
    }

    function decodeAmpersand(text) {
        return String(text).replace(/&amp;/g, '&');
    }

    /* ------------------------------ 代码高亮 ------------------------------ */

    function R(re, cls) {
        return { re: re, cls: cls };
    }

    function buildSpec(items, flags) {
        var pattern = items.map(function (item) {
            return '(' + item.re.source + ')';
        }).join('|');
        return {
            regex: new RegExp(pattern, flags || 'g'),
            classes: items.map(function (item) {
                return item.cls;
            })
        };
    }

    var JS_KEYWORDS = 'false|true|null|undefined|function|const|let|var|return|if|else|for|while|do|switch|case|default|break|continue|new|class|extends|super|this|typeof|instanceof|delete|void|in|of|try|catch|finally|throw|async|await|yield|import|export|from|as|static|get|set';

    var CPP_KEYWORDS = 'alignas|alignof|auto|bool|break|case|catch|char|class|const|constexpr|continue|decltype|default|delete|do|double|else|enum|explicit|extern|false|float|for|friend|goto|if|inline|int|long|mutable|namespace|new|noexcept|nullptr|operator|private|protected|public|register|return|short|signed|sizeof|static|static_cast|struct|switch|template|this|throw|try|typedef|typeid|typename|union|unsigned|using|virtual|void|volatile|while|true|override|final|std|vector|string|size_t|uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|int64_t';

    var PY_KEYWORDS = 'def|class|return|if|elif|else|for|while|in|not|and|or|is|None|True|False|import|from|as|with|try|except|finally|raise|lambda|yield|global|nonlocal|pass|break|continue|async|await|self|print';

    var GO_KEYWORDS = 'func|package|import|var|const|type|struct|interface|map|chan|go|defer|return|if|else|for|range|switch|case|default|break|continue|select|nil|true|false|make|new|append|len|cap';

    var SPECS = {
        javascript: buildSpec([
            R(/\/\/[^\n]*|\/\*[\s\S]*?\*\//, 'tok-comment'),
            R(/"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'|`(?:\\.|[^`\\])*`/, 'tok-string'),
            R(new RegExp('\\b(?:' + JS_KEYWORDS + ')\\b'), 'tok-keyword'),
            R(/\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b/, 'tok-number'),
            R(/[A-Za-z_$][\w$]*(?=\s*\()/, 'tok-function'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        python: buildSpec([
            R(/#[^\n]*/, 'tok-comment'),
            R(/"""[\s\S]*?"""|'''[\s\S]*?'''|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'/, 'tok-string'),
            R(new RegExp('\\b(?:' + PY_KEYWORDS + ')\\b'), 'tok-keyword'),
            R(/\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b/, 'tok-number'),
            R(/[A-Za-z_][\w]*(?=\s*\()/, 'tok-function'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        cpp: buildSpec([
            R(/\/\/[^\n]*|\/\*[\s\S]*?\*\//, 'tok-comment'),
            R(/"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'/, 'tok-string'),
            R(/#[^\n]*/, 'tok-meta'),
            R(new RegExp('\\b(?:' + CPP_KEYWORDS + ')\\b'), 'tok-keyword'),
            R(/\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b/, 'tok-number'),
            R(/[A-Za-z_][\w:<>]*(?=\s*\()/, 'tok-function'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        java: buildSpec([
            R(/\/\/[^\n]*|\/\*[\s\S]*?\*\//, 'tok-comment'),
            R(/"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'/, 'tok-string'),
            R(/\b(?:public|private|protected|class|interface|enum|extends|implements|static|final|void|int|long|double|float|boolean|char|byte|short|new|return|if|else|for|while|do|switch|case|default|break|continue|try|catch|finally|throw|throws|this|super|package|import|null|true|false)\b/, 'tok-keyword'),
            R(/\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b/, 'tok-number'),
            R(/[A-Za-z_][\w]*(?=\s*\()/, 'tok-function'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        json: buildSpec([
            R(/"(?:\\.|[^"\\\n])*"/, 'tok-string'),
            R(/\b(?:true|false|null)\b/, 'tok-keyword'),
            R(/\b-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b/, 'tok-number'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        sql: buildSpec([
            R(/--[^\n]*|\/\*[\s\S]*?\*\//, 'tok-comment'),
            R(/'(?:''|\\.|[^'\\\n])*'|"(?:\\.|[^"\\\n])*"/, 'tok-string'),
            R(/\b(?:select|from|where|group|by|order|having|limit|offset|insert|into|values|update|set|delete|create|table|drop|alter|join|left|right|inner|outer|on|as|and|or|not|null|is|in|exists|between|like|case|when|then|else|end|count|sum|avg|min|max|distinct|union|all|primary|key|index|default|asc|desc)\b/, 'tok-keyword'),
            R(/\b-?\d+(?:\.\d+)?\b/, 'tok-number'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ], 'gi'),
        bash: buildSpec([
            R(/#[^\n]*/, 'tok-comment'),
            R(/"(?:\\.|[^"\\\n])*"|'[^'\n]*'/, 'tok-string'),
            R(/\$[A-Za-z_][\w]*|\${[^}]*}/, 'tok-meta'),
            R(/\b(?:if|then|else|elif|fi|for|while|until|do|done|case|esac|function|return|in|echo|cd|export|local|read|shift|exit|source)\b/, 'tok-keyword'),
            R(/\b\d+\b/, 'tok-number'),
            R(/[{}()\[\];,.:|&]/, 'tok-punct')
        ]),
        go: buildSpec([
            R(/\/\/[^\n]*|\/\*[\s\S]*?\*\//, 'tok-comment'),
            R(/"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'|`(?:\\.|[^`\\])*`/, 'tok-string'),
            R(new RegExp('\\b(?:' + GO_KEYWORDS + ')\\b'), 'tok-keyword'),
            R(/\b0[xX][0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b/, 'tok-number'),
            R(/[A-Za-z_][\w]*(?=\s*\()/, 'tok-function'),
            R(/[{}()\[\];,.:]/, 'tok-punct')
        ]),
        xml: buildSpec([
            R(/<!--[\s\S]*?-->/, 'tok-comment'),
            R(/<\/?[A-Za-z][^\s/>]*/, 'tok-tag'),
            R(/[A-Za-z_:][\w:.-]*(?=\s*=)/, 'tok-attr'),
            R(/"[^"]*"|'[^']*'/, 'tok-string')
        ])
    };

    var ALIASES = {
        js: 'javascript',
        javascript: 'javascript',
        jsx: 'javascript',
        ts: 'javascript',
        typescript: 'javascript',
        node: 'javascript',
        py: 'python',
        python: 'python',
        c: 'cpp',
        h: 'cpp',
        cpp: 'cpp',
        'c++': 'cpp',
        cc: 'cpp',
        cxx: 'cpp',
        hpp: 'cpp',
        hh: 'cpp',
        java: 'java',
        json: 'json',
        jsonc: 'json',
        sql: 'sql',
        mysql: 'sql',
        postgresql: 'sql',
        bash: 'bash',
        sh: 'bash',
        shell: 'bash',
        zsh: 'bash',
        console: 'bash',
        go: 'go',
        golang: 'go',
        html: 'xml',
        xml: 'xml',
        svg: 'xml'
    };

    function resolveLang(lang) {
        var key = String(lang || '').toLowerCase();
        if (!key) {
            return '';
        }
        return ALIASES[key] || (SPECS[key] ? key : '');
    }

    function highlightCode(code, lang) {
        var name = resolveLang(lang);
        var spec = name ? SPECS[name] : null;
        if (!spec) {
            return escapeHtml(code);
        }

        var regex = new RegExp(spec.regex.source, spec.regex.flags);
        var out = '';
        var last = 0;
        var match;

        while ((match = regex.exec(code)) !== null) {
            var token = match[0];
            var cls = '';
            for (var g = 1; g < match.length; g++) {
                if (match[g] !== undefined) {
                    cls = spec.classes[g - 1];
                    break;
                }
            }
            out += escapeHtml(code.slice(last, match.index));
            if (cls) {
                out += '<span class="' + cls + '">' + escapeHtml(token) + '</span>';
            } else {
                out += escapeHtml(token);
            }
            last = match.index + token.length;
            if (token.length === 0) {
                regex.lastIndex++;
            }
        }

        out += escapeHtml(code.slice(last));
        return out;
    }

    /* ------------------------------ 行内渲染 ------------------------------ */

    function renderInline(text) {
        var stores = [];
        var escaped = escapeHtml(text);

        // 行内代码
        escaped = escaped.replace(/`([^`\n]+)`/g, function (m, code) {
            stores.push('<code class="inline-code">' + code + '</code>');
            return '\u0001' + (stores.length - 1) + '\u0002';
        });

        // 链接（仅允许安全协议与相对路径）
        escaped = escaped.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, function (m, label, url) {
            var href = decodeAmpersand(url);
            if (!/^(?:https?:\/\/|mailto:|\/|#|\.\/|\.\.\/)/i.test(href)) {
                return label;
            }
            stores.push('<a class="md-link" href="' + escapeHtml(href) +
                '" target="_blank" rel="noopener noreferrer">' + label + '</a>');
            return '\u0001' + (stores.length - 1) + '\u0002';
        });

        // 粗体、删除线、斜体
        escaped = escaped.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
        escaped = escaped.replace(/__([^_]+)__/g, '<strong>$1</strong>');
        escaped = escaped.replace(/~~([^~]+)~~/g, '<del>$1</del>');
        escaped = escaped.replace(/\*([^*\n]+)\*/g, '<em>$1</em>');

        // 还原占位符
        escaped = escaped.replace(/\u0001(\d+)\u0002/g, function (m, index) {
            return stores[Number(index)] || '';
        });

        return escaped.replace(/\n/g, '<br>');
    }

    /* ------------------------------ 块级渲染 ------------------------------ */

    function isBlank(line) {
        return !line || !line.trim();
    }

    function splitTableRow(row) {
        var text = row.trim();
        if (text.charAt(0) === '|') {
            text = text.slice(1);
        }
        if (text.charAt(text.length - 1) === '|') {
            text = text.slice(0, -1);
        }
        return text.split('|').map(function (cell) {
            return cell.trim();
        });
    }

    function isTableSeparator(line) {
        if (!line || line.indexOf('|') === -1) {
            return false;
        }
        var cells = splitTableRow(line);
        if (!cells.length) {
            return false;
        }
        for (var i = 0; i < cells.length; i++) {
            if (!/^:?-{1,}:?$/.test(cells[i])) {
                return false;
            }
        }
        return true;
    }

    function renderTable(headerLine, separatorLine, bodyLines) {
        var headers = splitTableRow(headerLine);
        var separators = splitTableRow(separatorLine);
        var aligns = headers.map(function (h, index) {
            var sep = separators[index] || '';
            if (sep.charAt(0) === ':' && sep.charAt(sep.length - 1) === ':') {
                return 'center';
            }
            if (sep.charAt(sep.length - 1) === ':') {
                return 'right';
            }
            return 'left';
        });

        function cellTag(tag, content, index) {
            return '<' + tag + ' style="text-align:' + aligns[index] + '">' +
                renderInline(content || '') + '</' + tag + '>';
        }

        var html = '<div class="md-table-wrap"><table class="md-table"><thead><tr>';
        headers.forEach(function (cell, index) {
            html += cellTag('th', cell, index);
        });
        html += '</tr></thead><tbody>';

        bodyLines.forEach(function (row) {
            var cells = splitTableRow(row);
            html += '<tr>';
            headers.forEach(function (_, index) {
                html += cellTag('td', cells[index] || '', index);
            });
            html += '</tr>';
        });

        html += '</tbody></table></div>';
        return html;
    }

    function renderCodeBlock(code, lang) {
        var label = String(lang || '').trim() || 'text';
        var safeLang = label.replace(/[^a-z0-9_+#.-]/gi, '');
        return '<div class="code-block">' +
            '<div class="code-header">' +
            '<span class="code-lang">' + escapeHtml(label) + '</span>' +
            '<button type="button" class="copy-btn" title="复制代码">' +
            '<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" ' +
            'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' +
            '<rect x="9" y="9" width="12" height="12" rx="2"></rect>' +
            '<path d="M5 15V5a2 2 0 0 1 2-2h10"></path></svg>' +
            '<span>复制</span></button>' +
            '</div>' +
            '<pre class="code-pre"><code class="language-' + safeLang + '">' +
            highlightCode(code, safeLang) +
            '</code></pre></div>';
    }

    function renderBlocks(source) {
        var lines = String(source == null ? '' : source).replace(/\r\n?/g, '\n').split('\n');
        var html = '';
        var i = 0;

        while (i < lines.length) {
            var line = lines[i];
            var trimmed = line.trim();

            // 空行
            if (isBlank(line)) {
                i++;
                continue;
            }

            // 代码块（流式输出时允许未闭合）
            if (/^```/.test(trimmed)) {
                var lang = trimmed.replace(/^```/, '').trim();
                var codeLines = [];
                i++;
                while (i < lines.length && !/^```/.test(lines[i].trim())) {
                    codeLines.push(lines[i]);
                    i++;
                }
                if (i < lines.length) {
                    i++; // 跳过闭合的 ```
                }
                html += renderCodeBlock(codeLines.join('\n'), lang);
                continue;
            }

            // 标题
            var heading = /^(#{1,6})\s+(.*)$/.exec(trimmed);
            if (heading) {
                var level = heading[1].length;
                html += '<h' + level + '>' + renderInline(heading[2].trim()) + '</h' + level + '>';
                i++;
                continue;
            }

            // 分割线
            if (/^(-{3,}|\*{3,}|_{3,})$/.test(trimmed)) {
                html += '<hr class="md-hr">';
                i++;
                continue;
            }

            // 表格
            if (trimmed.indexOf('|') !== -1 && i + 1 < lines.length && isTableSeparator(lines[i + 1])) {
                var headerLine = lines[i];
                var separatorLine = lines[i + 1];
                var body = [];
                i += 2;
                while (i < lines.length && !isBlank(lines[i]) && lines[i].indexOf('|') !== -1) {
                    body.push(lines[i]);
                    i++;
                }
                html += renderTable(headerLine, separatorLine, body);
                continue;
            }

            // 引用
            if (trimmed.charAt(0) === '>') {
                var quoteLines = [];
                while (i < lines.length && lines[i].trim().charAt(0) === '>') {
                    quoteLines.push(lines[i].trim().replace(/^>\s?/, ''));
                    i++;
                }
                html += '<blockquote class="md-quote">' + renderBlocks(quoteLines.join('\n')) + '</blockquote>';
                continue;
            }

            // 无序列表
            if (/^[-*+]\s+/.test(trimmed)) {
                var ulItems = [];
                while (i < lines.length && /^[-*+]\s+/.test(lines[i].trim())) {
                    ulItems.push(lines[i].trim().replace(/^[-*+]\s+/, ''));
                    i++;
                }
                html += '<ul class="md-list">' +
                    ulItems.map(function (item) {
                        return '<li>' + renderInline(item) + '</li>';
                    }).join('') + '</ul>';
                continue;
            }

            // 有序列表
            if (/^\d+[.)]\s+/.test(trimmed)) {
                var olItems = [];
                while (i < lines.length && /^\d+[.)]\s+/.test(lines[i].trim())) {
                    olItems.push(lines[i].trim().replace(/^\d+[.)]\s+/, ''));
                    i++;
                }
                html += '<ol class="md-list">' +
                    olItems.map(function (item) {
                        return '<li>' + renderInline(item) + '</li>';
                    }).join('') + '</ol>';
                continue;
            }

            // 段落：收集连续的普通文本行
            var paragraph = [];
            while (i < lines.length) {
                var current = lines[i];
                var currentTrimmed = current.trim();
                if (isBlank(current)) {
                    break;
                }
                if (/^```/.test(currentTrimmed) ||
                    /^#{1,6}\s+/.test(currentTrimmed) ||
                    /^(-{3,}|\*{3,}|_{3,})$/.test(currentTrimmed) ||
                    currentTrimmed.charAt(0) === '>' ||
                    /^[-*+]\s+/.test(currentTrimmed) ||
                    /^\d+[.)]\s+/.test(currentTrimmed)) {
                    break;
                }
                if (currentTrimmed.indexOf('|') !== -1 &&
                    i + 1 < lines.length && isTableSeparator(lines[i + 1])) {
                    break;
                }
                paragraph.push(currentTrimmed);
                i++;
            }
            if (!paragraph.length) {
                i++;
                continue;
            }
            html += '<p class="md-p">' + renderInline(paragraph.join('\n')) + '</p>';
        }

        return html;
    }

    /* ------------------------------ 对外接口 ------------------------------ */

    global.MarkdownRenderer = {
        render: function (source) {
            return renderBlocks(source);
        },
        highlight: function (code, lang) {
            return highlightCode(code, lang);
        },
        escapeHtml: escapeHtml
    };
})(window);
