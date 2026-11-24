#include "Gravity.h"
#include <cmath>

namespace ml
{
namespace
{
constexpr double kStep = 0.001;     // 1 kHz physics
constexpr double kBallG = 3.0;
constexpr double kPendG = 4.9, kL = 0.5;

void wallReflect (double& p, double& v)
{
    if (p > 1.0) { p = 2.0 - p; v = -std::fabs (v); }
    if (p < -1.0) { p = -2.0 - p; v = std::fabs (v); }
    p = std::max (-1.0, std::min (1.0, p));
}

// double pendulum (equal masses and lengths): returns angular accelerations
void pendAcc (double t1, double t2, double w1, double w2, double& a1, double& a2)
{
    const double d = t1 - t2, g = kPendG, l = kL;
    const double den = 3.0 - std::cos (2.0 * d);
    a1 = (-3.0 * g * std::sin (t1) - g * std::sin (t1 - 2.0 * t2) - 2.0 * std::sin (d) * (w2 * w2 * l + w1 * w1 * l * std::cos (d))) / (l * den);
    a2 = (2.0 * std::sin (d) * (2.0 * w1 * w1 * l + 2.0 * g * std::cos (t1) + w2 * w2 * l * std::cos (d))) / (l * den);
}
} // namespace

void Body::resetState()
{
    const double k = index;
    rng.setSeed ((uint32_t) (index * 977 + 13));
    nx.seed ((uint32_t) (index * 31 + 5)); ny.seed ((uint32_t) (index * 57 + 11));
    vx = vy = w1 = w2 = 0;
    switch (type)
    {
        case bodyBall:     x = -0.7 + 0.45 * k; y = 0.9 - 0.2 * k; vx = (k == 1 || k == 3 ? -1 : 1) * (0.35 + 0.9 * energy); break;
        case bodyPendulum: th1 = (0.4 + 2.2 * energy) * (k == 1 || k == 3 ? -1 : 1); th2 = th1 * (1.0 + 0.6 * energy) + 0.05 * k; break;
        case bodySpring:   x = 0.75 * (k == 2 ? -1 : 1); y = 0.1 * k; vy = 0.9 + 0.2 * k; break;
        case bodyOrbit:    x = 0.6 - 0.05 * k; y = 0; vy = std::sqrt (1.0 / x) * (1.0 - 0.45 * energy) * (k == 1 || k == 3 ? -1 : 1); break;
        case bodyFirefly:  x = 0.3 * k - 0.45; y = -0.3 + 0.2 * k; break;
        default:           x = y = 0; break;
    }
    if (type == bodyPendulum)
    {
        x = kL * std::sin (th1) + kL * std::sin (th2);
        y = -(kL * std::cos (th1) + kL * std::cos (th2));
    }
    wasGrabbed = false;
    publish();
}

void Body::kick (float strength, uint32_t seed)
{
    const double a = (double) (hash32 (seed) & 0xffff) / 65536.0 * 2.0 * M_PI;
    if (type == bodySpring) { vx += std::cos (a) * strength * 1.6; vy += std::sin (a) * strength * 1.6; }
    else if (type == bodyFirefly) { vx += std::cos (a) * strength * 0.8; vy += std::sin (a) * strength * 0.8; }
    else if (type == bodyBall && y < -0.6) vy += strength * 1.2;
}

void Body::step (double dt)
{
    if (type == bodyOff) return;
    const bool g = grabbed.load (std::memory_order_relaxed);
    if (g)
    {
        const double gx = grabX.load (std::memory_order_relaxed), gy = grabY.load (std::memory_order_relaxed);
        const double nxp = x + (gx - x) * 0.25, nyp = y + (gy - y) * 0.25;
        const double rdt = 1.0 / std::max (1e-6, dt);
        vx = vx * 0.9 + 0.1 * (nxp - x) * rdt;
        vy = vy * 0.9 + 0.1 * (nyp - y) * rdt;
        x = nxp; y = nyp;
        if (type == bodyPendulum)
        {
            const double a = std::atan2 (x, -y);
            const double na = a;
            w1 = w1 * 0.9 + 0.1 * (na - th1) * rdt; w2 = w1;
            th1 = th2 = na;
        }
        wasGrabbed = true;
        return;
    }
    if (wasGrabbed)
    {
        wasGrabbed = false;
        const double vmax = 8.0;
        vx = std::max (-vmax, std::min (vmax, vx)); vy = std::max (-vmax, std::min (vmax, vy));
        w1 = std::max (-15.0, std::min (15.0, w1)); w2 = w1;
    }

    switch (type)
    {
        case bodyBall:
        {
            vy -= kBallG * dt;
            x += vx * dt; y += vy * dt;
            if (y < -1.0)
            {
                y = -2.0 - y;
                const double h = 0.35 + 1.6 * energy;     // bounce height (box is 2 tall)
                vy = std::sqrt (2.0 * kBallG * h) * (0.97 + 0.06 * rng.uni());
            }
            if (y > 1.0) { y = 2.0 - y; vy = -std::fabs (vy); }
            if (x > 1.0 || x < -1.0) wallReflect (x, vx);
            const double target = 0.3 + 1.1 * energy;
            const double sp = std::fabs (vx);
            vx *= 1.0 + (target - sp) * 0.2 * dt;
            if (std::fabs (vx) < 0.05) vx = vx < 0 ? -0.05 : 0.05;
            break;
        }
        case bodyPendulum:
        {
            // RK4
            auto f = [] (double t1, double t2, double a, double b, double& d1, double& d2) { pendAcc (t1, t2, a, b, d1, d2); };
            double k1a, k1b, k2a, k2b, k3a, k3b, k4a, k4b;
            f (th1, th2, w1, w2, k1a, k1b);
            f (th1 + 0.5 * dt * w1, th2 + 0.5 * dt * w2, w1 + 0.5 * dt * k1a, w2 + 0.5 * dt * k1b, k2a, k2b);
            const double w1b = w1 + 0.5 * dt * k1a, w2b = w2 + 0.5 * dt * k1b;
            f (th1 + 0.5 * dt * w1b, th2 + 0.5 * dt * w2b, w1 + 0.5 * dt * k2a, w2 + 0.5 * dt * k2b, k3a, k3b);
            const double w1c = w1 + 0.5 * dt * k2a, w2c = w2 + 0.5 * dt * k2b;
            f (th1 + dt * w1c, th2 + dt * w2c, w1 + dt * k3a, w2 + dt * k3b, k4a, k4b);
            const double w1d = w1 + dt * k3a, w2d = w2 + dt * k3b;
            th1 += dt / 6.0 * (w1 + 2 * w1b + 2 * w1c + w1d);
            th2 += dt / 6.0 * (w2 + 2 * w2b + 2 * w2c + w2d);
            w1 += dt / 6.0 * (k1a + 2 * k2a + 2 * k3a + k4a);
            w2 += dt / 6.0 * (k1b + 2 * k2b + 2 * k3b + k4b);
            // keep it swinging: tiny drive at low energy, gentle loss after a fling
            const double damp = 1.0 - 0.02 * dt;
            w1 *= damp; w2 *= damp;
            const double e = std::fabs (w1) + std::fabs (th1);
            if (e < 0.3 + 1.5 * energy) w1 += (w1 >= 0 ? 1 : -1) * 0.15 * dt;
            th1 = std::remainder (th1, 2 * M_PI); th2 = std::remainder (th2, 2 * M_PI);
            x = kL * std::sin (th1) + kL * std::sin (th2);
            y = -(kL * std::cos (th1) + kL * std::cos (th2));
            break;
        }
        case bodySpring:
        {
            const double kx = 4.8, ky = 4.8 * 2.618;
            const double c = 0.05 + 1.6 * (1.0 - energy) * (1.0 - energy);
            const double nf = energy * 0.6;
            const double ax = -kx * x - c * vx + nf * nx.next ((float) (dt * 0.9));
            const double ay = -ky * y - c * vy + nf * ny.next ((float) (dt * 0.9));
            vx += ax * dt; vy += ay * dt;
            x += vx * dt; y += vy * dt;
            wallReflect (x, vx); wallReflect (y, vy);
            break;
        }
        case bodyOrbit:
        {
            double ax = 0, ay = 0;
            auto pull = [&] (double sx, double sy, double gm)
            {
                const double dx = sx - x, dy = sy - y;
                const double r2 = dx * dx + dy * dy + 0.015;
                const double inv = gm / (r2 * std::sqrt (r2));
                ax += dx * inv; ay += dy * inv;
            };
            if (energy > 0.55)
            {
                // two suns slowly circling: chaotic orbits
                phaseSun += dt * 0.35;
                const double s = 0.18 + 0.3 * (energy - 0.55);
                pull (s * std::cos (phaseSun), s * std::sin (phaseSun), 0.55);
                pull (-s * std::cos (phaseSun), -s * std::sin (phaseSun), 0.55);
            }
            else pull (0, 0, 1.0);
            vx += ax * dt; vy += ay * dt;
            const double v = std::sqrt (vx * vx + vy * vy);
            if (v > 6.0) { vx *= 6.0 / v; vy *= 6.0 / v; }
            x += vx * dt; y += vy * dt;
            if (std::fabs (x) > 1.0) { wallReflect (x, vx); vx *= 0.8; }
            if (std::fabs (y) > 1.0) { wallReflect (y, vy); vy *= 0.8; }
            break;
        }
        case bodyFirefly:
        {
            const double a = 1.5 + 9.0 * energy;
            const double rate = 0.4 + 1.2 * energy;
            const double ax = a * nx.next ((float) (dt * rate)) - 1.1 * vx - 0.6 * x;
            const double ay = a * ny.next ((float) (dt * rate)) - 1.1 * vy - 0.6 * y;
            vx += ax * dt; vy += ay * dt;
            x += vx * dt; y += vy * dt;
            wallReflect (x, vx); wallReflect (y, vy);
            break;
        }
        default: break;
    }
}

void Body::publish()
{
    outX.store ((float) std::max (-1.0, std::min (1.0, x)), std::memory_order_relaxed);
    outY.store ((float) std::max (-1.0, std::min (1.0, y)), std::memory_order_relaxed);
    outEx.store ((float) (kL * std::sin (th1)), std::memory_order_relaxed);
    outEy.store ((float) (-kL * std::cos (th1)), std::memory_order_relaxed);
}

Gravity::Gravity()
{
    for (int i = 0; i < 4; ++i) { bodies[i].index = i; bodies[i].resetState(); }
}

void Gravity::setParams (int i, int type, float speed, float energy)
{
    auto& b = bodies[i];
    const bool changed = b.type != type;
    b.type = type; b.speed = speed; b.energy = energy;
    if (changed) b.resetState();
}

void Gravity::reset()
{
    for (auto& b : bodies) { b.phaseSun = 0; b.resetState(); }
    acc = 0;
}

void Gravity::noteOn (int note, float velocity)
{
    for (auto& b : bodies)
        if (b.type != bodyOff)
            b.kick (velocity * (0.3f + 0.7f * b.energy), (uint32_t) (note * 131 + b.index * 7));
}

void Gravity::process (int numSamples, double sr)
{
    acc += numSamples / sr;
    int steps = 0;
    while (acc >= kStep && steps < 400)
    {
        for (auto& b : bodies) b.step (kStep * b.speed);
        acc -= kStep; ++steps;
    }
    if (steps == 400) acc = 0;
    for (auto& b : bodies) b.publish();
}
} // namespace ml
