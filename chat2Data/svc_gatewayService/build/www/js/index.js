/**
 * 首页逻辑
 * 校验本地会话，拉取并展示用户信息，支持退出登录
 */
(function () {
    'use strict';

    const sessionId = SessionStore.getSessionId();
    if (!sessionId) {
        redirectToLogin('请先登录');
        return;
    }

    const pageLoading = document.getElementById('page-loading');
    const pageContent = document.getElementById('page-content');
    const logoutButton = document.getElementById('logout-button');
    const headerNickname = document.getElementById('header-nickname');
    const userAvatar = document.getElementById('user-avatar');
    const userIdElement = document.getElementById('user-id');
    const userNicknameElement = document.getElementById('user-nickname');
    const userEmailElement = document.getElementById('user-email');
    const welcomeTitle = document.getElementById('welcome-title');

    /**
     * 渲染用户信息
     * @param {object} userInfo 用户信息
     */
    function renderUserInfo(userInfo) {
        const nickname = userInfo.nickname || '';
        headerNickname.textContent = nickname || '未命名用户';
        userAvatar.textContent = nickname ? nickname.charAt(0).toUpperCase() : 'U';
        welcomeTitle.textContent = nickname ? '欢迎回来，' + nickname : '欢迎回来';
        userIdElement.textContent = userInfo.userId || '-';
        userNicknameElement.textContent = nickname || '-';
        userEmailElement.textContent = userInfo.email || '-';

        pageLoading.classList.add('form-hidden');
        pageContent.classList.remove('form-hidden');
    }

    /**
     * 加载用户信息
     */
    async function loadUserInfo() {
        try {
            const response = await UserApi.getUserInfo(sessionId);
            if (!isApiSuccess(response)) {
                SessionStore.clear();
                redirectToLogin(response.errorMsg || '登录状态已失效，请重新登录');
                return;
            }
            const userInfo = response.result ? response.result.userInfo : null;
            if (!userInfo) {
                SessionStore.clear();
                redirectToLogin('获取用户信息失败，请重新登录');
                return;
            }
            SessionStore.setUserInfo(userInfo);
            renderUserInfo(userInfo);
        } catch (error) {
            // 网络异常时优先展示本地缓存的用户信息
            const cachedUserInfo = SessionStore.getUserInfo();
            if (cachedUserInfo) {
                renderUserInfo(cachedUserInfo);
                showMessage('网络异常，当前展示的是本地缓存信息', 'error');
                return;
            }
            pageLoading.classList.add('form-hidden');
            pageContent.classList.remove('form-hidden');
            showMessage(error.message || '获取用户信息失败，请刷新页面重试', 'error');
        }
    }

    // 退出登录
    logoutButton.addEventListener('click', async function () {
        logoutButton.disabled = true;
        logoutButton.textContent = '退出中...';
        try {
            const response = await UserApi.logout(sessionId);
            if (!isApiSuccess(response)) {
                showMessage(response.errorMsg || '退出登录失败', 'error');
                return;
            }
        } catch (error) {
            // 会话已失效或网络异常时，仍然清除本地登录态
            console.warn('退出登录请求失败', error);
        } finally {
            SessionStore.clear();
            window.location.href = '/login.html';
        }
    });

    const flashMessage = consumeFlashMessage();
    if (flashMessage) {
        showMessage(flashMessage, 'info');
    }

    loadUserInfo();
})();
