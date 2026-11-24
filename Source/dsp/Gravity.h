// Gravity bodies (shared with LA): four little physics objects living in a box.
// Their positions (x, y in -1..1) are modulation sources. The UI can grab a
// body and fling it; the simulation is deterministic (fixed 1 kHz step) and
// restarts from the same state whenever the host transport starts.
#pragma once
#include "DspUtil.h"
#include <atomic>

namespace ml
{
enum BodyType { bodyOff, bodyBall, bodyPendulum, bodySpring, bodyOrbit, bodyFirefly };

struct Body
{
    int type = bodyOff;
    float speed = 1, energy = 0.5f;
    int index = 0;

    // state
    double x = 0, y = 0, vx = 0, vy = 0;
    double th1 = 0, th2 = 0, w1 = 0, w2 = 0;   // pendulum angles
    double phaseSun = 0;                       // orbit: binary suns
    SmoothNoise nx, ny;
    Rng rng;

    // UI interaction (written by the UI thread)
    std::atomic<bool> grabbed { false };
    std::atomic<float> grabX { 0 }, grabY { 0 };
    // published for the UI and the engine
    std::atomic<float> outX { 0 }, outY { 0 }, outEx { 0 }, outEy { 0 };   // tip + pendulum elbow
    double prevGX = 0, prevGY = 0;
    bool wasGrabbed = false;

    void resetState();
    void step (double dt);          // dt in seconds (already scaled by speed)
    void kick (float strength, uint32_t seed);   // note-on impulse
    void publish();
};

class Gravity
{
public:
    Gravity();
    void setParams (int i, int type, float speed, float energy);
    void reset();
    void process (int numSamples, double sampleRate);
    void noteOn (int note, float velocity);
    float x (int i) const { return bodies[i].outX.load (std::memory_order_relaxed); }
    float y (int i) const { return bodies[i].outY.load (std::memory_order_relaxed); }
    Body bodies[4];
private:
    double acc = 0;
};
} // namespace ml
