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
                throw new Error('请求失败');
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

    /**
     * 判断是否为网络层错误（服务未启动、连接被重置等）
     * 业务错误（errorCode != 0）不重试
     * @param {Error} error 异常对象
     * @returns {boolean} 网络错误返回true
     */
    function isNetworkError(error) {
        if (!error) {
            return false;
        }
        if (error.name === 'AbortError') {
            return false;
        }
        return error.name === 'TypeError' ||
            /failed to fetch|networkerror|network error|load failed/i.test(error.message || '');
    }

    /**
     * 等待指定毫秒数
     * @param {number} ms 毫秒
     * @returns {Promise<void>} 等待Promise
     */
    function sleep(ms) {
        return new Promise(function (resolve) {
            setTimeout(resolve, ms);
        });
    }

    /**
     * 对网络错误做有限次重试，业务错误立即抛出
     * @param {Function} operation 返回Promise的操作
     * @param {number} attempts 最多尝试次数
     * @param {number} delayMs 首次重试等待时间
     * @returns {Promise<*>} 操作结果
     */
    async function withNetworkRetry(operation, attempts, delayMs) {
        const maxAttempts = attempts || 3;
        const baseDelay = delayMs || 300;
        let lastError;
        for (let attempt = 0; attempt < maxAttempts; attempt += 1) {
            try {
                return await operation();
            } catch (error) {
                lastError = error;
                if (!isNetworkError(error) || attempt === maxAttempts - 1) {
                    throw error;
                }
                await sleep(baseDelay * (attempt + 1));
            }
        }
        throw lastError;
    }

    /**
     * 拼装URL查询参数
     * @param {object} params 参数对象
     * @returns {string} 查询字符串
     */
    function buildQuery(params) {
        const searchParams = new URLSearchParams();
        Object.keys(params).forEach(function (key) {
            if (typeof params[key] !== 'undefined' && params[key] !== null) {
                searchParams.append(key, params[key]);
            }
        });
        return searchParams.toString();
    }

    /**
     * 获取当前登录会话ID
     * @returns {string} 会话ID，未登录时返回空字符串
     */
    function getSessionId() {
        return global.SessionStore ? global.SessionStore.getSessionId() : '';
    }

    /**
     * 判断功能开关是否开启
     * @param {string} featureName 功能名
     * @returns {boolean} 开启返回true
     */
    function isFeatureEnabled(featureName) {
        return !!(global.APP_FEATURES && global.APP_FEATURES[featureName]);
    }

    /**
     * 功能未开放时的统一响应
     * @returns {Promise<object>} errorCode为501的响应
     */
    function featureDisabled() {
        return Promise.resolve({errorCode: 501, errorMsg: '功能待开放'});
    }

    /**
     * 发送二进制下载请求
     * 成功时返回blob，后端返回JSON错误时返回解析后的错误对象
     * @param {string} path 请求路径
     * @param {object} options fetch配置
     * @returns {Promise<Blob|object>} 文件blob或错误响应
     */
    async function requestBlob(path, options) {
        const controller = new AbortController();
        const timer = setTimeout(function () {
            controller.abort();
        }, config.REQUEST_TIMEOUT);

        try {
            const response = await fetch(config.API_BASE_URL + path, Object.assign({signal: controller.signal}, options));
            const contentType = response.headers.get('Content-Type') || '';
            if (contentType.indexOf('application/json') !== -1) {
                const text = await response.text();
                let data = {};
                if (text) {
                    try {
                        data = JSON.parse(text);
                    } catch (error) {
                        throw new Error('服务返回数据格式错误');
                    }
                }
                return data;
            }
            if (!response.ok) {
                throw new Error('请求失败');
            }
            return await response.blob();
        } catch (error) {
            if (error.name === 'AbortError') {
                throw new Error('请求超时，请稍后重试');
            }
            throw error;
        } finally {
            clearTimeout(timer);
        }
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
     * 文件子服务接口封装
     * 上传文件分为两步：先上传文件信息获取fileId，再上传文件二进制数据
     */
    const FileApi = {
        /**
         * 上传文件信息，获取文件ID
         * @param {object} fileInfo 文件信息：filename、fileSize、fileExt
         * @returns {Promise<object>} 接口响应，result.fileId为文件ID
         */
        uploadFileInfo: function (fileInfo) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            return postJson(config.FILE_API.UPLOAD_FILE_INFO, {
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileInfo: fileInfo
            });
        },

        /**
         * 上传文件二进制数据
         * @param {File} file 文件对象
         * @param {string} fileId 文件ID
         * @returns {Promise<object>} 接口响应，result.fileId为文件ID
         */
        uploadFile: function (file, fileId) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const query = buildQuery({
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileId: fileId
            });
            return withNetworkRetry(function () {
                return request(config.FILE_API.UPLOAD_FILE + '?' + query, {
                    method: 'POST',
                    headers: {'Content-Type': 'application/octet-stream'},
                    body: file
                });
            }, 2);
        },

        /**
         * 获取文件信息
         * @param {string} fileId 文件ID
         * @returns {Promise<object>} 接口响应，result为文件信息
         */
        getFileInfo: function (fileId) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const query = buildQuery({
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileId: fileId
            });
            return withNetworkRetry(function () {
                return request(config.FILE_API.GET_FILE_INFO + '?' + query, {method: 'GET'});
            }, 2);
        },

        /**
         * 获取当前用户的文件列表
         * @returns {Promise<object>} 接口响应，result.fileList为文件数组
         */
        getFileList: function () {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            return withNetworkRetry(function () {
                return postJson(config.FILE_API.FILE_LIST, {
                    requestId: generateRequestId(),
                    sessionId: getSessionId()
                });
            }, 3);
        },

        /**
         * 预览Excel文件
         * @param {string} fileId 文件ID
         * @param {number} pageNumber 页码，可选，默认1
         * @param {number} pageSize 每页行数，可选，默认50
         * @returns {Promise<object>} 接口响应，result.excelData为表格数据
         */
        previewExcel: function (fileId, pageNumber, pageSize) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const payload = {
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileId: fileId
            };
            if (typeof pageNumber === 'number') {
                payload.pageNumber = pageNumber;
            }
            if (typeof pageSize === 'number') {
                payload.pageSize = pageSize;
            }
            return withNetworkRetry(function () {
                return postJson(config.FILE_API.PREVIEW_EXCEL, payload);
            }, 2);
        },

        /**
         * 下载文件
         * @param {string} fileId 文件ID
         * @returns {Promise<Blob|object>} 成功返回文件blob，失败返回错误对象
         */
        downloadFile: function (fileId) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const query = buildQuery({
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileId: fileId
            });
            return withNetworkRetry(function () {
                return requestBlob(config.FILE_API.DOWNLOAD_FILE + '?' + query, {method: 'GET'});
            }, 2);
        },

        /**
         * 删除文件
         * @param {string} fileId 文件ID
         * @returns {Promise<object>} 接口响应
         */
        deleteFile: function (fileId) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const query = buildQuery({
                requestId: generateRequestId(),
                sessionId: getSessionId()
            });
            return withNetworkRetry(function () {
                return request(config.FILE_API.DELETE_FILE + encodeURIComponent(fileId) + '?' + query, {
                    method: 'DELETE'
                });
            }, 3);
        },

        /**
         * 上传SQLite文件
         * @param {File} file SQLite文件对象
         * @returns {Promise<object>} 接口响应，result.fileId为文件ID
         */
        uploadSqliteFile: function (file) {
            if (!isFeatureEnabled('FILE_SERVICE')) {
                return featureDisabled();
            }
            const query = buildQuery({
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                filename: file.name
            });
            return request(config.FILE_API.UPLOAD_SQLITE_FILE + '?' + query, {
                method: 'POST',
                headers: {'Content-Type': 'application/octet-stream'},
                body: file
            });
        },

        /**
         * 关联文件与聊天会话
         * 当前AI子服务未开放，受AI_SERVICE开关控制
         * @param {string} fileId 文件ID
         * @param {string} chatSessionId 聊天会话ID
         * @returns {Promise<object>} 接口响应
         */
        mapChatSession: function (fileId, chatSessionId) {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            return postJson(config.FILE_API.MAP_CHAT_SESSION, {
                requestId: generateRequestId(),
                sessionId: getSessionId(),
                fileId: fileId,
                chatSessionId: chatSessionId
            });
        }
    };

    /**
     * 数据库子服务预留接口
     * 当前网关未开放，开关为false时直接返回501，不发送真实请求
     */
    const DbApi = {
        /**
         * 新建数据库连接
         * @param {object} database 数据库连接配置
         * @returns {Promise<object>} 接口响应
         */
        connect: function (database) {
            if (!isFeatureEnabled('DB_SERVICE')) {
                return featureDisabled();
            }
            // 待数据库子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 断开数据库连接
         * @param {string} connectionId 连接ID
         * @returns {Promise<object>} 接口响应
         */
        disconnect: function (connectionId) {
            if (!isFeatureEnabled('DB_SERVICE')) {
                return featureDisabled();
            }
            // 待数据库子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 获取数据库表列表
         * @param {string} dbConnectId 数据库连接ID
         * @returns {Promise<object>} 接口响应
         */
        getTables: function (dbConnectId) {
            if (!isFeatureEnabled('DB_SERVICE')) {
                return featureDisabled();
            }
            // 待数据库子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 获取表数据
         * @param {string} dbConnectId 数据库连接ID
         * @param {string} tableName 表名
         * @param {boolean} forceOriginal 是否强制获取原始表数据
         * @returns {Promise<object>} 接口响应
         */
        getTableData: function (dbConnectId, tableName, forceOriginal) {
            if (!isFeatureEnabled('DB_SERVICE')) {
                return featureDisabled();
            }
            // 待数据库子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 获取指定连接的临时表列表
         * @param {string} dbConnectId 数据库连接ID
         * @returns {Promise<object>} 接口响应
         */
        getConnectionStatus: function (dbConnectId) {
            if (!isFeatureEnabled('DB_SERVICE')) {
                return featureDisabled();
            }
            // 待数据库子服务接口开放后补充具体请求实现
            return featureDisabled();
        }
    };

    /**
     * AI子服务预留接口
     * 当前网关未开放，开关为false时直接返回501，不发送真实请求
     */
    const AiApi = {
        /**
         * 获取支持的模型列表
         * @returns {Promise<object>} 接口响应
         */
        getModels: function () {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 新建聊天会话
         * @param {string} modelName 模型名称
         * @param {string} title 会话标题
         * @returns {Promise<object>} 接口响应
         */
        createSession: function (modelName, title) {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 获取聊天会话列表
         * @returns {Promise<object>} 接口响应
         */
        getChatSessionList: function () {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 获取聊天会话历史消息
         * @param {string} chatSessionId 聊天会话ID
         * @returns {Promise<object>} 接口响应
         */
        getHistory: function (chatSessionId) {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 删除聊天会话
         * @param {string} chatSessionId 聊天会话ID
         * @returns {Promise<object>} 接口响应
         */
        deleteSession: function (chatSessionId) {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务接口开放后补充具体请求实现
            return featureDisabled();
        },

        /**
         * 发送流式消息
         * @param {object} payload 消息参数
         * @returns {Promise<object>} 接口响应
         */
        sendStreamMessage: function (payload) {
            if (!isFeatureEnabled('AI_SERVICE')) {
                return featureDisabled();
            }
            // 待AI子服务开放后补充SSE流式请求实现
            return featureDisabled();
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
    global.FileApi = FileApi;
    global.DbApi = DbApi;
    global.AiApi = AiApi;
    global.isApiSuccess = isApiSuccess;
})(window);
