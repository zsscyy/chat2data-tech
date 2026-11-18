/**
 * 个人中心页面逻辑
 * 展示当前登录用户信息，并提供退出登录按钮
 */
(function () {
    'use strict';

    const sessionId = SessionStore.getSessionId();
    if (!sessionId) {
        return;
    }

    const userIdElement = document.getElementById('profile-user-id');
    const nicknameElement = document.getElementById('profile-nickname');
    const emailElement = document.getElementById('profile-email');
    const logoutButton = document.getElementById('profile-logout-button');

    /**
     * 渲染用户信息
     * @param {object} userInfo 用户信息
     */
    function renderUserInfo(userInfo) {
        if (!userInfo) {
            return;
        }
        userIdElement.textContent = userInfo.userId || '-';
        nicknameElement.textContent = userInfo.nickname || '-';
        emailElement.textContent = userInfo.email || '-';
    }

    /**
     * 加载用户信息
     * 优先使用本地缓存，再通过用户子服务刷新
     */
    async function loadUserInfo() {
        const cachedUserInfo = SessionStore.getUserInfo();
        if (cachedUserInfo) {
            renderUserInfo(cachedUserInfo);
        }

        try {
            const response = await UserApi.getUserInfo(sessionId);
            const userInfo = response.result ? response.result.userInfo : null;
            if (isApiSuccess(response) && userInfo) {
                SessionStore.setUserInfo(userInfo);
                renderUserInfo(userInfo);
            }
        } catch (error) {
            // 用户信息加载失败时保留缓存展示，登录态由应用主逻辑统一处理
            console.warn('个人中心加载用户信息失败', error);
        }
    }

    /**
     * 退出登录
     * 无论请求成功或失败，都清空本地登录态并返回登录页
     */
    async function handleLogout() {
        logoutButton.disabled = true;
        logoutButton.textContent = '退出中...';
        try {
            await UserApi.logout(sessionId);
        } catch (error) {
            console.warn('退出登录请求失败', error);
        } finally {
            SessionStore.clear();
            window.location.href = '/login.html';
        }
    }

    logoutButton.addEventListener('click', handleLogout);
    loadUserInfo();
})();
