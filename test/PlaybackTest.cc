// SPDX-FileCopyrightText: 2022 Sveriges Television AB
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include <ostream>
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch2/catch.hpp"

extern "C" {
#include <SDL.h>
}


#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include "SourceConfig.hh"
#include "Controller.hh"
#include "TimeUtils.hh"
#include <stdlib.h>

/*
 
  Mock eventLoop or mock VivictOOUI


*/

static int SDLCALL my_event_filter(void *userdata, SDL_Event * event)
{
    (void) userdata;
    switch (event->type) {
    case SDL_QUIT:
    case SDL_MOUSEMOTION:
    case SDL_MOUSEWHEEL:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_KEYDOWN:
        return 0;
    case SDL_WINDOWEVENT:
        return 1;
    }
    return 1;
}

void mockKeyEvent(SDL_Keycode keycode) {
    SDL_Event event;
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = keycode;  //SDLK_SPACE
    SDL_PeepEvents(&event, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
}

void sleepSeconds(int n) {
    std::this_thread::sleep_for(std::chrono::seconds(n));
}

void sleepMillis(int n) {
    std::this_thread::sleep_for(std::chrono::milliseconds(n));
}

VivictPPConfig testConfig() {
    std::vector<SourceConfig> sourceConfigs;
//    sourceConfigs.push_back(SourceConfig("/home/gugr01/local-work/test-video/ToS-4k-1920.mov"));
//    sourceConfigs.push_back(SourceConfig("/home/gugr01/local-work/test-video/ToS-4k-1920.mov"));
    sourceConfigs.push_back(SourceConfig("../testdata/test1.mp4"));
    sourceConfigs.push_back(SourceConfig("../testdata/test1.mp4"));
    VivictPPConfig vivictPPConfig(sourceConfigs, true);
    return vivictPPConfig;
}

class EventFilterInitializer {
public:
    EventFilterInitializer() {
        SDL_SetEventFilter(my_event_filter, nullptr);
    }

    ~EventFilterInitializer() {
        SDL_SetEventFilter(nullptr, nullptr);
    }

private:
    vivictpp::sdl::SDLInitializer sdlInitializer;

};

class TestContext{
public:
    TestContext():
        eventFilterInitializer(),
        sdlEventLoop(std::make_shared<vivictpp::sdl::SDLEventLoop>(testConfig().sourceConfigs)),
        controller(sdlEventLoop, sdlEventLoop, testConfig()),
        thread(&vivictpp::Controller::run, &controller) {

    };

    ~TestContext() {
        controller.onQuit();
        thread.join();
    };

    void stepForward() {
        mockKeyEvent(SDLK_PERIOD);
    };

    void stepBackward() {
        mockKeyEvent(SDLK_COMMA);
    };

    void seekForward() {
        mockKeyEvent(SDLK_SLASH);
    };

    void seekBackward() {
        mockKeyEvent(SDLK_m);
    };

    void togglePlay() {
        mockKeyEvent(SDLK_SPACE);
    }

    double currentPts() { return controller.getPlayerState().pts; }

    EventFilterInitializer eventFilterInitializer;
    std::shared_ptr<vivictpp::sdl::SDLEventLoop> sdlEventLoop;
    vivictpp::Controller controller;
    std::thread thread;
};



TEST_CASE( "Seeking" ) {
    vivictpp::logging::initializeLogging();
    std::cout << "Creating test context" << std::flush << std::endl;
    TestContext testContext;
    sleepMillis(100);

    SECTION("Seek forward") {
        testContext.seekForward();
        testContext.seekForward();
        sleepMillis(500);
        double t = testContext.controller.getPlayerState().pts;

        REQUIRE( t == 10);
    }

    SECTION("Seek backward") {
        testContext.seekForward();
        testContext.seekForward();
        sleepMillis(200);
        testContext.seekBackward();
        sleepMillis(200);
        double t = testContext.controller.getPlayerState().pts;

        REQUIRE( t == 5);
    }


    SECTION("Seek backward beyond start") {
        testContext.stepForward();
        testContext.stepForward();
        sleepMillis(200);
        testContext.seekBackward();
        sleepMillis(200);
        double t = testContext.controller.getPlayerState().pts;

        REQUIRE(t == 0);

    }
}

TEST_CASE( "Test playback" ) {
    vivictpp::logging::initializeLogging();
    TestContext testContext;
    sleepMillis(100);

    SECTION("Starts at pts 0") {
        double t = testContext.controller.getPlayerState().pts;

        REQUIRE(t == 0);
    }

    SECTION("Playback speed") {
        testContext.togglePlay();
        uint64_t a0 = testContext.controller.getPlayerState().lastFrameAdvance;
        while (a0 == std::numeric_limits<uint64_t>::min()) {
            sleepMillis(1);
            a0 = testContext.controller.getPlayerState().lastFrameAdvance;
        }
        double t0 = testContext.controller.getPlayerState().pts;

        sleepSeconds(10);

        double t1 = testContext.controller.getPlayerState().pts;
        uint64_t a1 = testContext.controller.getPlayerState().lastFrameAdvance;
        uint64_t b1 = vivictpp::util::relativeTimeMicros();
        uint64_t playerElapsedTimeMillis = static_cast<uint64_t>(1000 * (t1 - t0)) + (b1 - a1) / 1000;

        int drift = std::abs(static_cast<int>(playerElapsedTimeMillis) - 10000);

        REQUIRE(drift < 10);

    }

    SECTION("Stops at end of stream") {
        // TODO
    }

}
