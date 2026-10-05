#include "app/application.h"
#include "core/launch_options.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

// Entry point: parse the command line, then run the game.
int main(int argc, char** argv)
{
    BilliardsSaloon::LaunchOptions options;
    try
    {
        const std::vector<std::string_view> arguments(argv + 1, argv + argc);
        options = BilliardsSaloon::parseLaunchOptions(arguments);
    }
    catch (const std::invalid_argument& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        BilliardsSaloon::Application app(options);
        return app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}