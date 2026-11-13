#include "../../common/utils.h"
#include <iostream>

// UUID 生成次数
constexpr int UUID_SAMPLE_COUNT = 10;

int main() {
    for (int i = 0; i < UUID_SAMPLE_COUNT; ++i) {
        std::cout << chat2Data::Utils::generateUuid() << std::endl;
    }
    return 0;
}
