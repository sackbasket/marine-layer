#include "Fx.h"

// Reverb and Smog are shared with LA (same author); the reverb runs on the cloud only.
namespace ml
{
// =============================================================================== Reverb (8-line FDN)
void Reverb::prepare (double s)
{
    sr = s;
    static const float lens[N] = { 1201, 1453, 1709, 1999, 2297, 2593, 2909, 3229 };   // at 48 kHz, size 1
    const float k = (float) (sr / 48000.0);
    for (int i = 0; i < N; ++i) { baseLen[i] = lens[i] * k; line[i].init ((int) (lens[i] * k * 2.2f) + 64); }
    for (auto& p : pre) p.init ((int) (sr * 0.26) + 4);
    static const float aps[4] = { 142, 107, 379, 277 };
    for (int c = 0; c < 2; ++c) for (int j = 0; j < 4; ++j) { apLen[j] = aps[j] * k * (c ? 1.07f : 1.0f); ap[c][j].init ((int) (aps[j] * k * 1.2f) + 8); }
    shim.init ((int) (sr * 0.2));
    reset();
}

void Reverb::reset()
{
    for (auto& x : line) x.clear();
    for (auto& x : pre) x.clear();
    for (auto& c : ap) for (auto& x : c) x.clear();
    shim.clear();
    for (auto& v : damp) v = 0;
    modPhase = 0; shimPhase = 0; shimLp.reset(); shimHp.reset();
}

void Reverb::process (float* l, float* r, int n, float size, float decay, float tone, float shimmer, float motion, float preMs, float mix)
{
    mixR.set (mix, n); shimR.set (shimmer, n);
    const float sizeMul = 0.35f + 1.65f * size;
    float len[N], g[N];
    for (int i = 0; i < N; ++i)
    {
        len[i] = baseLen[i] * sizeMul;
        g[i] = std::pow (10.0f, -3.0f * len[i] / ((float) sr * std::max (0.1f, decay)));
    }
    const float dampHz = 1800.0f * std::exp2 (tone * 2.4f + 1.0f);
    const float da = std::exp (-kTwoPi * dampHz / (float) sr);
    shimLp.setCutoff (7000.0f, (float) sr); shimHp.setCutoff (250.0f, (float) sr);
    const float preD = std::max (1.0f, preMs * 0.001f * (float) sr);
    const float modDepth = motion * 14.0f * (float) (sr / 48000.0);
    const double modInc = 0.21 / sr;
    const float win = (float) (sr * 0.08);
    const float apG = 0.6f;

    for (int i = 0; i < n; ++i)
    {
        const float m = mixR.next(), sh = shimR.next();
        pre[0].push (l[i]); pre[1].push (r[i]);
        float in[2] = { pre[0].read (preD), pre[1].read (preD) };
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 4; ++j)
            {
                const float dl = ap[c][j].readInt ((int) apLen[j]);
                const float v = in[c] + apG * dl;
                ap[c][j].push (v);
                in[c] = dl - apG * v;
            }

        // read lines (modulated)
        float y[N];
        for (int k = 0; k < N; ++k)
        {
            const float ph = (float) (modPhase * (1.0 + k * 0.137)) + k * 0.25f;
            const float mod = modDepth * sin01 (ph - std::floor (ph));
            y[k] = line[k].read (len[k] + mod + modDepth + 2.0f);
            damp[k] = y[k] + da * (damp[k] - y[k]);
            y[k] = damp[k] * g[k];
        }
        // shimmer: octave-up pitch shift of the tail fed back in
        float shimOut = 0;
        if (sh > 0.0005f)
        {
            shim.push ((y[0] + y[3] + y[5] + y[6]) * 0.5f);
            shimPhase += 1.0f / win;   // reading twice as fast = +12 st
            if (shimPhase >= 1.0f) shimPhase -= 1.0f;
            const float p2 = shimPhase + 0.5f >= 1.0f ? shimPhase - 0.5f : shimPhase + 0.5f;
            const float a = shim.read (2.0f + (1.0f - shimPhase) * win), b = shim.read (2.0f + (1.0f - p2) * win);
            const float wa = sin01 (shimPhase * 0.5f), wb = sin01 (p2 * 0.5f);
            shimOut = shimHp.hp (shimLp.lp (a * wa + b * wb)) * sh * 0.9f;
        }
        // Hadamard mix (fast Walsh-Hadamard, normalised)
        float h[N];
        for (int k = 0; k < N; ++k) h[k] = y[k];
        for (int len2 = 1; len2 < N; len2 <<= 1)
            for (int a = 0; a < N; a += len2 * 2)
                for (int b = a; b < a + len2; ++b) { const float u = h[b], v = h[b + len2]; h[b] = u + v; h[b + len2] = u - v; }
        const float norm = 0.35355339f;
        for (int k = 0; k < N; ++k)
        {
            const float inj = (k & 1) ? in[1] : in[0];
            line[k].push (h[k] * norm + inj * 0.5f + shimOut * ((k & 2) ? 1.0f : -1.0f));
        }
        const float wl = (y[0] - y[2] + y[4] - y[6] + y[1] * 0.5f) * 0.45f;
        const float wr = (y[1] - y[3] + y[5] - y[7] + y[2] * 0.5f) * 0.45f;
        l[i] = l[i] * (1.0f - 0.45f * m * m) + wl * m;
        r[i] = r[i] * (1.0f - 0.45f * m * m) + wr * m;
        modPhase += modInc; if (modPhase > 1000.0) modPhase -= 1000.0;
    }
}

// =============================================================================== Smog
void Smog::prepare (double s)
{
    sr = s;
    for (auto& x : d) x.init ((int) (sr * 0.03) + 8);
    reset();
}

void Smog::reseed (uint32_t seed)
{
    wow.seed (seed); flut.seed (seed * 3 + 1); rng.setSeed (seed * 17 + 3);
    flutPhase = 0; dropGain = dropTarget = 1; dropHold = 0;
}

void Smog::reset (uint32_t seed)
{
    for (auto& x : d) x.clear();
    wow.seed (seed); flut.seed (seed * 3 + 1); rng.setSeed (seed * 17 + 3);
    flutPhase = 0; holdL = holdR = holdPhase = 0;
    for (int c = 0; c < 2; ++c) { lp1[c].reset(); lp2[c].reset(); hp[c].reset(); hissLp[c].reset(); dropLp[c].reset(); }
    crackle = crackleEnv = 0; dropGain = dropTarget = 1; dropHold = 0;
    for (auto& c : pinkB) for (auto& v : c) v = 0;
}

void Smog::process (float* l, float* r, int n, float amount, float wobble, float crush, float dust, float drop)
{
    amtR.set (amount, n);
    const float fs = (float) sr;
    const float baseDelay = fs * 0.008f;
    for (int i = 0; i < n; ++i)
    {
        const float a = amtR.next();
        const float aw = a * wobble, ac = a * crush, ad = a * dust, ap = a * drop;

        // ---- wobble (wow + flutter) via modulated delay
        d[0].push (l[i]); d[1].push (r[i]);
        const float wowv = wow.next (0.7f / fs);
        flutPhase += (9.5 + 2.0 * flut.next (0.5f / fs)) / sr; if (flutPhase >= 1.0) flutPhase -= 1.0;
        const float fl = sin01 ((float) flutPhase);
        const float mod = aw * (fs * 0.0045f * wowv + fs * 0.00028f * fl);
        const float dry0 = d[0].read (baseDelay), dry1 = d[1].read (baseDelay);
        if (a < 0.0005f) { l[i] = dry0; r[i] = dry1; continue; }
        float x0 = d[0].read (baseDelay + mod), x1 = d[1].read (baseDelay + mod * 0.92f);

        // ---- tape saturation
        const float drv = 1.0f + a * 2.5f;
        x0 = fastTanh (x0 * drv) / std::sqrt (drv); x1 = fastTanh (x1 * drv) / std::sqrt (drv);

        // ---- rot: sample-rate + bit reduction
        if (ac > 0.002f)
        {
            const float hold = 1.0f + std::pow (ac, 1.6f) * 10.0f;
            holdPhase += 1.0f;
            if (holdPhase >= hold) { holdPhase -= hold; holdL = x0; holdR = x1; }
            const float bits = std::exp2 (16.0f - std::pow (ac, 1.2f) * 11.0f);
            const float mixc = std::min (1.0f, ac * 3.0f);
            x0 = lerp (x0, std::round (holdL * bits) / bits, mixc);
            x1 = lerp (x1, std::round (holdR * bits) / bits, mixc);
        }

        // ---- dusty band limiting
        const float filt = a * (0.55f + 0.45f * dust);
        const float lpHz = 20000.0f * std::pow (0.14f, filt), hpHz = 12.0f * std::pow (14.0f, filt);
        if ((i & 15) == 0)
            for (int c = 0; c < 2; ++c) { lp1[c].setCutoff (lpHz, fs); lp2[c].setCutoff (lpHz * 1.3f, fs); hp[c].setCutoff (hpHz, fs); }
        x0 = hp[0].hp (lp2[0].lp (lp1[0].lp (x0)));
        x1 = hp[1].hp (lp2[1].lp (lp1[1].lp (x1)));

        // ---- dropouts
        if (ap > 0.002f)
        {
            if (dropHold > 0) { if (--dropHold == 0) dropTarget = 1.0f; }
            else if (rng.uni() < std::pow (ap, 1.5f) * 1.4f / fs)
            {
                dropTarget = 0.12f + 0.45f * rng.uni();
                dropHold = (int) (fs * (0.04f + 0.22f * rng.uni()));
            }
        }
        else dropTarget = 1.0f;
        const float dc = dropTarget < dropGain ? 1.0f / (0.005f * fs) : 1.0f / (0.09f * fs);
        dropGain += (dropTarget - dropGain) * std::min (1.0f, dc * 4.0f);
        if (dropGain < 0.999f)
        {
            const float dlp = 1.0f - dropGain;
            const float c = std::exp (-kTwoPi * (12000.0f * (1.0f - dlp) + 800.0f) / fs);
            dropLp[0].a = c; dropLp[1].a = c;
            x0 = dropLp[0].lp (x0) * dropGain; x1 = dropLp[1].lp (x1) * dropGain;
        }
        else { dropLp[0].reset (x0); dropLp[1].reset (x1); }

        // ---- dust: hiss + crackle
        if (ad > 0.002f)
        {
            float hiss[2];
            for (int c = 0; c < 2; ++c)
            {
                const float w = rng.bi();
                auto& b = pinkB[c];
                b[0] = 0.99765f * b[0] + w * 0.0990460f;
                b[1] = 0.96300f * b[1] + w * 0.2965164f;
                b[2] = 0.57000f * b[2] + w * 1.0526913f;
                hiss[c] = (b[0] + b[1] + b[2] + w * 0.1848f) * 0.11f;
            }
            const float hissLvl = std::pow (ad, 1.6f) * 0.035f;
            if (rng.uni() < (ad * ad * 55.0f + ad * 6.0f) / fs) { crackle = (rng.bi() > 0 ? 1.0f : -1.0f) * (0.3f + 0.7f * rng.uni()); crackleEnv = 1.0f; }
            crackleEnv *= 0.82f;
            const float cr = crackle * crackleEnv * ad * 0.25f;
            x0 += hiss[0] * hissLvl + cr;
            x1 += hiss[1] * hissLvl + cr * 0.8f;
        }

        const float wet = std::min (1.0f, a * 25.0f);
        l[i] = lerp (dry0, x0, wet); r[i] = lerp (dry1, x1, wet);
    }
}
} // namespace ml
