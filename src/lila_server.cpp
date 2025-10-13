#include "Lila/Lila.hpp"

int main(int argc, char** argv)
{
    auto lila = std::make_shared<Lila::Lila>();
    if (!lila->initialize(Lila::OperationMode::Both, 9090)) {
        std::cerr << "Failed to init: " << lila->get_last_error() << "\n";
        return 1;
    }

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}
