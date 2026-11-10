/* ==========================================================================
   Chat2Excel 前端交互逻辑
   接口：
     GET  /api/health（兼容 /health）
     POST /api/uploadFile?fileName=xxx
     POST /api/sendMessage（SSE 流式返回）
   ========================================================================== */

(function () {
    'use strict';

    /* ------------------------------ 配置 ------------------------------ */

    // 服务器地址与端口（页面由服务器托管时使用同源地址，直接打开本地文件时使用该地址）
    var SERVER_ORIGIN = 'http://192.168.150.129:9000';
    var API_BASE = (location.protocol === 'http:' || location.protocol === 'https:') ? '' : SERVER_ORIGIN;
    var MAX_LENGTH = 2000;

    /* ------------------------------ DOM ------------------------------ */

    var els = {
        chatArea: document.getElementById('chatArea'),
        messageList: document.getElementById('messageList'),
        welcome: document.getElementById('welcome'),
        selectBtn: document.getElementById('selectFileBtn'),
        filePicker: document.getElementById('filePicker'),
        filePath: document.getElementById('filePathInput'),
        fileState: document.getElementById('fileState'),
        input: document.getElementById('messageInput'),
        sendBtn: document.getElementById('sendBtn'),
        charCount: document.getElementById('charCount'),
        statusPill: document.getElementById('statusPill'),
        statusText: document.getElementById('statusText'),
        toast: document.getElementById('toast')
    };

    /* ------------------------------ 状态 ------------------------------ */

    var state = {
        fileId: '',
        fileName: '',
        uploading: false,
        sending: false
    };

    var toastTimer = null;

    /* ------------------------------ 工具函数 ------------------------------ */

    function nowTime() {
        var date = new Date();
        function pad(num) {
            return String(num).padStart(2, '0');
        }
        return pad(date.getHours()) + ':' + pad(date.getMinutes()) + ':' + pad(date.getSeconds());
    }

    function isNearBottom() {
        var area = els.chatArea;
        return area.scrollHeight - area.scrollTop - area.clientHeight < 80;
    }

    function scrollToBottom(force) {
        if (!force && !isNearBottom()) {
            return;
        }
        els.chatArea.scrollTop = els.chatArea.scrollHeight;
    }

    function showToast(message) {
        els.toast.textContent = message;
        els.toast.classList.add('show');
        clearTimeout(toastTimer);
        toastTimer = setTimeout(function () {
            els.toast.classList.remove('show');
        }, 2400);
    }

    function setStatus(online) {
        els.statusPill.classList.toggle('online', online);
        els.statusPill.classList.toggle('offline', !online);
        els.statusText.textContent = online ? '服务正常' : '服务未连接';
    }

    function copyText(text) {
        if (navigator.clipboard && window.isSecureContext) {
            return navigator.clipboard.writeText(text);
        }
        return new Promise(function (resolve, reject) {
            var textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.top = '-9999px';
            textarea.style.opacity = '0';
            document.body.appendChild(textarea);
            textarea.select();
            try {
                var ok = document.execCommand('copy');
                ok ? resolve() : reject(new Error('copy failed'));
            } catch (err) {
                reject(err);
            } finally {
                document.body.removeChild(textarea);
            }
        });
    }

    /* ------------------------------ 消息渲染 ------------------------------ */

    function hideWelcome() {
        if (els.welcome && !els.welcome.classList.contains('is-hidden')) {
            els.welcome.classList.add('is-hidden');
        }
    }

    function renderMessage(msg) {
        var html = window.MarkdownRenderer.render(msg.text || '');
        if (msg.streaming) {
            html += '<span class="stream-cursor" aria-hidden="true"></span>';
        }
        msg.md.innerHTML = html;
    }

    function flushRender(msg) {
        if (msg.renderTimer) {
            clearTimeout(msg.renderTimer);
            msg.renderTimer = null;
        }
        renderMessage(msg);
    }

    function scheduleRender(msg) {
        if (msg.renderTimer) {
            return;
        }
        msg.renderTimer = setTimeout(function () {
            msg.renderTimer = null;
            var stick = isNearBottom();
            renderMessage(msg);
            if (stick) {
                scrollToBottom(true);
            }
        }, 70);
    }

    function setStreaming(msg, active) {
        msg.streaming = active;
        msg.el.classList.toggle('streaming', active);
        flushRender(msg);
    }

    function appendMessage(role, text) {
        hideWelcome();

        var wrap = document.createElement('div');
        wrap.className = 'message ' + (role === 'user' ? 'user' : 'ai');

        var avatar = document.createElement('div');
        avatar.className = 'avatar';
        avatar.textContent = role === 'user' ? '我' : 'AI';

        var body = document.createElement('div');
        body.className = 'msg-body';

        var bubble = document.createElement('div');
        bubble.className = 'bubble';

        var md = document.createElement('div');
        md.className = 'md-body';

        var meta = document.createElement('div');
        meta.className = 'msg-meta';
        meta.textContent = nowTime();

        bubble.appendChild(md);
        body.appendChild(bubble);
        body.appendChild(meta);
        wrap.appendChild(avatar);
        wrap.appendChild(body);
        els.messageList.appendChild(wrap);

        var msg = {
            el: wrap,
            bubble: bubble,
            md: md,
            text: '',
            streaming: false,
            renderTimer: null
        };

        msg.text = text || '';
        flushRender(msg);
        scrollToBottom(true);
        return msg;
    }

    /* ------------------------------ 文件上传 ------------------------------ */

    function setFileState(text, type) {
        els.fileState.textContent = text || '';
        els.fileState.title = text || '';
        els.fileState.className = 'file-state' + (type ? ' ' + type : '');
        els.fileState.hidden = !text;
    }

    async function uploadFile(file) {
        state.uploading = true;
        state.fileId = '';
        state.fileName = file.name;

        els.selectBtn.disabled = true;
        els.selectBtn.querySelector('span').textContent = '上传中…';
        setFileState('正在上传…', 'pending');
        updateSendState();

        try {
            var response = await fetch(API_BASE + '/api/uploadFile?fileName=' + encodeURIComponent(file.name), {
                method: 'POST',
                headers: { 'Content-Type': 'application/octet-stream' },
                body: file
            });

            var data = null;
            try {
                data = await response.json();
            } catch (parseErr) {
                data = null;
            }

            if (!response.ok || !data || data.success !== true ||
                !data.result || !data.result.fileId) {
                var reason = (data && data.error) || ('HTTP ' + response.status);
                throw new Error(reason);
            }

            state.fileId = String(data.result.fileId);
            setFileState('上传成功 · ID ' + state.fileId, 'ok');
            showToast('文件上传成功，可以开始提问了');
        } catch (err) {
            state.fileId = '';
            setFileState('上传失败：' + (err && err.message ? err.message : '未知错误'), 'error');
            showToast('文件上传失败，请重试');
        } finally {
            state.uploading = false;
            els.selectBtn.disabled = false;
            els.selectBtn.querySelector('span').textContent = '选择文件';
            updateSendState();
        }
    }

    /* ------------------------------ SSE 解析 ------------------------------ */

    function parseEventData(rawEvent) {
        var lines = rawEvent.split('\n');
        var data = [];
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            if (line.indexOf('data:') === 0) {
                var value = line.slice(5);
                if (value.charAt(0) === ' ') {
                    value = value.slice(1);
                }
                data.push(value);
            }
        }
        return data.length ? data.join('\n') : null;
    }

    function decodePayload(payload) {
        if (!payload) {
            return '';
        }
        var trimmed = payload.trim();
        if (!trimmed || trimmed === '[DONE]') {
            return '';
        }
        // 服务端每个分片是 JSON 字符串字面量，需要解析出真实文本
        if (trimmed.charAt(0) === '"') {
            try {
                var parsed = JSON.parse(trimmed);
                return typeof parsed === 'string' ? parsed : '';
            } catch (err) {
                return payload;
            }
        }
        return payload;
    }

    function drainEvents(buffer, aiMsg) {
        buffer = buffer.replace(/\r\n/g, '\n');
        var finished = false;
        var index = buffer.indexOf('\n\n');

        while (index !== -1) {
            var rawEvent = buffer.slice(0, index);
            buffer = buffer.slice(index + 2);

            var payload = parseEventData(rawEvent);
            if (payload !== null) {
                var trimmed = payload.trim();
                if (trimmed === '[DONE]') {
                    finished = true;
                    break;
                }
                var chunk = decodePayload(payload);
                if (chunk) {
                    aiMsg.text += chunk;
                    scheduleRender(aiMsg);
                }
            }

            if (finished) {
                break;
            }
            index = buffer.indexOf('\n\n');
        }

        return { rest: buffer, finished: finished };
    }

    /* ------------------------------ 发送消息 ------------------------------ */

    async function streamAnswer(question, aiMsg) {
        try {
            var response = await fetch(API_BASE + '/api/sendMessage', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ fileId: state.fileId, message: question })
            });

            if (!response.ok) {
                var errorText = '请求失败（HTTP ' + response.status + '）';
                try {
                    var errorJson = await response.json();
                    if (errorJson && errorJson.error) {
                        errorText = errorJson.error;
                    }
                } catch (ignore) {
                    /* 响应体不是 JSON 时使用默认错误信息 */
                }
                throw new Error(errorText);
            }

            if (!response.body) {
                throw new Error('当前浏览器不支持流式响应');
            }

            var reader = response.body.getReader();
            var decoder = new TextDecoder('utf-8');
            var buffer = '';
            var streamDone = false;

            while (!streamDone) {
                var readResult = await reader.read();
                if (readResult.done) {
                    break;
                }
                buffer += decoder.decode(readResult.value, { stream: true });
                var drained = drainEvents(buffer, aiMsg);
                buffer = drained.rest;
                streamDone = drained.finished;
            }

            if (streamDone) {
                try {
                    await reader.cancel();
                } catch (ignore) {
                    /* 忽略取消流时的异常 */
                }
            }

            if (!aiMsg.text) {
                aiMsg.text = '（模型未返回内容，请稍后重试）';
            }
        } catch (err) {
            aiMsg.el.classList.add('error');
            aiMsg.text = '请求失败：' + (err && err.message ? err.message : '未知错误');
            flushRender(aiMsg);
            showToast('发送消息失败');
        } finally {
            setStreaming(aiMsg, false);
            state.sending = false;
            updateSendState();
        }
    }

    function handleSend() {
        if (state.sending) {
            return;
        }

        var text = els.input.value.trim();
        if (!text) {
            return;
        }
        if (state.uploading) {
            showToast('文件正在上传，请稍候');
            return;
        }
        if (!state.fileId) {
            showToast('请先选择并上传 .xlsx 文件');
            els.selectBtn.focus();
            return;
        }

        appendMessage('user', text);
        var aiMsg = appendMessage('ai', '');
        setStreaming(aiMsg, true);

        els.input.value = '';
        autoResize();
        updateCharCount();

        state.sending = true;
        updateSendState();

        streamAnswer(text, aiMsg);
    }

    /* ------------------------------ 输入框状态 ------------------------------ */

    function autoResize() {
        els.input.style.height = 'auto';
        els.input.style.height = Math.min(els.input.scrollHeight, 168) + 'px';
    }

    function updateCharCount() {
        var length = els.input.value.length;
        els.charCount.textContent = length + ' / ' + MAX_LENGTH;
        els.charCount.classList.toggle('warn', length >= MAX_LENGTH * 0.9);
    }

    function updateSendState() {
        var hasText = els.input.value.trim().length > 0;
        els.sendBtn.disabled = state.sending || !hasText;
    }

    /* ------------------------------ 事件绑定 ------------------------------ */

    els.selectBtn.addEventListener('click', function () {
        if (state.uploading) {
            return;
        }
        els.filePicker.click();
    });

    els.filePicker.addEventListener('change', function () {
        var file = els.filePicker.files && els.filePicker.files[0];
        els.filePicker.value = '';
        if (!file) {
            return;
        }
        if (!/\.xlsx$/i.test(file.name)) {
            showToast('仅支持 .xlsx 格式的 Excel 文件');
            return;
        }

        els.filePath.value = file.name;
        els.filePath.title = file.name;
        uploadFile(file);
    });

    els.input.addEventListener('input', function () {
        autoResize();
        updateCharCount();
        updateSendState();
    });

    els.input.addEventListener('keydown', function (event) {
        if (event.key === 'Enter' && !event.shiftKey) {
            if (event.isComposing || event.keyCode === 229) {
                return;
            }
            event.preventDefault();
            handleSend();
        }
    });

    els.sendBtn.addEventListener('click', handleSend);

    // 代码块一键复制（事件委托，流式重绘后仍然有效）
    els.messageList.addEventListener('click', function (event) {
        var target = event.target;
        var button = target && target.closest ? target.closest('.copy-btn') : null;
        if (!button || !els.messageList.contains(button)) {
            return;
        }

        var block = button.closest('.code-block');
        var codeElement = block ? block.querySelector('code') : null;
        if (!codeElement) {
            return;
        }

        copyText(codeElement.textContent || '').then(function () {
            var label = button.querySelector('span');
            button.classList.add('copied');
            if (label) {
                label.textContent = '已复制';
            }
            setTimeout(function () {
                button.classList.remove('copied');
                if (label) {
                    label.textContent = '复制';
                }
            }, 1600);
        }).catch(function () {
            showToast('复制失败，请手动选择复制');
        });
    });

    // 示例问题快捷填充
    var chips = document.querySelectorAll('.chip');
    Array.prototype.forEach.call(chips, function (chip) {
        chip.addEventListener('click', function () {
            els.input.value = chip.getAttribute('data-q') || chip.textContent.trim();
            autoResize();
            updateCharCount();
            updateSendState();
            els.input.focus();
        });
    });

    /* ------------------------------ 健康检查 ------------------------------ */

    async function checkHealth() {
        var endpoints = ['/api/health', '/health'];
        for (var i = 0; i < endpoints.length; i++) {
            try {
                var response = await fetch(API_BASE + endpoints[i], { cache: 'no-store' });
                if (response.ok) {
                    var data = await response.json();
                    if (data && data.status === 'healthy') {
                        setStatus(true);
                        return;
                    }
                }
            } catch (ignore) {
                /* 尝试下一个地址 */
            }
        }
        setStatus(false);
    }

    /* ------------------------------ 初始化 ------------------------------ */

    function init() {
        autoResize();
        updateCharCount();
        updateSendState();
        checkHealth();
    }

    init();
})();
