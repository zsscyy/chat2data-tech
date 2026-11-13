/**
 * 登录页逻辑
 * 支持密码登录、验证码登录，以及进入页面时用本地会话ID自动恢复登录
 */
(function () {
    'use strict';

    const tabs = document.querySelectorAll('.auth-tab');
    const passwordForm = document.getElementById('password-form');
    const verifyForm = document.getElementById('verify-form');
    const passwordSubmit = document.getElementById('password-submit');
    const verifySubmit = document.getElementById('verify-submit');
    const usernameInput = document.getElementById('username');
    const passwordInput = document.getElementById('password');
    const emailInput = document.getElementById('email');
    const verifyCodeInput = document.getElementById('verify-code');
    const codeButton = document.getElementById('code-button');

    // 获取验证码接口返回的验证码ID
    let codeId = '';
    // 验证码倒计时定时器
    let countdownTimer = null;

    /**
     * 设置按钮加载状态
     * @param {HTMLButtonElement} button 按钮
     * @param {boolean} loading 是否加载中
     * @param {string} loadingText 加载中文案
     * @param {string} normalText 正常文案
     */
    function setButtonLoading(button, loading, loadingText, normalText) {
        button.disabled = loading;
        button.textContent = loading ? loadingText : normalText;
    }

    /**
     * 切换登录方式
     * @param {string} tabName 页签名称：password 或 verify
     */
    function switchTab(tabName) {
        tabs.forEach(function (tab) {
            tab.classList.toggle('active', tab.dataset.tab === tabName);
        });
        passwordForm.classList.toggle('form-hidden', tabName !== 'password');
        verifyForm.classList.toggle('form-hidden', tabName !== 'verify');
        showMessage('');
    }

    /**
     * 获取用户信息并进入首页
     * @param {string} sessionId 会话ID
     */
    async function enterHome(sessionId) {
        try {
            const response = await UserApi.getUserInfo(sessionId);
            if (isApiSuccess(response) && response.result && response.result.userInfo) {
                SessionStore.setUserInfo(response.result.userInfo);
            }
        } catch (error) {
            // 用户信息获取失败不阻塞跳转，首页会重新拉取
            console.warn('获取用户信息失败', error);
        }
        window.location.href = '/index.html';
    }

    /**
     * 处理登录成功
     * @param {string} sessionId 会话ID
     */
    async function handleLoginSuccess(sessionId) {
        SessionStore.setSessionId(sessionId);
        showMessage('登录成功，正在跳转...', 'success');
        await enterHome(sessionId);
    }

    /**
     * 启动验证码倒计时
     */
    function startCountdown() {
        let seconds = 60;
        codeButton.disabled = true;
        codeButton.textContent = seconds + '秒后重试';
        countdownTimer = setInterval(function () {
            seconds -= 1;
            if (seconds <= 0) {
                clearInterval(countdownTimer);
                countdownTimer = null;
                codeButton.disabled = false;
                codeButton.textContent = '获取验证码';
                return;
            }
            codeButton.textContent = seconds + '秒后重试';
        }, 1000);
    }

    /**
     * 停止验证码倒计时
     */
    function stopCountdown() {
        if (countdownTimer) {
            clearInterval(countdownTimer);
            countdownTimer = null;
        }
        codeButton.disabled = false;
        codeButton.textContent = '获取验证码';
    }

    // 登录方式页签切换
    tabs.forEach(function (tab) {
        tab.addEventListener('click', function () {
            switchTab(tab.dataset.tab);
        });
    });

    // 密码登录
    passwordForm.addEventListener('submit', async function (event) {
        event.preventDefault();
        const username = usernameInput.value.trim();
        const password = passwordInput.value;
        if (!username || !password) {
            showMessage('请输入昵称/邮箱和密码', 'error');
            return;
        }

        setButtonLoading(passwordSubmit, true, '登录中...', '登录');
        try {
            const response = await UserApi.passwordLogin(username, password);
            if (!isApiSuccess(response)) {
                showMessage(response.errorMsg || '登录失败，请检查账号和密码', 'error');
                return;
            }
            const sessionId = response.result ? response.result.sessionId : '';
            if (!sessionId) {
                showMessage('登录失败：未获取到会话ID', 'error');
                return;
            }
            await handleLoginSuccess(sessionId);
        } catch (error) {
            showMessage(error.message || '网络异常，请稍后重试', 'error');
        } finally {
            setButtonLoading(passwordSubmit, false, '登录中...', '登录');
        }
    });

    // 获取邮箱验证码
    codeButton.addEventListener('click', async function () {
        const email = emailInput.value.trim();
        if (!email) {
            showMessage('请先输入邮箱', 'error');
            emailInput.focus();
            return;
        }

        codeButton.disabled = true;
        codeButton.textContent = '发送中...';
        try {
            const response = await UserApi.getVerifyCode(email);
            if (!isApiSuccess(response)) {
                showMessage(response.errorMsg || '验证码发送失败', 'error');
                stopCountdown();
                return;
            }
            codeId = response.result ? response.result.codeId : '';
            if (!codeId) {
                showMessage('验证码发送失败：未获取到验证码ID', 'error');
                stopCountdown();
                return;
            }
            showMessage('验证码已发送，请查收邮箱', 'success');
            startCountdown();
        } catch (error) {
            showMessage(error.message || '网络异常，请稍后重试', 'error');
            stopCountdown();
        }
    });

    // 验证码登录
    verifyForm.addEventListener('submit', async function (event) {
        event.preventDefault();
        const email = emailInput.value.trim();
        const verifyCode = verifyCodeInput.value.trim();
        if (!email || !verifyCode) {
            showMessage('请输入邮箱和验证码', 'error');
            return;
        }
        if (!codeId) {
            showMessage('请先获取验证码', 'error');
            return;
        }

        setButtonLoading(verifySubmit, true, '登录中...', '登录');
        try {
            const response = await UserApi.verifyCodeLogin(email, verifyCode, codeId);
            if (!isApiSuccess(response)) {
                showMessage(response.errorMsg || '登录失败，请检查验证码', 'error');
                return;
            }
            const sessionId = response.result ? response.result.sessionId : '';
            if (!sessionId) {
                showMessage('登录失败：未获取到会话ID', 'error');
                return;
            }
            await handleLoginSuccess(sessionId);
        } catch (error) {
            showMessage(error.message || '网络异常，请稍后重试', 'error');
        } finally {
            setButtonLoading(verifySubmit, false, '登录中...', '登录');
        }
    });

    // 输入邮箱变化后，已获取的验证码ID失效
    emailInput.addEventListener('input', function () {
        codeId = '';
    });

    // 页面加载：展示一次性提示，并尝试用本地会话自动登录
    (async function init() {
        const flashMessage = consumeFlashMessage();
        if (flashMessage) {
            showMessage(flashMessage, 'info');
        }

        const sessionId = SessionStore.getSessionId();
        if (!sessionId) {
            return;
        }

        showMessage('正在恢复登录状态...', 'info');
        try {
            const response = await UserApi.sessionLogin(sessionId);
            if (isApiSuccess(response)) {
                await enterHome(sessionId);
                return;
            }
            SessionStore.clear();
            showMessage('登录状态已失效，请重新登录', 'info');
        } catch (error) {
            SessionStore.clear();
            showMessage('登录状态已失效，请重新登录', 'info');
        }
    })();
})();
