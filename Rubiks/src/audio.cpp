#include "audio.h"
#include <iostream>

Audio::Audio() {
    // SFML handles initialization automatically
}

Audio::~Audio() {
    stop();
}

bool Audio::load(const std::string& filename) {
    if (!soundBuffer.loadFromFile(filename)) {
        std::cerr << "Failed to load audio file: " << filename << std::endl;
        loaded = false;
        return false;
    }
    
    sound.setBuffer(soundBuffer);
    loaded = true;
    std::cout << "Successfully loaded: " << filename << std::endl;
    return true;
}

void Audio::play() {
    if (!loaded) {
        std::cerr << "No audio loaded" << std::endl;
        return;
    }
    sound.play();
}

void Audio::pause() {
    sound.pause();
}

void Audio::stop() {
    sound.stop();
}

void Audio::setVolume(float volume) {
    // Clamp volume between 0 and 100
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 100.0f) volume = 100.0f;
    sound.setVolume(volume);
}

void Audio::setPitch(float pitch) {
    // Clamp pitch between 0.1 and 10.0
    if (pitch < 0.1f) pitch = 0.1f;
    if (pitch > 10.0f) pitch = 10.0f;
    sound.setPitch(pitch);
}

void Audio::setLoop(bool loop) {
    sound.setLoop(loop);
}

bool Audio::isPlaying() const {
    return sound.getStatus() == sf::Sound::Playing;
}

bool Audio::isPaused() const {
    return sound.getStatus() == sf::Sound::Paused;
}

bool Audio::isStopped() const {
    return sound.getStatus() == sf::Sound::Stopped;
}

bool Audio::isLoaded() const {
    return loaded;
}

void Audio::setPlayingOffset(float seconds) {
    sound.setPlayingOffset(sf::seconds(seconds));
}

float Audio::getPlayingOffset() const {
    return sound.getPlayingOffset().asSeconds();
}

float Audio::getDuration() const {
    return soundBuffer.getDuration().asSeconds();
}

float Audio::getVolume() const {
    return sound.getVolume();
}

float Audio::getPitch() const {
    return sound.getPitch();
}

bool Audio::getLoop() const {
    return sound.getLoop();
}