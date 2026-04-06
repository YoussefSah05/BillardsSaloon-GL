#include "app/application.h"

#include <cstdlib>
#include <exception>
#include <iostream>

// Core entry point of the application - initializes and runs the Billiards Saloon application.
int main()
{
    try
    {
        BilliardsSaloon::Application app;
        return app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}