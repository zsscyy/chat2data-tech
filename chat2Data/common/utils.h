#pragma once

#include <cstddef>
#include <string>

namespace chat2Data {

class Utils {
public:
    /**
     * @brief  生成符合项目规则的UUID字符串
     * @return 8字节UUID字符串，格式为 xxxx_xxxxxxxx_yyyy，例如 fd7c_91ceb230_0000
     */
    static std::string generateUuid();

    /**
     * @brief  对二进制数据进行标准Base64编码（RFC 4648，含末尾'='填充）
     * @param  data    待编码数据首地址
     * @param  length  待编码字节数
     * @return Base64编码结果；data为空或length为0时返回空字符串
     */
    static std::string base64Encode(const unsigned char* data, std::size_t length);
};

} // namespace chat2Data
