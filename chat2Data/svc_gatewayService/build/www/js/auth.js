/**
 * 登录态管理
 * 负责会话ID、用户信息与页面跳转提示的本地存储
 */
(function (global) {
    'use strict';

    const keys = global.APP_CONFIG.STORAGE_KEYS;

    const SessionStore = {
        /**
         * 获取本地保存的会话ID
         * @returns {string} 会话ID，不存在时返回空字符串
         */
        getSessionId: function () {
            return localStorage.getItem(keys.SESSION_ID) || '';
        },

        /**
         * 保存会话ID
         * @param {string} sessionId 会话ID
         */
        setSessionId: function (sessionId) {
            localStorage.setItem(keys.SESSION_ID, sessionId);
        },

        /**
         * 获取本地保存的用户信息
         * @returns {object|null} 用户信息对象，不存在或解析失败返回null
         */
        getUserInfo: function () {
            try {
                return JSON.parse(localStorage.getItem(keys.USER_INFO) || 'null');
            } catch (error) {
                return null;
            }
        },

        /**
         * 保存用户信息
         * @param {object} userInfo 用户信息
         */
        setUserInfo: function (userInfo) {
            localStorage.setItem(keys.USER_INFO, JSON.stringify(userInfo));
        },

        /**
         * 清除本地登录态
         */
        clear: function () {
            localStorage.removeItem(keys.SESSION_ID);
            localStorage.removeItem(keys.USER_INFO);
        }
    };

    /**
     * 跳转到登录页
     * @param {string} message 需要在登录页展示的提示信息
     */
    function redirectToLogin(message) {
        if (message) {
            sessionStorage.setItem(keys.FLASH_MESSAGE, message);
        }
        window.location.href = '/login.html';
    }

    /**
     * 读取并清除一次性提示信息
     * @returns {string} 提示信息，不存在时返回空字符串
     */
    function consumeFlashMessage() {
        const message = sessionStorage.getItem(keys.FLASH_MESSAGE) || '';
        sessionStorage.removeItem(keys.FLASH_MESSAGE);
        return message;
    }

    /**
     * 显示页面消息
     * @param {string} text 消息内容
     * @param {string} type 消息类型：success、error、info
     */
    function showMessage(text, type) {
        const messageBox = document.getElementById('message');
        if (!messageBox) {
            return;
        }
        messageBox.textContent = text || '';
        messageBox.className = 'message' + (text ? ' message-' + (type || 'info') : '');
    }

    global.SessionStore = SessionStore;
    global.redirectToLogin = redirectToLogin;
    global.consumeFlashMessage = consumeFlashMessage;
    global.showMessage = showMessage;
})(window);
