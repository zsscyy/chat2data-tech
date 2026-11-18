/**
 * 智能 Excel 助手页面逻辑
 * 负责文件上传、文件列表、Excel 预览、下载与删除
 * AI 会话与自然语言操作依赖 AI 子服务，当前仅展示待开放状态
 */
(function () {
    'use strict';

    if (!SessionStore.getSessionId()) {
        return;
    }

    // 页面状态
    const state = {
        files: [],
        selectedFile: null,
        localFile: null,
        preview: null,
        activeSheetIndex: 0,
        pageSize: 50
    };

    // 待删除文件
    let pendingDeleteFile = null;

    const messageElement = document.getElementById('excel-message');
    const fileInput = document.getElementById('excel-file-input');
    const uploadTriggerButton = document.getElementById('excel-upload-trigger');
    const refreshButton = document.getElementById('excel-refresh-button');
    const localFileElement = document.getElementById('excel-local-file');
    const localFileNameElement = document.getElementById('excel-local-file-name');
    const localFileMetaElement = document.getElementById('excel-local-file-meta');
    const uploadButton = document.getElementById('excel-upload-button');
    const clearFileButton = document.getElementById('excel-clear-file-button');
    const currentFileNameElement = document.getElementById('excel-current-file-name');
    const fileListElement = document.getElementById('excel-file-list');
    const fileCountElement = document.getElementById('excel-file-count');
    const fileInfoElement = document.getElementById('excel-file-info');
    const infoNameElement = document.getElementById('excel-info-name');
    const infoSizeElement = document.getElementById('excel-info-size');
    const infoTimeElement = document.getElementById('excel-info-time');
    const infoChatElement = document.getElementById('excel-info-chat');
    const downloadSelectedButton = document.getElementById('excel-download-selected');
    const deleteSelectedButton = document.getElementById('excel-delete-selected');
    const sheetTabsElement = document.getElementById('excel-sheet-tabs');
    const tableWrapElement = document.getElementById('excel-table-wrap');
    const previewFooterElement = document.getElementById('excel-preview-footer');
    const previousPageButton = document.getElementById('excel-prev-page');
    const nextPageButton = document.getElementById('excel-next-page');
    const pageIndicatorElement = document.getElementById('excel-page-indicator');
    const pageSizeSelect = document.getElementById('excel-page-size');
    const previewHintElement = document.getElementById('excel-preview-hint');
    const confirmMask = document.getElementById('excel-confirm-mask');
    const confirmText = document.getElementById('excel-confirm-text');
    const confirmCancelButton = document.getElementById('excel-confirm-cancel');
    const confirmOkButton = document.getElementById('excel-confirm-ok');

    // 预览无数据时的说明
    const EMPTY_DATA_HINT = '文件已解析出工作表，数据入库/取数依赖数据库子服务，当前为空。';

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
     * @param {string} type 消息类型：success、error、info
     */
    function showPageMessage(text, type) {
        messageElement.textContent = text || '';
        messageElement.className = 'message excel-message' + (text ? ' message-' + (type || 'info') : '');
    }

    /**
     * 统一处理接口错误
     * @param {object} response 接口响应
     */
    function showApiError(response) {
        if (response && response.errorCode === 501) {
            showPageMessage('功能待开放', 'error');
            return;
        }
        showPageMessage((response && response.errorMsg) || '请求失败', 'error');
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
        // 兼容秒级与毫秒级时间戳
        const milliseconds = value < 1000000000000 ? value * 1000 : value;
        const date = new Date(milliseconds);
        if (isNaN(date.getTime())) {
            return '-';
        }
        return date.toLocaleString('zh-CN', {hour12: false});
    }

    /**
     * 校验本地选择的Excel文件
     * @param {File} file 文件对象
     * @returns {string} 校验失败返回错误提示，通过返回空字符串
     */
    function validateExcelFile(file) {
        if (!file) {
            return '请选择要上传的 Excel 文件';
        }
        if (!/\.xlsx$/i.test(file.name)) {
            return '仅支持 .xlsx 格式的 Excel 文件';
        }
        if (file.size <= 0) {
            return '文件大小为 0，请重新选择';
        }
        return '';
    }

    /**
     * 清空本地选择的文件
     */
    function clearLocalFile() {
        state.localFile = null;
        fileInput.value = '';
        localFileElement.classList.add('form-hidden');
        localFileNameElement.textContent = '-';
        localFileMetaElement.textContent = '-';
    }

    /**
     * 更新当前文件显示
     */
    function updateCurrentFileDisplay() {
        const file = state.selectedFile;
        currentFileNameElement.textContent = file ? (file.fileName || file.fileId) : '未选择文件';
        downloadSelectedButton.disabled = !file;
        deleteSelectedButton.disabled = !file;
    }

    /**
     * 显示选中文件的信息
     * @param {object|null} file 文件信息
     */
    function renderFileInfo(file) {
        if (!file) {
            fileInfoElement.classList.add('form-hidden');
            return;
        }
        infoNameElement.textContent = file.fileName || '-';
        infoSizeElement.textContent = formatFileSize(file.fileSize);
        infoTimeElement.textContent = formatTime(file.uploadTime);
        infoChatElement.textContent = file.chatSessionId || '未关联';
        fileInfoElement.classList.remove('form-hidden');
    }

    /**
     * 渲染文件列表
     */
    function renderFileList() {
        fileListElement.textContent = '';
        fileCountElement.textContent = '共 ' + state.files.length + ' 个文件';

        if (!state.files.length) {
            fileListElement.appendChild(createElement('div', 'excel-empty', '暂无文件，点击“上传 Excel”添加'));
            return;
        }

        state.files.forEach(function (file) {
            const row = createElement('div', 'excel-file-row');
            row.dataset.fileId = file.fileId || '';
            if (state.selectedFile && state.selectedFile.fileId === file.fileId) {
                row.classList.add('is-selected');
            }

            const main = createElement('div', 'excel-file-row-main');
            main.appendChild(createElement('div', 'excel-file-name', file.fileName || file.fileId || '未命名文件'));
            const meta = createElement('div', 'excel-file-meta');
            meta.appendChild(createElement('span', '', '大小：' + formatFileSize(file.fileSize)));
            meta.appendChild(createElement('span', '', '上传时间：' + formatTime(file.uploadTime)));
            meta.appendChild(createElement('span', '', '会话：' + (file.chatSessionId || '未关联')));
            main.appendChild(meta);

            const actions = createElement('div', 'excel-file-row-actions');
            const downloadButton = createElement('button', 'btn-mini', '下载');
            downloadButton.type = 'button';
            downloadButton.dataset.fileAction = 'download';
            const deleteButton = createElement('button', 'btn-mini btn-mini-danger', '删除');
            deleteButton.type = 'button';
            deleteButton.dataset.fileAction = 'delete';
            actions.appendChild(downloadButton);
            actions.appendChild(deleteButton);

            row.appendChild(main);
            row.appendChild(actions);
            fileListElement.appendChild(row);
        });
    }

    /**
     * 加载文件列表
     */
    async function loadFileList() {
        try {
            const response = await FileApi.getFileList();
            if (!isApiSuccess(response)) {
                showApiError(response);
                fileListElement.textContent = '';
                fileListElement.appendChild(createElement('div', 'excel-empty', '文件列表加载失败'));
                return;
            }
            const fileList = response.result && Array.isArray(response.result.fileList)
                ? response.result.fileList
                : [];
            state.files = fileList;
            renderFileList();
        } catch (error) {
            showPageMessage(error.message || '文件列表加载失败', 'error');
            fileListElement.textContent = '';
            fileListElement.appendChild(createElement('div', 'excel-empty', '文件列表加载失败'));
        }
    }

    /**
     * 渲染预览占位内容
     * @param {string} text 占位提示
     */
    function renderPreviewPlaceholder(text) {
        sheetTabsElement.textContent = '';
        tableWrapElement.textContent = '';
        tableWrapElement.appendChild(createElement('div', 'excel-empty', text));
        previewFooterElement.classList.add('form-hidden');
        previewHintElement.classList.add('form-hidden');
    }

    /**
     * 渲染sheet标签页
     * @param {Array} sheets 工作表数组
     */
    function renderSheetTabs(sheets) {
        sheetTabsElement.textContent = '';
        sheets.forEach(function (sheet, index) {
            const tab = createElement('button', 'sheet-tab', sheet.name || ('Sheet' + (index + 1)));
            tab.type = 'button';
            if (index === state.activeSheetIndex) {
                tab.classList.add('active');
            }
            tab.addEventListener('click', function () {
                state.activeSheetIndex = index;
                renderPreview();
            });
            sheetTabsElement.appendChild(tab);
        });
    }

    /**
     * 渲染无数据提示
     */
    function renderEmptyTable() {
        tableWrapElement.textContent = '';
        const emptyBox = createElement('div', 'excel-table-empty');
        emptyBox.appendChild(createElement('strong', '', '暂无数据'));
        emptyBox.appendChild(createElement('p', '', EMPTY_DATA_HINT));
        tableWrapElement.appendChild(emptyBox);
        previewHintElement.textContent = EMPTY_DATA_HINT;
        previewHintElement.classList.remove('form-hidden');
    }

    /**
     * 渲染表格
     * @param {object} sheet 工作表数据
     */
    function renderTable(sheet) {
        const data = Array.isArray(sheet.data) ? sheet.data : [];
        let columns = Array.isArray(sheet.columns) ? sheet.columns.slice() : [];

        if (!data.length) {
            renderEmptyTable();
            return;
        }

        // 后端未返回列名时，按行数据的最大列数生成占位列名
        if (!columns.length) {
            const maxColumnCount = data.reduce(function (max, row) {
                return Math.max(max, Array.isArray(row) ? row.length : 0);
            }, 0);
            for (let i = 0; i < maxColumnCount; i += 1) {
                columns.push('列' + (i + 1));
            }
        }

        tableWrapElement.textContent = '';
        const table = createElement('table', 'excel-table');
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        columns.forEach(function (column) {
            headerRow.appendChild(createElement('th', '', column));
        });
        thead.appendChild(headerRow);

        const tbody = document.createElement('tbody');
        data.forEach(function (row) {
            const cells = Array.isArray(row) ? row : [];
            const tableRow = document.createElement('tr');
            for (let i = 0; i < columns.length; i += 1) {
                const cellValue = typeof cells[i] === 'undefined' || cells[i] === null ? '' : cells[i];
                tableRow.appendChild(createElement('td', '', String(cellValue)));
            }
            tbody.appendChild(tableRow);
        });

        table.appendChild(thead);
        table.appendChild(tbody);
        tableWrapElement.appendChild(table);

        previewHintElement.classList.add('form-hidden');
    }

    /**
     * 渲染分页信息
     * @param {object} sheet 工作表数据
     */
    function renderPagination(sheet) {
        const currentPage = Number(sheet.currentPage) || 1;
        const totalPages = Number(sheet.totalPages) || 0;
        const totalRows = Number(sheet.totalRows) || 0;

        if (totalPages <= 0) {
            previewFooterElement.classList.add('form-hidden');
            return;
        }

        pageIndicatorElement.textContent = '第 ' + currentPage + ' / ' + totalPages + ' 页 · 共 ' + totalRows + ' 行';
        previousPageButton.disabled = currentPage <= 1;
        nextPageButton.disabled = currentPage >= totalPages;
        previewFooterElement.classList.remove('form-hidden');
    }

    /**
     * 获取当前工作表
     * @returns {object|null} 当前工作表
     */
    function getActiveSheet() {
        if (!state.preview || !Array.isArray(state.preview.sheets)) {
            return null;
        }
        return state.preview.sheets[state.activeSheetIndex] || null;
    }

    /**
     * 渲染预览区域
     */
    function renderPreview() {
        if (!state.preview) {
            renderPreviewPlaceholder('请选择文件查看预览');
            return;
        }

        renderFileInfo(state.selectedFile);

        const sheets = state.preview.sheets || [];
        if (!sheets.length) {
            renderSheetTabs([]);
            renderEmptyTable();
            previewFooterElement.classList.add('form-hidden');
            return;
        }

        if (state.activeSheetIndex >= sheets.length) {
            state.activeSheetIndex = 0;
        }
        renderSheetTabs(sheets);
        const sheet = getActiveSheet();
        renderTable(sheet);
        renderPagination(sheet);
    }

    /**
     * 加载Excel预览
     * @param {string} fileId 文件ID
     * @param {number} pageNumber 页码
     * @param {number} pageSize 每页行数
     * @param {number} sheetIndex 需要保持选中的工作表下标
     */
    async function loadPreview(fileId, pageNumber, pageSize, sheetIndex) {
        sheetTabsElement.textContent = '';
        tableWrapElement.textContent = '';
        tableWrapElement.appendChild(createElement('div', 'excel-empty', '正在加载预览...'));
        previewFooterElement.classList.add('form-hidden');
        previewHintElement.classList.add('form-hidden');

        try {
            const response = await FileApi.previewExcel(fileId, pageNumber, pageSize);
            if (!isApiSuccess(response)) {
                state.preview = null;
                renderPreviewPlaceholder('预览加载失败');
                showApiError(response);
                return;
            }
            const result = response.result || {};
            const sheets = result.excelData && Array.isArray(result.excelData.sheets)
                ? result.excelData.sheets
                : [];
            state.preview = {
                fileId: result.fileId || fileId,
                sheets: sheets
            };
            state.activeSheetIndex = Math.min(sheetIndex || 0, Math.max(sheets.length - 1, 0));
            renderPreview();
        } catch (error) {
            state.preview = null;
            renderPreviewPlaceholder('预览加载失败');
            showPageMessage(error.message || '预览加载失败', 'error');
        }
    }

    /**
     * 选中文件并加载预览
     * @param {object} file 文件信息
     */
    function selectFile(file) {
        state.selectedFile = file;
        updateCurrentFileDisplay();
        renderFileList();
        renderFileInfo(file);
        loadPreview(file.fileId, 1, state.pageSize, 0);
    }

    /**
     * 清空预览区域
     * @param {string} text 占位提示
     */
    function clearPreview(text) {
        state.preview = null;
        state.activeSheetIndex = 0;
        renderFileInfo(null);
        renderPreviewPlaceholder(text || '请选择文件查看预览');
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
            link.download = file.fileName || 'download.xlsx';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(objectUrl);
            showPageMessage('文件下载已开始：' + (file.fileName || file.fileId), 'success');
        } catch (error) {
            showPageMessage(error.message || '下载失败', 'error');
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
            showPageMessage('文件已删除：' + (file.fileName || file.fileId), 'success');
            if (state.selectedFile && state.selectedFile.fileId === file.fileId) {
                state.selectedFile = null;
                updateCurrentFileDisplay();
                clearPreview('文件已删除，请选择其他文件');
            }
            await loadFileList();
        } catch (error) {
            showPageMessage(error.message || '删除失败', 'error');
            closeDeleteConfirm();
        }
    }

    /**
     * 处理本地文件选择
     */
    function handleFileSelected() {
        const file = fileInput.files && fileInput.files[0];
        const errorMessage = validateExcelFile(file);
        if (errorMessage) {
            showPageMessage(errorMessage, 'error');
            clearLocalFile();
            return;
        }
        state.localFile = file;
        localFileNameElement.textContent = file.name;
        localFileMetaElement.textContent = formatFileSize(file.size) + ' · .xlsx';
        localFileElement.classList.remove('form-hidden');
        showPageMessage('已选择文件，点击“上传”开始上传', 'info');
    }

    /**
     * 设置上传中的按钮状态
     * @param {boolean} uploading 是否上传中
     */
    function setUploading(uploading) {
        uploadTriggerButton.disabled = uploading;
        refreshButton.disabled = uploading;
        uploadButton.disabled = uploading;
        clearFileButton.disabled = uploading;
        uploadButton.textContent = uploading ? '上传中...' : '上传';
    }

    /**
     * 上传文件
     */
    /**
     * 上传内容失败时清理刚创建的文件元数据，避免列表中出现“有记录无内容”的坏文件
     * @param {string} fileId 文件ID
     */
    async function cleanupUploadedFile(fileId) {
        if (!fileId) {
            return;
        }
        try {
            await FileApi.deleteFile(fileId);
        } catch (error) {
            console.warn('清理上传失败的文件元数据失败', fileId, error);
        }
    }

    async function uploadFile() {
        const file = state.localFile;
        const errorMessage = validateExcelFile(file);
        if (errorMessage) {
            showPageMessage(errorMessage, 'error');
            return;
        }

        setUploading(true);
        let uploadedFileId = '';
        let contentUploaded = false;
        try {
            showPageMessage('正在上传文件信息...', 'info');
            const infoResponse = await FileApi.uploadFileInfo({
                filename: file.name,
                fileSize: file.size,
                fileExt: '.xlsx'
            });
            if (!isApiSuccess(infoResponse)) {
                showApiError(infoResponse);
                return;
            }
            uploadedFileId = infoResponse.result ? infoResponse.result.fileId : '';
            if (!uploadedFileId) {
                showPageMessage('上传失败：未获取到文件ID', 'error');
                return;
            }

            showPageMessage('正在上传文件数据...', 'info');
            const uploadResponse = await FileApi.uploadFile(file, uploadedFileId);
            if (!isApiSuccess(uploadResponse)) {
                await cleanupUploadedFile(uploadedFileId);
                uploadedFileId = '';
                showApiError(uploadResponse);
                return;
            }
            contentUploaded = true;

            const uploadedFile = {
                fileId: uploadedFileId,
                fileName: file.name,
                fileSize: file.size,
                uploadTime: Math.floor(Date.now() / 1000),
                chatSessionId: ''
            };
            clearLocalFile();
            await loadFileList();

            const targetFile = state.files.find(function (item) {
                return item.fileId === uploadedFileId;
            }) || uploadedFile;

            showPageMessage('文件上传成功，正在加载预览...', 'success');
            // 上传后按约定使用第一页、每页50行进行预览
            state.pageSize = 50;
            pageSizeSelect.value = '50';
            selectFile(targetFile);
        } catch (error) {
            if (!contentUploaded) {
                await cleanupUploadedFile(uploadedFileId);
            }
            if (contentUploaded) {
                showPageMessage('文件已上传，但刷新列表或预览失败：' + (error.message || '请稍后重试'), 'error');
            } else {
                showPageMessage(error.message || '上传失败，请稍后重试', 'error');
            }
        } finally {
            setUploading(false);
        }
    }

    // 打开文件选择框
    uploadTriggerButton.addEventListener('click', function () {
        fileInput.click();
    });

    // 文件选择变化
    fileInput.addEventListener('change', handleFileSelected);

    // 上传选中的本地文件
    uploadButton.addEventListener('click', uploadFile);

    // 取消本地选择
    clearFileButton.addEventListener('click', function () {
        clearLocalFile();
        showPageMessage('', 'info');
    });

    // 刷新文件列表
    refreshButton.addEventListener('click', loadFileList);

    // 文件列表点击：选中文件或执行行内操作
    fileListElement.addEventListener('click', function (event) {
        const row = event.target.closest('.excel-file-row');
        if (!row) {
            return;
        }
        const file = state.files.find(function (item) {
            return item.fileId === row.dataset.fileId;
        });
        if (!file) {
            return;
        }

        const actionButton = event.target.closest('[data-file-action]');
        if (actionButton) {
            event.stopPropagation();
            if (actionButton.dataset.fileAction === 'download') {
                downloadFile(file);
            } else if (actionButton.dataset.fileAction === 'delete') {
                openDeleteConfirm(file);
            }
            return;
        }
        selectFile(file);
    });

    // 预览区下载与删除按钮
    downloadSelectedButton.addEventListener('click', function () {
        downloadFile(state.selectedFile);
    });
    deleteSelectedButton.addEventListener('click', function () {
        if (state.selectedFile) {
            openDeleteConfirm(state.selectedFile);
        }
    });

    // 上一页、下一页
    previousPageButton.addEventListener('click', function () {
        const sheet = getActiveSheet();
        const currentPage = sheet ? Number(sheet.currentPage) || 1 : 1;
        if (currentPage <= 1) {
            return;
        }
        loadPreview(state.preview.fileId, currentPage - 1, state.pageSize, state.activeSheetIndex);
    });

    nextPageButton.addEventListener('click', function () {
        const sheet = getActiveSheet();
        if (!sheet) {
            return;
        }
        const currentPage = Number(sheet.currentPage) || 1;
        const totalPages = Number(sheet.totalPages) || 1;
        if (currentPage >= totalPages) {
            return;
        }
        loadPreview(state.preview.fileId, currentPage + 1, state.pageSize, state.activeSheetIndex);
    });

    // 每页条数变化，回到第一页重新加载
    pageSizeSelect.addEventListener('change', function () {
        state.pageSize = Number(pageSizeSelect.value) || 50;
        if (state.preview) {
            loadPreview(state.preview.fileId, 1, state.pageSize, state.activeSheetIndex);
        }
    });

    // 删除确认弹窗
    confirmCancelButton.addEventListener('click', closeDeleteConfirm);
    confirmOkButton.addEventListener('click', deleteFile);
    confirmMask.addEventListener('click', function (event) {
        if (event.target === confirmMask) {
            closeDeleteConfirm();
        }
    });

    // 初始化
    updateCurrentFileDisplay();
    clearPreview('请选择文件查看预览');
    loadFileList();
})();
