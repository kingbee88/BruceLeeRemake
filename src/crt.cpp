// Bruce Lee, a Windows port.
// Copyright (C) 2026 Jacob Marner
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// No copyright is claimed over the original game, which is copyright 1984 Datasoft, Inc.

#include "crt.h"

#include <cmath>
#include <thread>
#include <vector>

namespace {

// Living-room television, not a fisheye. Nudge these after looking at a room.
constexpr float kBend = 0.08f;
// 0.50 keeps most of each pixel and lends a little to the columns beside it.
constexpr float kHorizontalSigma = 0.50f;
constexpr float kBeamSigma = 0.30f;
// 0.15 leaves the curved corners at about 85% of the color just inside them.
constexpr float kVignette = 0.15f;
constexpr float kVignetteStart = 0.6f;
constexpr float kVignetteEnd = 1.8f;

constexpr int kBeamPhases = 256;
constexpr int kEncodeSteps = 4096;

struct LinearRgb {
    float r;
    float g;
    float b;
};

struct Tables {
    float toLinear[256];
    uint8_t toSrgb[kEncodeSteps];
    float blurWeight[5];
    int beamRow[kBeamPhases][3];
    float beamWeight[kBeamPhases][3];
};

float SrgbByteToLinear(int channel) {
    const float c = static_cast<float>(channel) / 255.f;
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float LinearToSrgb(float linear) {
    if (linear <= 0.0031308f) {
        return linear * 12.92f;
    }
    return 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
}

float BeamAt(float distance) {
    const float twoSigmaSq = 2.f * kBeamSigma * kBeamSigma;
    return std::exp(-(distance * distance) / twoSigmaSq);
}

// The beam weight is the same for every pixel that sits at the same fraction
// of a source row. Dividing by the average across one row keeps a flat color
// at its old brightness. Dividing by the local weight would flatten the lines.
void BuildBeam(Tables& tables) {
    float raw[kBeamPhases][3];
    double sum = 0.0;
    for (int phase = 0; phase < kBeamPhases; ++phase) {
        const float frac = (static_cast<float>(phase) + 0.5f) / static_cast<float>(kBeamPhases);
        int rows[4];
        float weight[4];
        float dist[4];
        for (int i = 0; i < 4; ++i) {
            rows[i] = i - 1;
            const float delta = frac - static_cast<float>(rows[i]);
            dist[i] = delta < 0.f ? -delta : delta;
            weight[i] = BeamAt(delta);
        }
        int order[4] = {0, 1, 2, 3};
        for (int a = 0; a < 3; ++a) {
            int best = a;
            for (int b = a + 1; b < 4; ++b) {
                if (dist[order[b]] < dist[order[best]]) {
                    best = b;
                }
            }
            const int swap = order[a];
            order[a] = order[best];
            order[best] = swap;
        }
        float total = 0.f;
        for (int i = 0; i < 3; ++i) {
            tables.beamRow[phase][i] = rows[order[i]];
            raw[phase][i] = weight[order[i]];
            total += weight[order[i]];
        }
        sum += static_cast<double>(total);
    }
    const float norm = static_cast<float>(sum / kBeamPhases);
    for (int phase = 0; phase < kBeamPhases; ++phase) {
        for (int i = 0; i < 3; ++i) {
            tables.beamWeight[phase][i] = raw[phase][i] / norm;
        }
    }
}

const Tables& Lut() {
    static Tables tables{};
    static bool ready = false;
    if (ready) {
        return tables;
    }
    for (int channel = 0; channel < 256; ++channel) {
        tables.toLinear[channel] = SrgbByteToLinear(channel);
    }
    for (int step = 0; step < kEncodeSteps; ++step) {
        const float linear = static_cast<float>(step) / static_cast<float>(kEncodeSteps - 1);
        int byte = static_cast<int>(LinearToSrgb(linear) * 255.f + 0.5f);
        if (byte < 0) {
            byte = 0;
        }
        if (byte > 255) {
            byte = 255;
        }
        tables.toSrgb[step] = static_cast<uint8_t>(byte);
    }
    float weightSum = 0.f;
    const float blurDenom = 2.f * kHorizontalSigma * kHorizontalSigma;
    for (int tap = -2; tap <= 2; ++tap) {
        const float weight = std::exp(-(static_cast<float>(tap * tap)) / blurDenom);
        tables.blurWeight[tap + 2] = weight;
        weightSum += weight;
    }
    for (float& weight : tables.blurWeight) {
        weight /= weightSum;
    }
    BuildBeam(tables);
    ready = true;
    return tables;
}

int Encode(const Tables& tables, float linear) {
    if (linear <= 0.f) {
        return 0;
    }
    if (linear >= 1.f) {
        return 255;
    }
    int step = static_cast<int>(linear * static_cast<float>(kEncodeSteps - 1));
    if (step < 0) {
        step = 0;
    }
    if (step >= kEncodeSteps) {
        step = kEncodeSteps - 1;
    }
    return tables.toSrgb[step];
}

// Full brightness through the middle of the glass. The falloff is on the
// pre-bend radius, so it follows the tube rather than the bowed picture.
float Vignette(float radiusSq) {
    float t = (radiusSq - kVignetteStart) / (kVignetteEnd - kVignetteStart);
    if (t <= 0.f) {
        return 1.f;
    }
    if (t >= 1.f) {
        return 1.f - kVignette;
    }
    const float smooth = t * t * (3.f - 2.f * t);
    return 1.f - kVignette * smooth;
}

void BlurHorizontal(const Tables& tables, const uint32_t* src, int srcW, int srcH, std::vector<LinearRgb>& blurred) {
    blurred.resize(static_cast<size_t>(srcW) * static_cast<size_t>(srcH));
    for (int y = 0; y < srcH; ++y) {
        const uint32_t* in = src + static_cast<size_t>(y) * static_cast<size_t>(srcW);
        LinearRgb* out = blurred.data() + static_cast<size_t>(y) * static_cast<size_t>(srcW);
        for (int x = 0; x < srcW; ++x) {
            float r = 0.f;
            float g = 0.f;
            float b = 0.f;
            for (int tap = 0; tap < 5; ++tap) {
                int xx = x + tap - 2;
                if (xx < 0) {
                    xx = 0;
                }
                if (xx >= srcW) {
                    xx = srcW - 1;
                }
                const uint32_t pixel = in[xx];
                const float weight = tables.blurWeight[tap];
                r += tables.toLinear[(pixel >> 16) & 255u] * weight;
                g += tables.toLinear[(pixel >> 8) & 255u] * weight;
                b += tables.toLinear[pixel & 255u] * weight;
            }
            out[x].r = r;
            out[x].g = g;
            out[x].b = b;
        }
    }
}

// std::floor is a library call in this loop. The source coordinate stays near
// the picture, so a truncating cast is the same result.
int FloorToInt(float value) {
    const int truncated = static_cast<int>(value);
    return static_cast<float>(truncated) > value ? truncated - 1 : truncated;
}

void SampleRow(const LinearRgb* row, int i0, int i1, float frac, float& r, float& g, float& b) {
    const LinearRgb& left = row[i0];
    const LinearRgb& right = row[i1];
    r = left.r + (right.r - left.r) * frac;
    g = left.g + (right.g - left.g) * frac;
    b = left.b + (right.b - left.b) * frac;
}

int ScaleChannel(int byte, float vignette);

void LinearOf(const Tables& tables, uint32_t pixel, float& r, float& g, float& b) {
    r = tables.toLinear[(pixel >> 16) & 255u];
    g = tables.toLinear[(pixel >> 8) & 255u];
    b = tables.toLinear[pixel & 255u];
}

// Blur on interpolates the pre-smeared row. Blur off takes the nearest column.
void SampleX(const Tables& tables, const uint32_t* srcRow, const LinearRgb* blurRow,
             int srcW, float sx, bool blur, float& r, float& g, float& b) {
    if (blur) {
        const int x0 = FloorToInt(sx);
        const float frac = sx - static_cast<float>(x0);
        int i0 = x0;
        int i1 = x0 + 1;
        if (i0 < 0) {
            i0 = 0;
        }
        if (i1 < 0) {
            i1 = 0;
        }
        if (i0 >= srcW) {
            i0 = srcW - 1;
        }
        if (i1 >= srcW) {
            i1 = srcW - 1;
        }
        SampleRow(blurRow, i0, i1, frac, r, g, b);
        return;
    }
    int col = FloorToInt(sx + 0.5f);
    if (col < 0) {
        col = 0;
    }
    if (col >= srcW) {
        col = srcW - 1;
    }
    LinearOf(tables, srcRow[col], r, g, b);
}

// One band of destination rows. Callers that split the picture give each
// worker its own band, and those bands do not overlap.
void ShadeRows(const Tables& tables, const uint32_t* src, const LinearRgb* blurred, uint32_t* dest,
               int srcW, int srcH, int destW, int destH, CrtOptions options, int y0, int y1) {
    const float destWidth = static_cast<float>(destW);
    const float destHeight = static_cast<float>(destH);
    const float sourceWidth = static_cast<float>(srcW);
    const float sourceHeight = static_cast<float>(srcH);
    const float bendDenom = 1.f + kBend;
    const size_t sourceStride = static_cast<size_t>(srcW);

    for (int y = y0; y < y1; ++y) {
        const float py = ((static_cast<float>(y) + 0.5f) / destHeight) * 2.f - 1.f;
        const float py2 = py * py;
        uint32_t* rowOut = dest + static_cast<size_t>(y) * static_cast<size_t>(destW);
        for (int x = 0; x < destW; ++x) {
            const float px = ((static_cast<float>(x) + 0.5f) / destWidth) * 2.f - 1.f;
            const float radiusSq = px * px + py2;
            float bx = px;
            float by = py;
            if (options.curve) {
                const float bowed = (1.f + kBend * radiusSq) / bendDenom;
                bx = px * bowed;
                by = py * bowed;
            }
            if (bx < -1.f || bx > 1.f || by < -1.f || by > 1.f) {
                rowOut[x] = 0xFF000000u;
                continue;
            }
            const float sx = (bx * 0.5f + 0.5f) * sourceWidth - 0.5f;
            const float sy = (by * 0.5f + 0.5f) * sourceHeight - 0.5f;
            float r = 0.f;
            float g = 0.f;
            float b = 0.f;
            if (options.scanlines) {
                const int base = FloorToInt(sy);
                float rowFrac = sy - static_cast<float>(base);
                if (rowFrac < 0.f) {
                    rowFrac = 0.f;
                }
                int phase = static_cast<int>(rowFrac * static_cast<float>(kBeamPhases));
                if (phase < 0) {
                    phase = 0;
                }
                if (phase >= kBeamPhases) {
                    phase = kBeamPhases - 1;
                }
                for (int i = 0; i < 3; ++i) {
                    const int row = base + tables.beamRow[phase][i];
                    if (row < 0 || row >= srcH) {
                        continue;
                    }
                    float rr = 0.f;
                    float gg = 0.f;
                    float bb = 0.f;
                    const uint32_t* srcRow = src + static_cast<size_t>(row) * sourceStride;
                    const LinearRgb* blurRow = options.blur ? blurred + static_cast<size_t>(row) * sourceStride : nullptr;
                    SampleX(tables, srcRow, blurRow, srcW, sx, options.blur, rr, gg, bb);
                    const float weight = tables.beamWeight[phase][i];
                    r += rr * weight;
                    g += gg * weight;
                    b += bb * weight;
                }
            } else {
                int row = FloorToInt(sy + 0.5f);
                if (row < 0) {
                    row = 0;
                }
                if (row >= srcH) {
                    row = srcH - 1;
                }
                const uint32_t* srcRow = src + static_cast<size_t>(row) * sourceStride;
                const LinearRgb* blurRow = options.blur ? blurred + static_cast<size_t>(row) * sourceStride : nullptr;
                SampleX(tables, srcRow, blurRow, srcW, sx, options.blur, r, g, b);
            }
            const float vignette = options.vignette ? Vignette(radiusSq) : 1.f;
            const int ir = ScaleChannel(Encode(tables, r), vignette);
            const int ig = ScaleChannel(Encode(tables, g), vignette);
            const int ib = ScaleChannel(Encode(tables, b), vignette);
            rowOut[x] = 0xFF000000u | (static_cast<uint32_t>(ir) << 16) |
                        (static_cast<uint32_t>(ig) << 8) | static_cast<uint32_t>(ib);
        }
    }
}

int ScaleChannel(int byte, float vignette) {
    int scaled = static_cast<int>(static_cast<float>(byte) * vignette + 0.5f);
    if (scaled < 0) {
        return 0;
    }
    if (scaled > 255) {
        return 255;
    }
    return scaled;
}

}  // namespace

void ApplyCrt(const uint32_t* src, int srcW, int srcH, uint32_t* dest, int destW, int destH,
              CrtOptions options) {
    if (src == nullptr || dest == nullptr || srcW < 1 || srcH < 1 || destW < 1 || destH < 1) {
        return;
    }
    const Tables& tables = Lut();
    static std::vector<LinearRgb> blurred;
    const LinearRgb* blur = nullptr;
    if (options.blur) {
        BlurHorizontal(tables, src, srcW, srcH, blurred);
        blur = blurred.data();
    }

    // The game waits 60ms after Paint returns. One core spends most of that on
    // a fullscreen picture, so the rows are split across the machine. A small
    // window stays on this thread; starting workers would cost more than it saves.
    unsigned workers = std::thread::hardware_concurrency();
    if (workers == 0 || static_cast<size_t>(destW) * static_cast<size_t>(destH) < 640u * 400u) {
        workers = 1;
    }
    if (workers > static_cast<unsigned>(destH)) {
        workers = static_cast<unsigned>(destH);
    }
    if (workers == 1) {
        ShadeRows(tables, src, blur, dest, srcW, srcH, destW, destH, options, 0, destH);
        return;
    }
    std::vector<std::thread> threads;
    threads.reserve(workers - 1);
    int bandStart = 0;
    for (unsigned worker = 0; worker + 1 < workers; ++worker) {
        const int bandEnd = static_cast<int>((static_cast<size_t>(destH) * (worker + 1)) / workers);
        threads.emplace_back([&, bandStart, bandEnd, options] {
            ShadeRows(tables, src, blur, dest, srcW, srcH, destW, destH, options, bandStart, bandEnd);
        });
        bandStart = bandEnd;
    }
    ShadeRows(tables, src, blur, dest, srcW, srcH, destW, destH, options, bandStart, destH);
    for (std::thread& worker : threads) {
        worker.join();
    }
}
