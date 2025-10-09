#include "Lila/Lila.hpp"
#include <iostream>

int main()
{
    Lila::Lila lila;

    if (!lila.initialize()) {
        std::cerr << "Failed to initialize Lila\n";
        return 1;
    }

    std::cout << "Lila initialized ✓\n\n";

    // Test 1: Free-form code (no live_reload function)
    const char* test1 = R"(
        #include <iostream>
        std::cout << "Hello from free-form JIT!\n";
        int result = 5 + 7;
        std::cout << "5 + 7 = " << result << "\n";
    )";

    std::cout << "=== Test 1: Free-form code ===\n";
    if (lila.eval(test1)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error: " << lila.get_last_error() << "\n\n";
    }

    // Test 2: Another free-form snippet (should not conflict)
    const char* test2 = R"(
        #include <iostream>
        std::cout << "Second snippet - no conflicts!\n";
    )";

    std::cout << "=== Test 2: Second snippet ===\n";
    if (lila.eval(test2)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error: " << lila.get_last_error() << "\n\n";
    }

    // Test 3: With user-defined function
    const char* test3 = R"(
    #include <iostream>
    
    void live_reload() {
        std::cout << "User-defined live_reload called!\n";
    }
)";

    std::cout << "=== Test 3: User-defined function ===\n";
    if (lila.eval(test3)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error: " << lila.get_last_error() << "\n\n";
    }

    return 0;
}
