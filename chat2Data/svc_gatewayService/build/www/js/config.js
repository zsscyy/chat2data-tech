/**
 * 前端全局配置
 * 页面通过相对路径访问网关，静态资源与接口同源部署
 */
window.APP_CONFIG = {
    // 网关服务地址，留空表示与当前页面同源
    API_BASE_URL: '',
    // 请求超时时间，单位毫秒
    REQUEST_TIMEOUT: 15000,
    // 本地存储键名
    STORAGE_KEYS: {
        SESSION_ID: 'chat2data_session_id',
        USER_INFO: 'chat2data_user_info',
        FLASH_MESSAGE: 'chat2data_flash_message'
    },
    // 用户子服务接口路径
    USER_API: {
        VALID_NICKNAME: '/api/user/valid/nickname',
        VALID_EMAIL: '/api/user/valid/email',
        REGISTER: '/api/user/register',
        PASSWD_LOGIN: '/api/user/passwd/login',
        GET_CODE: '/api/user/code',
        VCODE_LOGIN: '/api/user/vcode/login',
        SESSION_LOGIN: '/api/user/session/login',
        LOGOUT: '/api/user/logout',
        USER_INFO: '/api/user/info'
    }
};
