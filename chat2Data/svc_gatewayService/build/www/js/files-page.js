/**
 * 我的文件页面逻辑
 * 负责文件列表加载、下载与删除
 */
(function () {
    'use strict';

    if (!SessionStore.getSessionId()) {
        return;
    }

    const tableWrap = document.getElementById('files-table-wrap');
    const countElement = document.getElementById('files-count');
    const messageElement = document.getElementById('files-message');
    const refreshButton = document.getElementById('files-refresh-button');
    const confirmMask = document.getElementById('files-confirm-mask');
    const confirmText = document.getElementById('files-confirm-text');
    const confirmCancelButton = document.getElementById('files-confirm-cancel');
    const confirmOkButton = document.getElementById('files-confirm-ok');

    const state = {
        files: []
    };
    let pendingDeleteFile = null;

    /**
     * 创建DOM元素
     * @param {string} tag 标签名
     * @param {string} className 样式类名
     * @param {string} text 文本内容
     * @returns {HTMLElement} 创建的元素
     */
    function createElement(tag, className, text) {
        const element = document.createElement(tag);
        if (className) {
            element.className = className;
        }
        if (typeof text !== 'undefined') {
            element.textContent = text;
        }
        return element;
    }

    /**
     * 显示页面内消息
     * @param {string} text 消息内容
     * @param {string} type 消息类型
     */
    function showMessage(text, type) {
        messageElement.textContent = text || '';
        messageElement.className = 'message files-message' + (text ? ' message-' + (type || 'info') : '');
    }

    /**
     * 统一处理接口错误
     * @param {object} response 接口响应
     */
    function showApiError(response) {
        if (response && response.errorCode === 501) {
            showMessage('功能待开放', 'error');
            return;
        }
        showMessage((response && response.errorMsg) || '请求失败', 'error');
    }

    /**
     * 格式化文件大小
     * @param {number} size 字节数
     * @returns {string} 格式化后的大小
     */
    function formatFileSize(size) {
        const bytes = Number(size) || 0;
        if (bytes <= 0) {
            return '0 B';
        }
        if (bytes < 1024) {
            return bytes + ' B';
        }
        if (bytes < 1024 * 1024) {
            return (bytes / 1024).toFixed(1) + ' KB';
        }
        return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    }

    /**
     * 格式化上传时间
     * @param {number} timestamp 秒或毫秒时间戳
     * @returns {string} 格式化后的时间
     */
    function formatTime(timestamp) {
        const value = Number(timestamp) || 0;
        if (value <= 0) {
            return '-';
        }
        const milliseconds = value < 1000000000000 ? value * 1000 : value;
        const date = new Date(milliseconds);
        if (isNaN(date.getTime())) {
            return '-';
        }
        return date.toLocaleString('zh-CN', {hour12: false});
    }

    /**
     * 渲染文件表格
     */
    function renderFiles() {
        tableWrap.textContent = '';
        countElement.textContent = '共 ' + state.files.length + ' 个文件';

        if (!state.files.length) {
            tableWrap.appendChild(createElement('div', 'excel-empty', '暂无文件，请先到智能 Excel 助手上传'));
            return;
        }

        const table = createElement('table', 'excel-table files-table');
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        ['文件名', '大小', '上传时间', 'chatSessionId', '操作'].forEach(function (title) {
            const th = createElement('th', '', title);
            th.scope = 'col';
            headerRow.appendChild(th);
        });
        thead.appendChild(headerRow);

        const tbody = document.createElement('tbody');
        state.files.forEach(function (file) {
            const row = document.createElement('tr');

            const nameCell = createElement('td', 'files-name', file.fileName || file.fileId || '未命名文件');
            nameCell.title = file.fileId || '';

            const actionCell = createElement('td', '');
            const actions = createElement('div', 'files-actions');

            const downloadButton = createElement('button', 'btn-mini', '下载');
            downloadButton.type = 'button';
            downloadButton.dataset.fileAction = 'download';
            downloadButton.dataset.fileId = file.fileId || '';

            const deleteButton = createElement('button', 'btn-mini btn-mini-danger', '删除');
            deleteButton.type = 'button';
            deleteButton.dataset.fileAction = 'delete';
            deleteButton.dataset.fileId = file.fileId || '';

            const mapButton = createElement('button', 'btn-mini', '关联会话');
            mapButton.type = 'button';
            mapButton.disabled = true;
            mapButton.title = 'AI 子服务待开放';

            actions.appendChild(downloadButton);
            actions.appendChild(deleteButton);
            actions.appendChild(mapButton);
            actionCell.appendChild(actions);

            row.appendChild(nameCell);
            row.appendChild(createElement('td', '', formatFileSize(file.fileSize)));
            row.appendChild(createElement('td', '', formatTime(file.uploadTime)));
            row.appendChild(createElement('td', '', file.chatSessionId || '-'));
            row.appendChild(actionCell);
            tbody.appendChild(row);
        });

        table.appendChild(thead);
        table.appendChild(tbody);
        tableWrap.appendChild(table);
    }

    /**
     * 加载文件列表
     */
    async function loadFileList() {
        try {
            const response = await FileApi.getFileList();
            if (!isApiSuccess(response)) {
                showApiError(response);
                state.files = [];
                renderFiles();
                return;
            }
            state.files = response.result && Array.isArray(response.result.fileList)
                ? response.result.fileList
                : [];
            renderFiles();
        } catch (error) {
            showMessage(error.message || '文件列表加载失败', 'error');
            state.files = [];
            renderFiles();
        }
    }

    /**
     * 获取指定ID的文件
     * @param {string} fileId 文件ID
     * @returns {object|null} 文件信息
     */
    function findFile(fileId) {
        return state.files.find(function (item) {
            return item.fileId === fileId;
        }) || null;
    }

    /**
     * 下载文件
     * @param {object} file 文件信息
     */
    async function downloadFile(file) {
        if (!file) {
            return;
        }
        try {
            const result = await FileApi.downloadFile(file.fileId);
            if (!(result instanceof Blob)) {
                showApiError(result);
                return;
            }
            const objectUrl = URL.createObjectURL(result);
            const link = document.createElement('a');
            link.href = objectUrl;
            link.download = file.fileName || 'download';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(objectUrl);
            showMessage('文件下载已开始：' + (file.fileName || file.fileId), 'success');
        } catch (error) {
            showMessage(error.message || '下载失败', 'error');
        }
    }

    /**
     * 打开删除确认弹窗
     * @param {object} file 文件信息
     */
    function openDeleteConfirm(file) {
        pendingDeleteFile = file;
        confirmText.textContent = '确定删除文件“' + (file.fileName || file.fileId) + '”吗？删除后不可恢复。';
        confirmMask.classList.remove('form-hidden');
    }

    /**
     * 关闭删除确认弹窗
     */
    function closeDeleteConfirm() {
        pendingDeleteFile = null;
        confirmMask.classList.add('form-hidden');
        confirmOkButton.disabled = false;
        confirmOkButton.textContent = '删除';
    }

    /**
     * 删除文件
     */
    async function deleteFile() {
        const file = pendingDeleteFile;
        if (!file) {
            return;
        }
        confirmOkButton.disabled = true;
        confirmOkButton.textContent = '删除中...';
        try {
            const response = await FileApi.deleteFile(file.fileId);
            if (!isApiSuccess(response)) {
                showApiError(response);
                closeDeleteConfirm();
                return;
            }
            closeDeleteConfirm();
            showMessage('文件已删除：' + (file.fileName || file.fileId), 'success');
            await loadFileList();
        } catch (error) {
            showMessage(error.message || '删除失败', 'error');
            closeDeleteConfirm();
        }
    }

    tableWrap.addEventListener('click', function (event) {
        const button = event.target.closest('[data-file-action]');
        if (!button) {
            return;
        }
        const file = findFile(button.dataset.fileId);
        if (!file) {
            return;
        }
        if (button.dataset.fileAction === 'download') {
            downloadFile(file);
        } else if (button.dataset.fileAction === 'delete') {
            openDeleteConfirm(file);
        }
    });

    refreshButton.addEventListener('click', loadFileList);
    confirmCancelButton.addEventListener('click', closeDeleteConfirm);
    confirmOkButton.addEventListener('click', deleteFile);
    confirmMask.addEventListener('click', function (event) {
        if (event.target === confirmMask) {
            closeDeleteConfirm();
        }
    });

    // SQLite上传成功后刷新文件列表
    window.addEventListener('chat2data:files-changed', loadFileList);

    // 每次进入“我的文件”页面时重新加载，避免其他页面操作后展示旧数据
    window.addEventListener('hashchange', function () {
        if (window.location.hash === '#files') {
            loadFileList();
        }
    });

    loadFileList();
})();
