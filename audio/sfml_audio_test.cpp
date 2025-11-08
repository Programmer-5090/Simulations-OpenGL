#include "audio/audio.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== Audio Test ===" << std::endl;

    Audio audio;

    // Test loading
    std::cout << "Loading audio file..." << std::endl;
    if (!audio.load("audio/beep.wav")) {
        std::cerr << "Failed to load audio file!" << std::endl;
        return -1;
    }
    
    std::cout << "Duration: " << audio.getDuration() << " seconds" << std::endl;
    
    // Test 1: Basic playback
    std::cout << "\n🔊 Test 1: Basic playback" << std::endl;
    audio.play();
    
    while (audio.isPlaying()) {
        std::cout << "Playing... (" << audio.getPlayingOffset() << "s)" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test 2: Volume control
    std::cout << "\n🔊 Test 2: Volume test (50%)" << std::endl;
    audio.setVolume(50.0f);
    audio.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    audio.stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test 3: Pitch control
    std::cout << "\n🔊 Test 3: Pitch test (2x speed)" << std::endl;
    audio.setPitch(2.0f);
    audio.setVolume(100.0f);
    audio.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    audio.stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test 4: Looping
    std::cout << "\n🔊 Test 4: Looping for 3 seconds" << std::endl;
    audio.setPitch(1.0f);
    audio.setLoop(true);
    audio.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    audio.stop();
    
    std::cout << "\n✅ Audio test completed!" << std::endl;
    return 0;
}