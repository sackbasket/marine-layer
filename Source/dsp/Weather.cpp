#include "Weather.h"
#include <algorithm>

namespace ml
{
namespace
{
inline float hashf (int x, int y, int z)
{
    uint32_t h = hash32 ((uint32_t) x * 73856093u ^ (uint32_t) y * 19349663u ^ (uint32_t) z * 83492791u);
    return (float) (h & 0xffffff) / 16777216.0f * 2.0f - 1.0f;
}
inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
// 3D value noise (x, y, time)
float vnoise (float x, float y, float t)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y), ti = (int) std::floor (t);
    const float fx = smooth (x - xi), fy = smooth (y - yi), ft = smooth (t - ti);
    auto layer = [&] (int tt)
    {
        const float a = hashf (xi, yi, tt), b = hashf (xi + 1, yi, tt), c = hashf (xi, yi + 1, tt), d = hashf (xi + 1, yi + 1, tt);
        return lerp (lerp (a, b, fx), lerp (c, d, fx), fy);
    };
    return lerp (layer (ti), layer (ti + 1), ft);
}
} // namespace

Weather::Weather() { reset (1); }

void Weather::reset (uint32_t seed)
{
    rng.setSeed (seed * 7919u + 3);
    lifeA.seed (seed * 3 + 1); lifeB.seed (seed * 5 + 2);
    for (auto& p : parts) p = Particle {};
    for (auto& s : strokes) s = Stroke {};
    acc = 0; simTime = 0; spawnAcc = 0; nextId = 0; emitterTurn = 0;
    hvx = hvy = 0; pendingBurst = 0; pendingNoteCount = 0;
    numEvents = 0;
    publish();
}

void Weather::addStroke (const Stroke& s)
{
    int slot = 0; float least = 1e9f;
    for (int i = 0; i < maxStrokes; ++i) if (strokes[i].ttl < least) { least = strokes[i].ttl; slot = i; }
    strokes[slot] = s;
}

void Weather::burst (float x, float y, float strength) { pendingBurstX = x; pendingBurstY = y; pendingBurst = strength; }
void Weather::spawnNote (float semis, int count) { pendingNote = semis; pendingNoteCount = count; }

float Weather::psi (float x, float y, float t) const
{
    return vnoise (x * 3.0f, y * 1.6f, t * 0.35f) + 0.5f * vnoise (x * 7.0f + 11.0f, y * 3.5f + 3.0f, t * 0.7f);
}

void Weather::spawn (const WeatherParams& p, int src)
{
    Particle* slot = nullptr;
    for (auto& q : parts) if (! q.alive) { slot = &q; break; }
    if (! slot) return;
    auto& q = *slot;
    q = Particle {};
    q.alive = true;
    q.src = (uint8_t) src;
    q.z = 0.35f + 0.65f * rng.uni();
    q.life = clampf (1.6f + p.sizeSec * 16.0f, 1.6f, 14.0f) * (0.6f + 0.8f * rng.uni());
    q.phase = rng.uni();
    q.jitter = 0.7f + 0.6f * rng.uni();
    q.side = (nextId++ & 1) ? 1 : -1;
    if (src == 0)
    {
        // from the record head (the "now" edge on the left)
        q.x = 0.004f + 0.03f * rng.uni();
        const float g = (rng.bi() + rng.bi() + rng.bi()) / 3.0f;
        q.y = clampf (g * 0.9f, -0.95f, 0.95f);
        q.vx = 0.15f + 0.25f * rng.uni();
        q.vy = 0.15f * rng.bi();
    }
    else
    {
        const int e = src - 1;
        q.x = clampf (emX[e] + 0.012f * rng.bi(), 0.0f, 1.0f);
        q.y = clampf (emY[e] + 0.03f * rng.bi(), -1.0f, 1.0f);
        const float a = kPi * 0.5f + 1.1f * rng.bi();
        q.vx = 0.35f * std::cos (a); q.vy = 0.45f * std::sin (a);
    }
}

void Weather::step (double dtd, const WeatherParams& p, const Gravity& g, int offset)
{
    const float dt = (float) dtd;
    simTime += dtd;
    const float t = (float) simTime;

    // ---- spawning: keep the population near its target
    const float target = std::min ((float) maxParticles - 8.0f, 6.0f + std::pow (clampf (p.humidity, 0.0f, 1.6f), 1.1f) * 190.0f);
    int alive = 0;
    for (auto& q : parts) alive += q.alive ? 1 : 0;
    const float avgLife = clampf (1.6f + p.sizeSec * 16.0f, 1.6f, 14.0f);
    spawnAcc += dt * target / avgLife * (alive < target ? 1.6f : 0.35f);
    int numEm = 0; for (int i = 0; i < maxEmitters; ++i) numEm += emOn[i] ? 1 : 0;
    while (spawnAcc >= 1.0f)
    {
        spawnAcc -= 1.0f;
        int src = 0;
        if (numEm > 0 && rng.uni() < 0.55f)
        {
            for (int k = 0; k < maxEmitters; ++k)
            {
                emitterTurn = (emitterTurn + 1) % maxEmitters;
                if (emOn[emitterTurn]) { src = emitterTurn + 1; break; }
            }
        }
        spawn (p, src);
    }
    if (pendingNoteCount > 0)
    {
        for (int k = 0; k < pendingNoteCount; ++k)
        {
            spawn (p, 0);
            for (auto& q : parts)
                if (q.alive && q.age == 0 && std::isnan (q.pitchOv)) { q.pitchOv = pendingNote; q.phase = 0.999f - 0.05f * (float) k; q.x = 0.02f + 0.05f * rng.uni(); break; }
        }
        pendingNoteCount = 0;
    }

    // ---- hand
    const bool handDown = std::fabs (p.handPull) > 0.001f || p.handGust > 0.001f;
    const float hx = p.handX, hy = p.handY;
    const float rawVx = (hx - prevHX) / dt * 2.0f, rawVy = (hy - prevHY) / dt;
    prevHX = hx; prevHY = hy;
    const float hs = 1.0f - std::exp (-dt / 0.05f);
    hvx += (clampf (rawVx, -12.0f, 12.0f) - hvx) * hs;
    hvy += (clampf (rawVy, -12.0f, 12.0f) - hvy) * hs;

    // ---- wind (+ Life wandering)
    const float lifeTurn = p.lifeAmt * lifeA.next (dt * 0.08f) * 1.4f;
    const float lifeGust = p.lifeAmt * (0.5f + 0.5f * lifeB.next (dt * 0.11f)) * 0.25f;
    const float dir = p.windDir * kPi / 180.0f + lifeTurn;
    const float ws = p.wind * 1.2f + lifeGust;
    const float wvx = ws * std::cos (dir), wvy = ws * std::sin (dir);
    // the fog rolls from "now" towards the past on its own, across Reach over a lifetime
    const float drift = p.reach / avgLife * 2.0f * p.drift;

    float bx[4], by[4], bvx[4], bvy[4];
    bool bon[4];
    for (int i = 0; i < 4; ++i)
    {
        const auto& b = g.bodies[i];
        bon[i] = b.type != bodyOff;
        bx[i] = (b.outX.load (std::memory_order_relaxed) + 1.0f) * 0.5f;
        by[i] = b.outY.load (std::memory_order_relaxed);
        bvx[i] = (float) b.vx; bvy[i] = (float) b.vy;
    }

    const float damp = std::exp (-0.9f * dt);
    for (auto& q : parts)
    {
        if (! q.alive) continue;
        q.age += dt;
        if (q.age > q.life || q.x > 1.02f) { q.alive = false; continue; }

        float ax = 0, ay = 0;
        // wind relaxes velocity towards the wind
        ax += (wvx - q.vx) * 0.8f * p.wind;
        ay += (wvy - q.vy) * 0.8f * p.wind;
        // turbulence: curl of a moving noise field
        if (p.turb > 0.001f)
        {
            const float e = 0.01f;
            const float dpdy = (psi (q.x, q.y + e, t) - psi (q.x, q.y - e, t)) / (2 * e);
            const float dpdx = (psi (q.x + e, q.y, t) - psi (q.x - e, q.y, t)) / (2 * e);
            ax += p.turb * 2.2f * dpdy * 0.5f;
            ay += -p.turb * 2.2f * dpdx * 0.25f;
        }
        // gravity sinks (pitches fall), warm young air rises
        ay -= p.gravity * 1.8f;
        ay += p.temp * 1.8f * (1.0f - q.age / q.life);
        // gravity bodies: attraction + collisions
        if (p.bodyForce > 0.001f)
            for (int i = 0; i < 4; ++i)
            {
                if (! bon[i]) continue;
                const float dx = (bx[i] - q.x) * 2.0f, dy = by[i] - q.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > 0.5f) continue;
                const float d = std::sqrt (d2) + 1e-4f;
                if (d < 0.09f)
                {
                    // bounce off and pick up some of the body's motion
                    ax += -dx / d * 30.0f * p.bodyForce + bvx[i] * 4.0f * p.bodyForce;
                    ay += -dy / d * 30.0f * p.bodyForce + bvy[i] * 4.0f * p.bodyForce;
                    q.flash = std::max (q.flash, 0.6f);
                }
                else
                {
                    const float f = p.bodyForce * 0.9f / (d2 + 0.02f) * (1.0f - d2 / 0.5f);
                    ax += dx / d * f; ay += dy / d * f;
                }
            }
        // the hand: pull / push, drag speed = gust
        if (handDown)
        {
            const float dx = (hx - q.x) * 2.0f, dy = hy - q.y;
            const float d2 = dx * dx + dy * dy;
            const float d = std::sqrt (d2) + 1e-4f;
            if (std::fabs (p.handPull) > 0.001f && d < 1.2f)
            {
                const float f = p.handPull * 5.0f / (d + 0.12f) * (1.0f - d / 1.2f);
                ax += dx / d * f; ay += dy / d * f;
                if (p.handPull > 0 && d < 0.12f) { ax -= q.vx * 6.0f; ay -= q.vy * 6.0f; }   // settle in the palm
            }
            const float gw = std::exp (-d2 / 0.06f) * (0.4f + 2.5f * p.handGust);
            ax += hvx * gw * 6.0f; ay += hvy * gw * 6.0f;
        }
        // painted wind currents
        for (auto& s : strokes)
        {
            if (s.ttl <= 0 || s.n < 2) continue;
            float best = 0.12f; float tx = 0, ty = 0;
            for (int k = 0; k + 1 < s.n; ++k)
            {
                const float sx = (s.x[k + 1] - s.x[k]) * 2.0f, sy = s.y[k + 1] - s.y[k];
                const float L2 = sx * sx + sy * sy;
                if (L2 < 1e-8f) continue;
                const float px = (q.x - s.x[k]) * 2.0f, py = q.y - s.y[k];
                const float u = clamp01 ((px * sx + py * sy) / L2);
                const float ex = px - u * sx, ey = py - u * sy;
                const float dd = std::sqrt (ex * ex + ey * ey);
                if (dd < best) { best = dd; const float L = std::sqrt (L2); tx = sx / L; ty = sy / L; }
            }
            if (best < 0.12f)
            {
                const float k = (1.0f - best / 0.12f) * std::min (1.0f, s.ttl);
                const float sp = 1.6f;
                ax += (tx * sp - q.vx) * 6.0f * k;
                ay += (ty * sp - q.vy) * 6.0f * k;
            }
        }

        q.vx = (q.vx + ax * dt) * damp;
        q.vy = (q.vy + ay * dt) * damp;
        const float v2 = q.vx * q.vx + q.vy * q.vy;
        if (v2 > 16.0f) { const float s = 4.0f / std::sqrt (v2); q.vx *= s; q.vy *= s; }
        q.x += (q.vx * 0.5f + drift) * dt;
        q.y += q.vy * dt;
        if (q.y > 1.0f) { q.y = 2.0f - q.y; q.vy = -std::fabs (q.vy) * 0.6f; }
        if (q.y < -1.0f) { q.y = -2.0f - q.y; q.vy = std::fabs (q.vy) * 0.6f; }
        if (q.x < 0.0f) { q.x = -q.x; q.vx = std::fabs (q.vx) * 0.6f; }
        // beyond Reach the fog thins out faster
        if (q.x > p.reach) q.age += dt * 3.0f * (q.x - p.reach) / std::max (0.05f, 1.0f - p.reach) * 4.0f;
        q.flash *= 0.9f;

        // rain grains
        const float interval = std::max (0.012f, p.sizeSec * (0.5f + q.z) * q.jitter * 2.4f / std::max (0.2f, p.overlap));
        q.phase += dt / interval;
        if (q.phase >= 1.0f)
        {
            q.phase -= std::floor (q.phase);
            if (numEvents < maxEvents)
            {
                auto& ev = events[numEvents++];
                ev.x = q.x; ev.y = q.y; ev.z = q.z;
                ev.bright = clampf (0.6f + 0.5f * std::sqrt (q.vx * q.vx + q.vy * q.vy), 0.4f, 1.6f);
                const float r = clamp01 (std::sqrt ((q.x - 0.5f * p.reach) * (q.x - 0.5f * p.reach) * 4.0f + q.y * q.y) / 1.3f);
                ev.pan = (float) q.side * (0.15f + 0.85f * r);
                ev.offset = offset;
                ev.particle = (int) (&q - parts);
                ev.pitchOverride = q.pitchOv;
                q.flash = 1.0f;
            }
        }
    }

    // scatter burst (flick)
    if (pendingBurst > 0)
    {
        for (auto& q : parts)
        {
            if (! q.alive) continue;
            const float dx = (q.x - pendingBurstX) * 2.0f, dy = q.y - pendingBurstY;
            const float d = std::sqrt (dx * dx + dy * dy) + 1e-3f;
            if (d < 0.7f)
            {
                const float f = pendingBurst * 3.5f * (1.0f - d / 0.7f);
                q.vx += dx / d * f; q.vy += dy / d * f; q.flash = 1.0f;
            }
        }
        pendingBurst = 0;
    }
    for (auto& s : strokes) if (s.ttl > 0) s.ttl -= dt;
}

int Weather::process (int numSamples, double sr, const WeatherParams& p, const Gravity& g)
{
    numEvents = 0;
    // acc = time since the last step; steps fall at stepSec - acc, + stepSec, ... inside this block
    const double blockSec = numSamples / sr;
    double tNext = stepSec - acc;
    int steps = 0;
    while (tNext <= blockSec && steps < 64)
    {
        const int offset = std::min (numSamples - 1, std::max (0, (int) (tNext * sr)));
        step (stepSec, p, g, offset);
        tNext += stepSec; ++steps;
    }
    acc = stepSec - (tNext - blockSec);
    if (acc < 0 || acc >= stepSec) acc = 0;
    publish();
    grainsFired.fetch_add (numEvents, std::memory_order_relaxed);
    return numEvents;
}

void Weather::publish()
{
    const int w = 1 - visIndex.load (std::memory_order_relaxed);
    auto& v = vis[w];
    int n = 0;
    for (auto& q : parts)
        if (q.alive)
            v.p[n++] = { q.x, q.y, q.z, q.flash, clamp01 (1.0f - q.age / q.life), q.src };
    v.n = n;
    for (int i = 0; i < maxStrokes; ++i)
    {
        v.strokeN[i] = strokes[i].ttl > 0 ? strokes[i].n : 0;
        v.strokeTtl[i] = strokes[i].ttl;
        for (int k = 0; k < strokes[i].n; ++k) { v.strokeX[i][k] = strokes[i].x[k]; v.strokeY[i][k] = strokes[i].y[k]; }
    }
    visIndex.store (w, std::memory_order_release);
}
} // namespace ml
