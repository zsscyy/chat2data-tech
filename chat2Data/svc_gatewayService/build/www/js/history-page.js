/**
 * 历史会话页面逻辑
 * AI 子服务未开放时仅展示空状态，不请求历史会话接口
 */
(function () {
    'use strict';

    const titleElement = document.getElementById('history-empty-title');
    const textElement = document.getElementById('history-empty-text');

    const aiServiceEnabled = !!(window.APP_FEATURES && window.APP_FEATURES.AI_SERVICE);
    if (!aiServiceEnabled) {
        titleElement.textContent = 'AI 子服务尚未实现，历史会话暂不可用';
        textElement.textContent = 'AI 子服务接入后可在此查看历史会话。';
        return;
    }

    // AI 子服务已开启但历史会话接口尚未接入，仍不发送请求
    titleElement.textContent = '历史会话接口尚未接入';
    textElement.textContent = 'AI 子服务已开启，历史会话接口将在后续版本接入。';
})();
