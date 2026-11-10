#include "../../common/utils.h"
#include <iostream>

int main() {
    for (int i = 0; i < 10; ++i) {
        std::cout << chat2Data::Utils::generateUuid() << std::endl;
    }
    return 0;
}
