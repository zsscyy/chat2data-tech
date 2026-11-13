#include <bite_scaffold/mail.h>
#include <bite_scaffold/log.h>
#include <string>

int main(){
    // 1. 初始化日志器
    bitelog::bitelog_init();
    // 2. 设置邮箱配置
    bitecode::mail_settings settings = {
        .username = "17726854439@163.com",
        .password = "PPR9LQfPQCL98kkT",
        .url = "smtps://smtp.163.com:465",
        .from = "17726854439@163.com",
    };

    // 3. 创建邮件客户端并发送邮件
    bitecode::MailClient mailClient(settings);
    mailClient.send("264859838@qq.com", "123456");
    return 0;
}