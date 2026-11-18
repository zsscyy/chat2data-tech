/**
 * 应用主逻辑
 * 负责登录校验、用户信息加载、导航切换、hash路由与退出登录
 */
(function () {
    'use strict';

    // 支持的视图名称，顺序与导航一致
    const VIEW_NAMES = ['excel', 'database', 'files', 'history', 'profile'];
    // 默认视图
    const DEFAULT_VIEW = 'excel';

    const sessionId = SessionStore.getSessionId();
    if (!sessionId) {
        redirectToLogin('请先登录');
        return;
    }

    const loadingElement = document.getElementById('app-loading');
    const shellElement = document.getElementById('app-shell');
    const logoutButton = document.getElementById('logout-button');
    const navItems = Array.prototype.slice.call(document.querySelectorAll('.nav-item'));
    const viewSections = Array.prototype.slice.call(document.querySelectorAll('.view-section'));

    /**
     * 从URL hash解析视图名称
     * @returns {string} 合法视图名称，非法或为空时返回默认视图
     */
    function resolveViewFromHash() {
        const hash = (window.location.hash || '').replace(/^#/, '');
        return VIEW_NAMES.indexOf(hash) === -1 ? DEFAULT_VIEW : hash;
    }

    /**
     * 渲染指定视图
     * @param {string} viewName 视图名称
     */
    function renderView(viewName) {
        navItems.forEach(function (item) {
            item.classList.toggle('active', item.dataset.view === viewName);
        });
        viewSections.forEach(function (section) {
            section.classList.toggle('is-active', section.dataset.view === viewName);
        });
    }

    /**
     * 应用hash路由
     * 非法或空hash归一到默认视图，使用replaceState避免产生多余历史记录
     */
    function applyHashRoute() {
        const viewName = resolveViewFromHash();
        if (window.location.hash !== '#' + viewName) {
            window.history.replaceState(null, '', '#' + viewName);
        }
        renderView(viewName);
    }

    /**
     * 渲染用户信息
     * @param {object} userInfo 用户信息
     */
    function renderUserInfo(userInfo) {
        const nickname = userInfo.nickname || '未命名用户';
        document.getElementById('header-nickname').textContent = nickname;
        document.getElementById('user-avatar').textContent = nickname.charAt(0).toUpperCase();
        document.getElementById('profile-user-id').textContent = userInfo.userId || '-';
        document.getElementById('profile-nickname').textContent = nickname;
        document.getElementById('profile-email').textContent = userInfo.email || '-';
    }

    /**
     * 加载用户信息，成功后展示应用主界面
     * 加载失败时清空登录态并返回登录页
     */
    async function loadUserInfo() {
        try {
            const response = await UserApi.getUserInfo(sessionId);
            const userInfo = response.result ? response.result.userInfo : null;
            if (!isApiSuccess(response) || !userInfo) {
                SessionStore.clear();
                redirectToLogin(response.errorMsg || '登录状态已失效，请重新登录');
                return;
            }
            SessionStore.setUserInfo(userInfo);
            renderUserInfo(userInfo);
            loadingElement.classList.add('form-hidden');
            shellElement.classList.remove('form-hidden');
            applyHashRoute();
        } catch (error) {
            SessionStore.clear();
            redirectToLogin(error.message || '获取用户信息失败，请重新登录');
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

    // 导航点击直接通过链接hash触发路由切换
    window.addEventListener('hashchange', applyHashRoute);

    // 页面内跳转按钮，例如“前往我的文件”
    document.addEventListener('click', function (event) {
        const target = event.target.closest('[data-goto]');
        if (!target) {
            return;
        }
        const viewName = target.dataset.goto;
        if (VIEW_NAMES.indexOf(viewName) !== -1) {
            window.location.hash = '#' + viewName;
        }
    });

    logoutButton.addEventListener('click', handleLogout);

    const flashMessage = consumeFlashMessage();
    if (flashMessage) {
        showMessage(flashMessage, 'info');
    }

    // 先按hash渲染一次，避免用户信息加载期间展示错误视图
    applyHashRoute();
    loadUserInfo();
})();
