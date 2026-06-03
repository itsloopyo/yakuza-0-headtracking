#include <iostream>

int RunViewMathTests();

int main() {
    std::cout << "Yakuza0HeadTracking Tests\n";
    std::cout << "=========================\n";

    int failures = 0;
    failures += RunViewMathTests();

    if (failures == 0) {
        std::cout << "All tests passed!\n";
        return 0;
    }
    std::cout << failures << " test(s) FAILED\n";
    return 1;
}
