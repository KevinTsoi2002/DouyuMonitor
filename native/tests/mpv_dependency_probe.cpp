#include <cstdlib>
#include <iostream>

#include <mpv/client.h>

int main()
{
    if (mpv_client_api_version() == 0) {
        std::cerr << "libmpv reported an invalid API version\n";
        return EXIT_FAILURE;
    }

    mpv_handle *ctx = mpv_create();
    if (ctx == nullptr) {
        std::cerr << "libmpv could not create a client handle\n";
        return EXIT_FAILURE;
    }

    if (mpv_client_name(ctx) == nullptr) {
        std::cerr << "libmpv did not report a client name\n";
        mpv_terminate_destroy(ctx);
        return EXIT_FAILURE;
    }

    mpv_terminate_destroy(ctx);

    return EXIT_SUCCESS;
}
