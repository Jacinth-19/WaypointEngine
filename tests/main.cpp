// Waypoint Engine — test runner entry point.
// SPDX-License-Identifier: Apache-2.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "TestDoctest.hpp"

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    // Stop the whole run on the first failure inside a test case only; keep
    // going across test cases so a single run reports everything that is broken.
    context.setOption("no-breaks", true);

    return context.run();
}
