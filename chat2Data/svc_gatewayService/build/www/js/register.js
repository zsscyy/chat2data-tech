/**
 * 注册页逻辑
 * 输入过程中检测昵称、邮箱唯一性，校验通过后提交注册
 */
(function () {
    'use strict';

    // 唯一性检测状态：null 未检测，true 可用，false 不可用
    let nicknameAvailable = null;
    let emailAvailable = null;
    let nicknameTimer = null;
    let emailTimer = null;

    const registerForm = document.getElementById('register-form');
    const registerSubmit = document.getElementById('register-submit');
    const nicknameInput = document.getElementById('nickname');
    const emailInput = document.getElementById('email');
    const passwordInput = document.getElementById('password');
    const confirmPasswordInput = document.getElementById('confirm-password');
    const nicknameHint = document.getElementById('nickname-hint');
    const emailHint = document.getElementById('email-hint');

    /**
     * 设置输入框下方的提示
     * @param {HTMLElement} hintElement 提示元素
     * @param {string} text 提示内容
     * @param {string} type 提示类型：success、error、loading，空字符串表示普通提示
     */
    function setHint(hintElement, text, type) {
        hintElement.textContent = text || '';
        hintElement.className = 'field-hint' + (type ? ' hint-' + type : '');
    }

    /**
     * 设置输入框校验样式
     * @param {HTMLInputElement} input 输入框
     * @param {boolean|null} valid true 合法，false 非法，null 清除样式
     */
    function setInputState(input, valid) {
        input.classList.toggle('is-valid', valid === true);
        input.classList.toggle('is-invalid', valid === false);
    }

    /**
     * 校验邮箱格式
     * @param {string} email 邮箱
     * @returns {boolean} 格式合法返回true
     */
    function isValidEmail(email) {
        return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
    }

    /**
     * 检测昵称是否唯一
     */
    async function checkNickname() {
        const nickname = nicknameInput.value.trim();
        if (!nickname) {
            nicknameAvailable = null;
            setInputState(nicknameInput, null);
            setHint(nicknameHint, '', '');
            return;
        }

        setHint(nicknameHint, '正在检测昵称...', 'loading');
        try {
            const response = await UserApi.validNickname(nickname);
            if (nicknameInput.value.trim() !== nickname) {
                return;
            }
            if (isApiSuccess(response)) {
                nicknameAvailable = true;
                setInputState(nicknameInput, true);
                setHint(nicknameHint, '昵称可用', 'success');
            } else {
                nicknameAvailable = false;
                setInputState(nicknameInput, false);
                setHint(nicknameHint, response.errorMsg || '昵称已被使用', 'error');
            }
        } catch (error) {
            nicknameAvailable = null;
            setInputState(nicknameInput, null);
            setHint(nicknameHint, '昵称检测失败，请稍后重试', 'error');
        }
    }

    /**
     * 检测邮箱是否唯一
     */
    async function checkEmail() {
        const email = emailInput.value.trim();
        if (!email) {
            emailAvailable = null;
            setInputState(emailInput, null);
            setHint(emailHint, '', '');
            return;
        }
        if (!isValidEmail(email)) {
            emailAvailable = false;
            setInputState(emailInput, false);
            setHint(emailHint, '邮箱格式不正确', 'error');
            return;
        }

        setHint(emailHint, '正在检测邮箱...', 'loading');
        try {
            const response = await UserApi.validEmail(email);
            if (emailInput.value.trim() !== email) {
                return;
            }
            if (isApiSuccess(response)) {
                emailAvailable = true;
                setInputState(emailInput, true);
                setHint(emailHint, '邮箱可用', 'success');
            } else {
                emailAvailable = false;
                setInputState(emailInput, false);
                setHint(emailHint, response.errorMsg || '邮箱已被注册', 'error');
            }
        } catch (error) {
            emailAvailable = null;
            setInputState(emailInput, null);
            setHint(emailHint, '邮箱检测失败，请稍后重试', 'error');
        }
    }

    /**
     * 设置注册按钮加载状态
     * @param {boolean} loading 是否加载中
     */
    function setSubmitLoading(loading) {
        registerSubmit.disabled = loading;
        registerSubmit.textContent = loading ? '注册中...' : '注册';
    }

    // 昵称输入：防抖检测唯一性
    nicknameInput.addEventListener('input', function () {
        nicknameAvailable = null;
        setInputState(nicknameInput, null);
        setHint(nicknameHint, '', '');
        clearTimeout(nicknameTimer);
        nicknameTimer = setTimeout(checkNickname, 400);
    });

    // 邮箱输入：防抖检测唯一性
    emailInput.addEventListener('input', function () {
        emailAvailable = null;
        setInputState(emailInput, null);
        setHint(emailHint, '', '');
        clearTimeout(emailTimer);
        emailTimer = setTimeout(checkEmail, 400);
    });

    // 失焦时立即检测
    nicknameInput.addEventListener('blur', checkNickname);
    emailInput.addEventListener('blur', checkEmail);

    // 提交注册
    registerForm.addEventListener('submit', async function (event) {
        event.preventDefault();
        const nickname = nicknameInput.value.trim();
        const email = emailInput.value.trim();
        const password = passwordInput.value;
        const confirmPassword = confirmPasswordInput.value;

        if (!nickname) {
            showMessage('请输入昵称', 'error');
            nicknameInput.focus();
            return;
        }
        if (!email || !isValidEmail(email)) {
            showMessage('请输入正确的邮箱', 'error');
            emailInput.focus();
            return;
        }
        if (password.length < 6 || /\s/.test(password)) {
            showMessage('密码至少6位且不能包含空格', 'error');
            passwordInput.focus();
            return;
        }
        if (password !== confirmPassword) {
            showMessage('两次输入的密码不一致', 'error');
            confirmPasswordInput.focus();
            return;
        }
        if (nicknameAvailable === false) {
            showMessage('昵称已被使用，请更换昵称', 'error');
            return;
        }
        if (emailAvailable === false) {
            showMessage('邮箱已被注册，请更换邮箱', 'error');
            return;
        }

        setSubmitLoading(true);
        try {
            // 提交前确保兜底做过唯一性检测
            if (nicknameAvailable === null) {
                await checkNickname();
            }
            if (emailAvailable === null) {
                await checkEmail();
            }
            if (nicknameAvailable !== true) {
                showMessage('昵称不可用，请更换昵称', 'error');
                return;
            }
            if (emailAvailable !== true) {
                showMessage('邮箱不可用，请更换邮箱', 'error');
                return;
            }

            const response = await UserApi.register(nickname, password, email);
            if (!isApiSuccess(response)) {
                showMessage(response.errorMsg || '注册失败，请稍后重试', 'error');
                return;
            }
            sessionStorage.setItem(APP_CONFIG.STORAGE_KEYS.FLASH_MESSAGE, '注册成功，请使用新账号登录');
            window.location.href = '/login.html';
        } catch (error) {
            showMessage(error.message || '网络异常，请稍后重试', 'error');
        } finally {
            setSubmitLoading(false);
        }
    });
})();
