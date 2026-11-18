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
    },
    // 文件子服务接口路径
    FILE_API: {
        UPLOAD_FILE_INFO: '/api/file/upload/info',
        GET_FILE_INFO: '/api/file/info',
        UPLOAD_FILE: '/api/file/upload',
        DOWNLOAD_FILE: '/api/file/download',
        DELETE_FILE: '/api/file/',
        FILE_LIST: '/api/file/list',
        PREVIEW_EXCEL: '/api/file/preview',
        MAP_CHAT_SESSION: '/api/file/chat/map',
        UPLOAD_SQLITE_FILE: '/api/file/sqlite/upload'
    },
    // 数据库子服务接口路径
    DB_API: {
        CONNECT: '/api/db/connect',
        DISCONNECT: '/api/db/disconnect',
        TABLES: '/api/db/tables',
        TABLE_DATA: '/api/db/table/data',
        CONNECTION_STATUS: '/api/db/connection/status'
    },
    // AI子服务接口路径
    AI_API: {
        MODELS: '/api/ai/models',
        CREATE_SESSION: '/api/ai/session/create',
        SESSION_LIST: '/api/ai/chatSessionLists',
        HISTORY: '/api/ai/history',
        DELETE_SESSION: '/api/ai/delete',
        SEND_MESSAGE: '/api/ai/sendStreamMessage'
    }
};

/**
 * 功能开关
 * 网关尚未实现的子服务保持false，对应接口开放后只需改为true
 */
window.APP_FEATURES = {
    FILE_SERVICE: true,
    DB_SERVICE: false,
    AI_SERVICE: false
};
