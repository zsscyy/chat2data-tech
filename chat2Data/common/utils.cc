#include <atomic>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

#include "utils.h"

namespace chat2Data {

namespace {
// 随机部分总字节数，对应UUID前12个十六进制字符
constexpr int RANDOM_BYTE_COUNT = 6;
// 前4个十六进制字符由2个随机字节组成
constexpr int RANDOM_BYTE_GROUP_SIZE = 2;
// 单字节取值范围上限
constexpr int MAX_BYTE_VALUE = 255;
// 单字节对应的十六进制字符宽度
constexpr int BYTE_HEX_WIDTH = 2;
// 自增整数对应的十六进制字符宽度
constexpr int SEQUENCE_HEX_WIDTH = 4;
constexpr char UUID_SEPARATOR = '_';

// Base64编码表，索引为6位无符号值
constexpr char BASE64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// Base64每3个字节编码为4个字符
constexpr std::size_t BASE64_GROUP_BYTES = 3;
// Base64每组输出字符数
constexpr std::size_t BASE64_GROUP_CHARS = 4;
// 取6位有效位的掩码
constexpr unsigned int BASE64_CHAR_MASK = 0x3F;
// 每组3字节组装为24位整数时，第1个字节的左移位数
constexpr int BASE64_FIRST_BYTE_SHIFT = 16;
// 每组3字节组装为24位整数时，第2个字节的左移位数
constexpr int BASE64_SECOND_BYTE_SHIFT = 8;
// 每组3字节左移的位数，使首字节占据最高6位
constexpr int BASE64_FIRST_CHAR_SHIFT = 18;
// 每组3字节左移的位数，使第2个6位组落到正确位置
constexpr int BASE64_SECOND_CHAR_SHIFT = 12;
// 每组3字节左移的位数，使第3个6位组落到正确位置
constexpr int BASE64_THIRD_CHAR_SHIFT = 6;
// Base64不足一组时的填充字符
constexpr char BASE64_PADDING = '=';

// UUID后2字节为自增整数，使用原子类型保证多线程下递增安全，溢出后由无符号类型自然回绕
static std::atomic<std::uint16_t> uuidSequence{0};

/**
 * @brief  获取当前线程的梅森旋转随机数引擎
 * @return 当前线程独立的随机数引擎引用
 */
std::mt19937& getRandomEngine() {
    // 每个线程独立持有引擎，避免多线程竞争；种子从操作系统真随机源获取
    thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}
} // namespace

std::string Utils::generateUuid() {
    std::ostringstream uuidStream;
    uuidStream << std::hex << std::setfill('0');

    // 前6字节逐字节生成随机数，每个字节转换为2个十六进制字符
    std::uniform_int_distribution<int> byteDistribution(0, MAX_BYTE_VALUE);
    std::mt19937& randomEngine = getRandomEngine();
    for (int byteIndex = 0; byteIndex < RANDOM_BYTE_COUNT; ++byteIndex) {
        if (byteIndex == RANDOM_BYTE_GROUP_SIZE) {
            uuidStream << UUID_SEPARATOR;
        }
        uuidStream << std::setw(BYTE_HEX_WIDTH) << byteDistribution(randomEngine);
    }

    // 后2字节为自增整数，原子自增保证唯一性，转换为4个十六进制字符
    const std::uint16_t sequence = uuidSequence.fetch_add(1, std::memory_order_relaxed);
    uuidStream << UUID_SEPARATOR << std::setw(SEQUENCE_HEX_WIDTH) << static_cast<unsigned int>(sequence);

    return uuidStream.str();
}

std::string Utils::base64Encode(const unsigned char* data, std::size_t length) {
    if (data == nullptr || length == 0) {
        return std::string();
    }

    const std::size_t groupCount = (length + BASE64_GROUP_BYTES - 1) / BASE64_GROUP_BYTES;
    std::string encoded;
    encoded.reserve(groupCount * BASE64_GROUP_CHARS);

    for (std::size_t offset = 0; offset < length; offset += BASE64_GROUP_BYTES) {
        // 不足3字节的尾组，缺失字节按0参与运算，输出时再补'='
        const unsigned int firstByte = data[offset];
        const unsigned int secondByte = (offset + 1 < length) ? data[offset + 1] : 0;
        const unsigned int thirdByte = (offset + 2 < length) ? data[offset + 2] : 0;
        const unsigned int triple =
            (firstByte << BASE64_FIRST_BYTE_SHIFT) | (secondByte << BASE64_SECOND_BYTE_SHIFT) | thirdByte;

        encoded.push_back(BASE64_TABLE[(triple >> BASE64_FIRST_CHAR_SHIFT) & BASE64_CHAR_MASK]);
        encoded.push_back(BASE64_TABLE[(triple >> BASE64_SECOND_CHAR_SHIFT) & BASE64_CHAR_MASK]);
        encoded.push_back((offset + 1 < length)
                              ? BASE64_TABLE[(triple >> BASE64_THIRD_CHAR_SHIFT) & BASE64_CHAR_MASK]
                              : BASE64_PADDING);
        encoded.push_back((offset + 2 < length)
                              ? BASE64_TABLE[triple & BASE64_CHAR_MASK]
                              : BASE64_PADDING);
    }

    return encoded;
}

} // namespace chat2Data
