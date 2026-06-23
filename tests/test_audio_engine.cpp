#include <gtest/gtest.h>
#include "engine/animation/audio_engine.hpp"

using namespace fap;

TEST(AudioTrackTest, DefaultConstruction) {
    AudioTrack track("Music");
    EXPECT_EQ(track.name(), "Music");
    EXPECT_FALSE(track.muted());
    EXPECT_FALSE(track.solo());
    EXPECT_FLOAT_EQ(track.volume(), 1.0f);
}

TEST(AudioTrackTest, MuteSolo) {
    AudioTrack track("FX");
    track.setMuted(true);
    EXPECT_TRUE(track.muted());
    track.setSolo(true);
    EXPECT_TRUE(track.solo());
}

TEST(AudioTrackTest, VolumeClamp) {
    AudioTrack track("Test");
    track.setVolume(5.0f);
    EXPECT_FLOAT_EQ(track.volume(), 2.0f);
    track.setVolume(-1.0f);
    EXPECT_FLOAT_EQ(track.volume(), 0.0f);
}

TEST(AudioTrackTest, AddRemoveClips) {
    AudioTrack track("Track");
    AudioClip clip;
    clip.filepath = "audio.wav";
    clip.duration = 3.0f;

    track.addClip(clip);
    EXPECT_EQ(track.clipCount(), 1);
    EXPECT_EQ(track.clips()[0].filepath, "audio.wav");

    track.removeClip(0);
    EXPECT_EQ(track.clipCount(), 0);
}

TEST(AudioTrackTest, Keyframes) {
    AudioTrack track("Track");
    AudioKeyframe kf1{0, 1.0f, 0.0f};
    AudioKeyframe kf2{10, 0.5f, 0.5f};

    track.addKeyframe(kf1);
    track.addKeyframe(kf2);
    EXPECT_EQ(track.keyframes().size(), 2);

    EXPECT_FLOAT_EQ(track.volumeAtFrame(0), 1.0f);
    EXPECT_FLOAT_EQ(track.volumeAtFrame(10), 0.5f);
    EXPECT_FLOAT_EQ(track.volumeAtFrame(5), 0.75f);
}

TEST(AudioTrackTest, VolumeBeforeFirstKeyframe) {
    AudioTrack track("Track");
    track.setVolume(0.8f);
    track.addKeyframe({5, 0.2f, 0.0f});

    EXPECT_FLOAT_EQ(track.volumeAtFrame(0), 0.2f);
}

TEST(AudioEngineTest, DefaultState) {
    AudioEngine engine;
    EXPECT_EQ(engine.sampleRate(), 44100);
    EXPECT_EQ(engine.fps(), 24);
    EXPECT_FALSE(engine.isPlaying());
    EXPECT_FLOAT_EQ(engine.masterVolume(), 1.0f);
}

TEST(AudioEngineTest, AddRemoveTracks) {
    AudioEngine engine;
    engine.addTrack(std::make_unique<AudioTrack>("Track1"));
    engine.addTrack(std::make_unique<AudioTrack>("Track2"));

    EXPECT_EQ(engine.trackCount(), 2);
    EXPECT_NE(engine.trackAt(0), nullptr);
    EXPECT_NE(engine.trackAt(1), nullptr);
    EXPECT_EQ(engine.trackAt(5), nullptr);

    engine.removeTrack(0);
    EXPECT_EQ(engine.trackCount(), 1);
}

TEST(AudioEngineTest, TimeFrameConversion) {
    AudioEngine engine(44100);
    engine.setFPS(24);

    EXPECT_FLOAT_EQ(engine.timeForFrame(0), 0.0f);
    EXPECT_FLOAT_EQ(engine.timeForFrame(24), 1.0f);
    EXPECT_FLOAT_EQ(engine.timeForFrame(12), 0.5f);
}

TEST(AudioEngineTest, DurationCalculation) {
    AudioEngine engine;
    engine.setFPS(30);

    AudioClip clip;
    clip.duration = 2.0f;

    auto track = std::make_unique<AudioTrack>("Track");
    track->addClip(clip);
    engine.addTrack(std::move(track));

    EXPECT_FLOAT_EQ(engine.durationSeconds(), 2.0f);
    EXPECT_EQ(engine.durationFrames(), 60);
}

TEST(AudioEngineTest, MasterVolume) {
    AudioEngine engine;
    engine.setMasterVolume(0.5f);
    EXPECT_FLOAT_EQ(engine.masterVolume(), 0.5f);
    engine.setMasterVolume(-0.5f);
    EXPECT_FLOAT_EQ(engine.masterVolume(), 0.0f);
    engine.setMasterVolume(1.5f);
    EXPECT_FLOAT_EQ(engine.masterVolume(), 1.0f);
}

TEST(AudioEngineTest, PlayPauseStop) {
    AudioEngine engine;
    EXPECT_FALSE(engine.isPlaying());

    engine.play();
    EXPECT_TRUE(engine.isPlaying());

    engine.pause();
    EXPECT_FALSE(engine.isPlaying());

    engine.play();
    EXPECT_TRUE(engine.isPlaying());

    engine.stop();
    EXPECT_FALSE(engine.isPlaying());
}

TEST(AudioEngineTest, FrameCallback) {
    AudioEngine engine;
    int calledFrame = -1;
    float calledTime = -1.0f;

    engine.setFrameCallback([&](int frame, float time) {
        calledFrame = frame;
        calledTime = time;
    });

    engine.seekToFrame(12);
    EXPECT_EQ(calledFrame, 12);
    EXPECT_NEAR(calledTime, 0.5f, 0.01f);
}
