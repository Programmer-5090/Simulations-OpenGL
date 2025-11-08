#pragma once
#include <SFML/Audio.hpp>
#include <string>
#include <memory>

class Audio {
private:
    sf::SoundBuffer soundBuffer;
    sf::Sound sound;
    bool loaded = false;

public:
    Audio();
    ~Audio();

    // Load audio from file
    bool load(const std::string& filename);
    
    // Playback controls
    void play();
    void pause();
    void stop();
    
    // Audio properties
    void setVolume(float volume);      // 0.0f to 100.0f
    void setPitch(float pitch);        // 0.1f to 10.0f (1.0f = normal)
    void setLoop(bool loop);
    
    // Status checks
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    bool isLoaded() const;
    
    // Position controls
    void setPlayingOffset(float seconds);
    float getPlayingOffset() const;
    float getDuration() const;
    
    // Getters
    float getVolume() const;
    float getPitch() const;
    bool getLoop() const;
};