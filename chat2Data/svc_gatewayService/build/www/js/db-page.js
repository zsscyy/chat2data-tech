/**
 * 数据库助手页面逻辑
 * 数据库子服务未开放时仅展示结构，不发送数据库请求
 * SQLite 文件上传走已开放的文件子服务接口
 */
(function () {
    'use strict';

    if (!SessionStore.getSessionId()) {
        return;
    }

    const tabs = Array.prototype.slice.call(document.querySelectorAll('.db-tab'));
    const panes = Array.prototype.slice.call(document.querySelectorAll('[data-db-pane]'));
    const sqliteFileInput = document.getElementById('db-sqlite-file');
    const sqliteUploadButton = document.getElementById('db-sqlite-upload');
    const sqliteMessageElement = document.getElementById('db-sqlite-message');
    const sqliteResultElement = document.getElementById('db-sqlite-result');
    const sqliteFileIdElement = document.getElementById('db-sqlite-file-id');

    /**
     * 显示页面内消息
     * @param {string} text 消息内容
     * @param {string} type 消息类型
     */
    function showMessage(text, type) {
        sqliteMessageElement.textContent = text || '';
        sqliteMessageElement.className = 'message db-sqlite-message' + (text ? ' message-' + (type || 'info') : '');
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
     * 切换MySQL/SQLite页签
     * @param {string} tabName 页签名称
     */
    function switchTab(tabName) {
        tabs.forEach(function (tab) {
            const active = tab.dataset.dbTab === tabName;
            tab.classList.toggle('active', active);
            tab.setAttribute('aria-selected', active ? 'true' : 'false');
        });
        panes.forEach(function (pane) {
            pane.classList.toggle('form-hidden', pane.dataset.dbPane !== tabName);
        });
    }

    /**
     * 上传SQLite文件
     */
    async function uploadSqliteFile() {
        const file = sqliteFileInput.files && sqliteFileInput.files[0];
        if (!file) {
            showMessage('请选择 .db 格式的 SQLite 文件', 'error');
            return;
        }
        if (!/\.db$/i.test(file.name)) {
            showMessage('仅支持 .db 格式的 SQLite 文件', 'error');
            return;
        }
        if (file.size <= 0) {
            showMessage('文件大小为 0，请重新选择', 'error');
            return;
        }

        sqliteUploadButton.disabled = true;
        sqliteUploadButton.textContent = '上传中...';
        showMessage('正在上传 SQLite 文件...', 'info');
        try {
            const response = await FileApi.uploadSqliteFile(file);
            if (!isApiSuccess(response)) {
                showApiError(response);
                return;
            }
            const fileId = response.result ? response.result.fileId : '';
            if (!fileId) {
                showMessage('上传失败：未获取到文件ID', 'error');
                return;
            }
            sqliteFileIdElement.textContent = fileId;
            sqliteResultElement.classList.remove('form-hidden');
            showMessage('SQLite 文件上传成功，已刷新文件列表', 'success');
            sqliteFileInput.value = '';
            // 通知“我的文件”页面刷新列表
            window.dispatchEvent(new CustomEvent('chat2data:files-changed'));
        } catch (error) {
            showMessage(error.message || '上传失败，请稍后重试', 'error');
        } finally {
            sqliteUploadButton.disabled = false;
            sqliteUploadButton.textContent = '上传 SQLite 文件';
        }
    }

    tabs.forEach(function (tab) {
        tab.addEventListener('click', function () {
            switchTab(tab.dataset.dbTab);
        });
    });

    sqliteFileInput.addEventListener('change', function () {
        const file = sqliteFileInput.files && sqliteFileInput.files[0];
        if (file) {
            showMessage('已选择：' + file.name, 'info');
        }
    });

    sqliteUploadButton.addEventListener('click', uploadSqliteFile);

    // MySQL连接、断开、表列表、表数据与查询均依赖数据库子服务，当前不绑定真实请求
    switchTab('mysql');
})();
