/**
 * 用户子服务接口封装
 * 统一处理请求ID生成、超时、JSON解析与错误提示
 */
(function (global) {
    'use strict';

    const config = global.APP_CONFIG;

    /**
     * 生成请求ID
     * @returns {string} 形如 req_1700000000000_ab12cd34 的请求ID
     */
    function generateRequestId() {
        return 'req_' + Date.now() + '_' + Math.random().toString(36).slice(2, 10);
    }

    /**
     * 发送HTTP请求
     * @param {string} path 请求路径
     * @param {object} options fetch配置
     * @returns {Promise<object>} 后端返回的JSON对象
     */
    async function request(path, options) {
        const controller = new AbortController();
        const timer = setTimeout(function () {
            controller.abort();
        }, config.REQUEST_TIMEOUT);

        try {
            const response = await fetch(config.API_BASE_URL + path, Object.assign({signal: controller.signal}, options));
            const text = await response.text();
            let data = {};
            if (text) {
                try {
                    data = JSON.parse(text);
                } catch (error) {
                    throw new Error('服务返回数据格式错误');
                }
            }
            // 网关业务错误通过errorCode返回，HTTP状态码通常仍为200
            if (!response.ok && typeof data.errorCode === 'undefined') {
                throw new Error('请求失败（HTTP ' + response.status + '）');
            }
            return data;
        } catch (error) {
            if (error.name === 'AbortError') {
                throw new Error('请求超时，请稍后重试');
            }
            throw error;
        } finally {
            clearTimeout(timer);
        }
    }

    /**
     * 发送JSON格式POST请求
     * @param {string} path 请求路径
     * @param {object} payload 请求体
     * @returns {Promise<object>} 后端返回的JSON对象
     */
    function postJson(path, payload) {
        return request(path, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
    }

    const UserApi = {
        /**
         * 检测昵称是否唯一
         * @param {string} nickname 用户昵称
         * @returns {Promise<object>} 接口响应
         */
        validNickname: function (nickname) {
            return postJson(config.USER_API.VALID_NICKNAME, {
                requestId: generateRequestId(),
                nickname: nickname
            });
        },

        /**
         * 检测邮箱是否唯一
         * @param {string} email 用户邮箱
         * @returns {Promise<object>} 接口响应
         */
        validEmail: function (email) {
            return postJson(config.USER_API.VALID_EMAIL, {
                requestId: generateRequestId(),
                email: email
            });
        },

        /**
         * 用户注册
         * @param {string} nickname 用户昵称
         * @param {string} password 用户密码
         * @param {string} email 用户邮箱
         * @returns {Promise<object>} 接口响应
         */
        register: function (nickname, password, email) {
            return postJson(config.USER_API.REGISTER, {
                requestId: generateRequestId(),
                nickname: nickname,
                password: password,
                email: email
            });
        },

        /**
         * 密码登录
         * @param {string} username 昵称或邮箱
         * @param {string} password 密码
         * @returns {Promise<object>} 接口响应，result.sessionId为会话ID
         */
        passwordLogin: function (username, password) {
            return postJson(config.USER_API.PASSWD_LOGIN, {
                requestId: generateRequestId(),
                username: username,
                password: password
            });
        },

        /**
         * 获取邮箱验证码
         * @param {string} email 用户邮箱
         * @returns {Promise<object>} 接口响应，result.codeId为验证码ID
         */
        getVerifyCode: function (email) {
            return postJson(config.USER_API.GET_CODE, {
                requestId: generateRequestId(),
                email: email
            });
        },

        /**
         * 验证码登录
         * @param {string} email 用户邮箱
         * @param {string} verifyCode 验证码
         * @param {string} codeId 验证码ID
         * @returns {Promise<object>} 接口响应，result.sessionId为会话ID
         */
        verifyCodeLogin: function (email, verifyCode, codeId) {
            return postJson(config.USER_API.VCODE_LOGIN, {
                requestId: generateRequestId(),
                email: email,
                verifyCode: verifyCode,
                codeId: codeId
            });
        },

        /**
         * 会话登录
         * @param {string} sessionId 会话ID
         * @returns {Promise<object>} 接口响应
         */
        sessionLogin: function (sessionId) {
            return postJson(config.USER_API.SESSION_LOGIN, {
                requestId: generateRequestId(),
                sessionId: sessionId
            });
        },

        /**
         * 退出登录
         * @param {string} sessionId 会话ID
         * @returns {Promise<object>} 接口响应
         */
        logout: function (sessionId) {
            return postJson(config.USER_API.LOGOUT, {
                requestId: generateRequestId(),
                sessionId: sessionId
            });
        },

        /**
         * 获取用户信息
         * @param {string} sessionId 会话ID
         * @param {string} userId 用户ID，可为空
         * @returns {Promise<object>} 接口响应，result.userInfo为用户信息
         */
        getUserInfo: function (sessionId, userId) {
            const params = new URLSearchParams({
                requestId: generateRequestId(),
                sessionId: sessionId || '',
                userId: userId || ''
            });
            return request(config.USER_API.USER_INFO + '?' + params.toString(), {method: 'GET'});
        }
    };

    /**
     * 判断接口是否调用成功
     * @param {object} response 接口响应
     * @returns {boolean} errorCode为0返回true
     */
    function isApiSuccess(response) {
        return !!response && response.errorCode === 0;
    }

    global.generateRequestId = generateRequestId;
    global.UserApi = UserApi;
    global.isApiSuccess = isApiSuccess;
})(window);
