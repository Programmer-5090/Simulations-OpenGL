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

    bool load(const std::string& filename);
    
    void play();
    void pause();
    void stop();
    
    void setVolume(float volume);      // 0.0f to 100.0f
    void setPitch(float pitch);        // 0.1f to 10.0f (1.0f = normal)
    void setLoop(bool loop);
    
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    bool isLoaded() const;
    
    void setPlayingOffset(float seconds);
    float getPlayingOffset() const;
    float getDuration() const;
    
    float getVolume() const;
    float getPitch() const;
    bool getLoop() const;
};