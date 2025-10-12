#include "Lila/Lila.hpp"
#include "Lila/Server.hpp"

#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

#include <csignal>
#include <iostream>

std::atomic<bool> g_running { true };

void signal_handler(int)
{
    g_running = false;
}

int main(int argc, char** argv)
{
    auto lila = std::make_shared<Lila::Lila>();
    if (!lila->initialize()) {
        std::cerr << "Failed to init: " << lila->get_last_error() << "\n";
        return 1;
    }

    Lila::Server server(lila, 9090);
    server.start();

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}
