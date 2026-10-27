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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>

#include "sound.h"

#include "config.h"
#include "load.h"

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

// The player at $E000 and the effect voices at $EB03 are self-modifying.
// Each instruction is read from the snapshot RAM when it runs.

namespace audio {

constexpr int kSidClock = 985248;
constexpr int kSampleRate = 48000;
constexpr int kFrameCycles = 19656;
constexpr int kBlock = 512;
// Three voices at volume 15, after the output is AC coupled, stay inside
// 16-bit without a hard clip. One voice at volume 5 is still a clear tick.
constexpr float kOutput = 1024.f;

constexpr uint8_t FC = 0x01;
constexpr uint8_t FZ = 0x02;
constexpr uint8_t FD = 0x08;
constexpr uint8_t FU = 0x20;
constexpr uint8_t FV = 0x40;
constexpr uint8_t FN = 0x80;

// PAL 6581 envelope periods, in cycles, for rates 0 through 15.
constexpr int kRates[16] = {
    9, 32, 63, 95, 149, 220, 267, 313,
    392, 977, 1954, 3126, 3907, 11720, 19532, 31251};

enum class Env { Attack, Decay, Sustain, Release };
enum class Mode { Effects, Music };

struct Voice {
    uint16_t freq = 0;
    uint16_t pw = 0;
    uint8_t ctrl = 0;
    uint8_t attack = 0;
    uint8_t decay = 0;
    uint8_t sustain = 0;
    uint8_t releaseN = 0;
    uint32_t acc = 0;
    uint32_t lfsr = 0x7FFFFF;
    bool locked = false;
    int env = 0;
    Env state = Env::Release;
    bool holdZero = true;
    int rateCounter = 0;
    int expCounter = 0;
    int expPeriod = 1;

    void NoteLevel() {
        switch (env) {
        case 0xFF: expPeriod = 1; break;
        case 0x5D: expPeriod = 2; break;
        case 0x36: expPeriod = 4; break;
        case 0x1A: expPeriod = 8; break;
        case 0x0E: expPeriod = 16; break;
        case 0x06: expPeriod = 30; break;
        case 0x00: expPeriod = 1; break;
        default: break;
        }
    }

    void Control(uint8_t value) {
        const bool test = (value & 0x08) != 0;
        const bool wasTest = (ctrl & 0x08) != 0;
        if (test && !wasTest) {
            acc = 0;
            lfsr = 0x7FFFFF;
            locked = false;
        }
        const bool gate = (value & 0x01) != 0;
        const bool wasGate = (ctrl & 0x01) != 0;
        if (gate && !wasGate) {
            state = Env::Attack;
            holdZero = false;
        } else if (!gate && wasGate) {
            state = Env::Release;
        }
        ctrl = value;
    }

    void ClockLfsr() {
        const uint32_t bit = ((lfsr >> 22) ^ (lfsr >> 17)) & 1u;
        lfsr = ((lfsr << 1) | bit) & 0x7FFFFFu;
    }

    void ClockOsc() {
        if (ctrl & 0x08) {
            acc = 0;
            return;
        }
        const uint32_t prev = acc;
        acc = (acc + freq) & 0xFFFFFFu;
        if ((ctrl & 0x80) && (ctrl & 0x70)) {
            locked = true;
        }
        if (locked || (ctrl & 0x80) == 0) {
            return;
        }
        if ((prev & 0x80000u) == 0 && (acc & 0x80000u) != 0) {
            ClockLfsr();
        }
    }

    void ClockEnv() {
        int nibble = attack;
        if (state == Env::Decay || state == Env::Sustain) {
            nibble = decay;
        } else if (state == Env::Release) {
            nibble = releaseN;
        }
        int period = kRates[nibble];
        if (period < 1) {
            period = 1;
        }
        rateCounter++;
        if (rateCounter < period) {
            return;
        }
        rateCounter = 0;
        // The sustain state keeps comparing. A lower sustain written while
        // the gate is still held continues the decay. The gate is left as
        // it was. Effect $8A's long pulse is sustain 0 after the envelope
        // has already reached sustain 15, so that tone decays.
        if (state == Env::Sustain) {
            const int level = (sustain << 4) | sustain;
            if (env <= level) {
                return;
            }
            state = Env::Decay;
        }
        if (state == Env::Attack) {
            if (env < 255) {
                env++;
                NoteLevel();
            }
            if (env == 255) {
                state = Env::Decay;
            }
            return;
        }
        if (holdZero) {
            return;
        }
        expCounter++;
        if (expCounter < expPeriod) {
            return;
        }
        expCounter = 0;
        if (env > 0) {
            env--;
        }
        if (state == Env::Decay) {
            const int level = (sustain << 4) | sustain;
            if (env <= level) {
                state = Env::Sustain;
            }
        }
        if (env == 0) {
            holdZero = true;
        }
        NoteLevel();
    }

    int Wave(int otherMsb) const {
        if (locked) {
            return 0;
        }
        int out = 0xFFF;
        int n = 0;
        if (ctrl & 0x10) {
            int tri = static_cast<int>((acc >> 11) & 0xFFF);
            int invert = static_cast<int>(acc >> 23) & 1;
            if (ctrl & 0x04) {
                invert ^= otherMsb;
            }
            if (invert) {
                tri ^= 0xFFF;
            }
            out &= tri;
            n++;
        }
        if (ctrl & 0x20) {
            out &= static_cast<int>((acc >> 12) & 0xFFF);
            n++;
        }
        if (ctrl & 0x40) {
            const int upper = static_cast<int>((acc >> 12) & 0xFFF);
            out &= upper >= pw ? 0xFFF : 0;
            n++;
        }
        if (ctrl & 0x80) {
            const uint32_t sr = lfsr;
            const int noise = static_cast<int>(
                ((sr & 0x100000u) >> 9) | ((sr & 0x040000u) >> 8) |
                ((sr & 0x004000u) >> 5) | ((sr & 0x000800u) >> 3) |
                ((sr & 0x000200u) >> 2) | ((sr & 0x000020u) << 1) |
                ((sr & 0x000004u) << 2) | (sr & 0x000001u));
            out &= noise;
            n++;
        }
        if (n == 0) {
            return 0;
        }
        return out;
    }
};

struct Sid {
    Voice v[3];
    uint8_t reg[0x19]{};
    float low = 0;
    float band = 0;

    void Reset() {
        for (Voice& voice : v) {
            voice = Voice{};
        }
        std::memset(reg, 0, sizeof reg);
        low = 0;
        band = 0;
    }

    void Write(int r, uint8_t value) {
        if (r < 0 || r > 0x18) {
            return;
        }
        reg[r] = value;
        if (r >= 0x15) {
            return;
        }
        Voice& voice = v[r / 7];
        switch (r % 7) {
        case 0:
            voice.freq = static_cast<uint16_t>((voice.freq & 0xFF00) | value);
            break;
        case 1:
            voice.freq = static_cast<uint16_t>((voice.freq & 0x00FF) | (value << 8));
            break;
        case 2:
            voice.pw = static_cast<uint16_t>((voice.pw & 0x0F00) | value);
            break;
        case 3:
            voice.pw = static_cast<uint16_t>((voice.pw & 0x00FF) | ((value & 0x0F) << 8));
            break;
        case 4:
            voice.Control(value);
            break;
        case 5:
            voice.attack = value >> 4;
            voice.decay = value & 0x0F;
            break;
        case 6:
            voice.sustain = value >> 4;
            voice.releaseN = value & 0x0F;
            break;
        default:
            break;
        }
    }

    void Clock() {
        uint32_t before[3];
        for (int i = 0; i < 3; ++i) {
            before[i] = v[i].acc;
            v[i].ClockOsc();
        }
        for (int i = 0; i < 3; ++i) {
            const int src = (i + 2) % 3;
            const bool rose = (v[src].acc & 0x800000u) != 0 && (before[src] & 0x800000u) == 0;
            if ((v[i].ctrl & 0x02) && rose && (v[i].ctrl & 0x08) == 0) {
                v[i].acc = 0;
            }
        }
        for (Voice& voice : v) {
            voice.ClockEnv();
        }
    }

    void Mix(int& dry, int& wet) const {
        dry = 0;
        wet = 0;
        const int route = reg[0x17] & 7;
        const bool mute3 = (reg[0x18] & 0x80) != 0;
        for (int i = 0; i < 3; ++i) {
            if (i == 2 && mute3) {
                continue;
            }
            const int other = static_cast<int>(v[(i + 2) % 3].acc >> 23) & 1;
            const int sample = (v[i].Wave(other) - 0x800) * v[i].env;
            if (route != 0 && (route & (1 << i)) != 0) {
                wet += sample;
            } else {
                dry += sample;
            }
        }
    }

    // A state-variable filter, run once per output sample, and only when a
    // voice is routed. No mode bit mutes the routed voice, as on the 6581.
    float Filter(float in) {
        const int mode = reg[0x18] & 0x70;
        if (mode == 0) {
            return 0;
        }
        const int fc = ((reg[0x16] << 3) | (reg[0x15] & 7)) & 0x7FF;
        const float hz = 30.f * std::pow(2.f, fc * (8.f / 2047.f));
        float f = 2.f * std::sin(3.14159265f * hz / static_cast<float>(kSampleRate));
        if (f < 0.f) {
            f = 0.f;
        }
        if (f > 0.95f) {
            f = 0.95f;
        }
        const float res = (reg[0x17] >> 4) / 15.f;
        float damp = 1.2f - res * 1.05f;
        if (damp < 0.15f) {
            damp = 0.15f;
        }
        low += f * band;
        const float high = in - low - damp * band;
        band += f * high;
        if (low > 4.0e6f || low < -4.0e6f || band != band) {
            low = 0;
            band = 0;
            return 0;
        }
        float out = 0;
        if (mode & 0x10) {
            out += low;
        }
        if (mode & 0x20) {
            out += band;
        }
        if (mode & 0x40) {
            out += high;
        }
        return out;
    }

    int Volume() const {
        return reg[0x18] & 0x0F;
    }

    int Route() const {
        return reg[0x17] & 7;
    }
};

uint8_t mem[65536];
Sid sid;
uint16_t pc = 0;
uint8_t a = 0;
uint8_t x = 0;
uint8_t y = 0;
uint8_t sp = 0xFF;
uint8_t p = FU;
bool fault = false;
uint8_t faultOp = 0;
uint16_t faultPc = 0;
int lastSteps = 0;
Mode mode = Mode::Effects;
int framePos = 0;
int cycleAcc = 0;
float hp = 0;
float prevIn = 0;
std::atomic<bool> audioOn{false};
std::atomic<bool> run{false};
std::atomic<bool> held{false};
std::mutex mu;
std::condition_variable started;
bool ready = false;
bool deviceOk = false;
std::thread thread;
IXAudio2* xa = nullptr;
IXAudio2MasteringVoice* master = nullptr;
IXAudio2SourceVoice* source = nullptr;

uint8_t Read(uint16_t addr) {
    return mem[addr];
}

void Write(uint16_t addr, uint8_t value) {
    mem[addr] = value;
    if (addr >= 0xD400 && addr <= 0xD418) {
        sid.Write(static_cast<int>(addr - 0xD400), value);
    }
}

void Zn(uint8_t value) {
    p = static_cast<uint8_t>((p & ~(FZ | FN)) | (value == 0 ? FZ : 0) | (value & FN));
}

void Push(uint8_t value) {
    mem[0x100 + sp] = value;
    sp = static_cast<uint8_t>(sp - 1);
}

uint8_t Pop() {
    sp = static_cast<uint8_t>(sp + 1);
    return mem[0x100 + sp];
}

uint8_t Fetch() {
    const uint8_t value = mem[pc];
    pc = static_cast<uint16_t>(pc + 1);
    return value;
}

uint16_t FetchWord() {
    const uint8_t lo = Fetch();
    const uint8_t hi = Fetch();
    return static_cast<uint16_t>(lo | (hi << 8));
}

void Fault(uint8_t op) {
    fault = true;
    faultOp = op;
    faultPc = static_cast<uint16_t>(pc - 1);
}

void Adc(uint8_t m) {
    if (p & FD) {
        unsigned lo = (a & 0x0F) + (m & 0x0F) + (p & FC);
        unsigned sum = (a & 0xF0) + (m & 0xF0);
        if (lo >= 10) {
            lo += 6;
        }
        sum += lo;
        p = static_cast<uint8_t>(p & ~(FC | FZ | FV | FN));
        if ((sum & 0x1F0) > 0x90) {
            sum += 0x60;
            p = static_cast<uint8_t>(p | FC);
        }
        a = static_cast<uint8_t>(sum);
        Zn(a);
        return;
    }
    const unsigned sum = a + m + (p & FC);
    const uint8_t result = static_cast<uint8_t>(sum);
    p = static_cast<uint8_t>(p & ~(FC | FV | FZ | FN));
    if (sum > 0xFF) {
        p = static_cast<uint8_t>(p | FC);
    }
    if ((~(a ^ m) & (a ^ result) & 0x80) != 0) {
        p = static_cast<uint8_t>(p | FV);
    }
    a = result;
    Zn(a);
}

void Sbc(uint8_t m) {
    if (p & FD) {
        const unsigned borrow = (p & FC) ? 0u : 1u;
        unsigned lo = (a & 0x0F) - (m & 0x0F) - borrow;
        unsigned hi = (a >> 4) - (m >> 4);
        p = static_cast<uint8_t>(p & ~(FC | FZ | FV | FN));
        if (lo & 0x10) {
            lo -= 6;
            hi--;
        }
        if (hi & 0x10) {
            hi -= 6;
        } else {
            p = static_cast<uint8_t>(p | FC);
        }
        a = static_cast<uint8_t>((hi << 4) | (lo & 0x0F));
        Zn(a);
        return;
    }
    Adc(static_cast<uint8_t>(m ^ 0xFF));
}

void Cmp(uint8_t reg, uint8_t m) {
    const unsigned diff = reg - m;
    p = static_cast<uint8_t>(p & ~(FC | FZ | FN));
    if (reg >= m) {
        p = static_cast<uint8_t>(p | FC);
    }
    Zn(static_cast<uint8_t>(diff));
}

uint16_t AddrZp() {
    return Fetch();
}

uint16_t AddrZpX() {
    return static_cast<uint8_t>(Fetch() + x);
}

uint16_t AddrZpY() {
    return static_cast<uint8_t>(Fetch() + y);
}

uint16_t AddrAbs() {
    return FetchWord();
}

uint16_t AddrAbsX() {
    return static_cast<uint16_t>(FetchWord() + x);
}

uint16_t AddrAbsY() {
    return static_cast<uint16_t>(FetchWord() + y);
}

uint16_t AddrIndX() {
    const uint8_t z = static_cast<uint8_t>(Fetch() + x);
    return static_cast<uint16_t>(mem[z] | (mem[static_cast<uint8_t>(z + 1)] << 8));
}

uint16_t AddrIndY() {
    const uint8_t z = Fetch();
    const uint16_t base = static_cast<uint16_t>(mem[z] | (mem[static_cast<uint8_t>(z + 1)] << 8));
    return static_cast<uint16_t>(base + y);
}

void Shift(uint16_t addr, int kind, bool accumulator) {
    uint8_t value = accumulator ? a : Read(addr);
    const uint8_t carry = p & FC;
    if (kind == 0) {
        p = static_cast<uint8_t>((p & ~FC) | ((value & 0x80) ? FC : 0));
        value = static_cast<uint8_t>(value << 1);
    } else if (kind == 1) {
        p = static_cast<uint8_t>((p & ~FC) | ((value & 0x80) ? FC : 0));
        value = static_cast<uint8_t>((value << 1) | carry);
    } else if (kind == 2) {
        p = static_cast<uint8_t>((p & ~FC) | ((value & 0x01) ? FC : 0));
        value = static_cast<uint8_t>(value >> 1);
    } else {
        p = static_cast<uint8_t>((p & ~FC) | ((value & 0x01) ? FC : 0));
        value = static_cast<uint8_t>((value >> 1) | (carry << 7));
    }
    Zn(value);
    if (accumulator) {
        a = value;
    } else {
        Write(addr, value);
    }
}

void IncMem(uint16_t addr, int delta) {
    const uint8_t value = static_cast<uint8_t>(Read(addr) + delta);
    Zn(value);
    Write(addr, value);
}

void Branch(bool take) {
    const int8_t off = static_cast<int8_t>(Fetch());
    if (take) {
        pc = static_cast<uint16_t>(pc + off);
    }
}

void Alu(uint8_t op) {
    const int which = op >> 5;
    const int modeBits = (op >> 2) & 7;
    if (which == 4 && modeBits == 2) {
        Fault(op);
        return;
    }
    uint16_t addr = 0;
    bool immediate = false;
    uint8_t imm = 0;
    switch (modeBits) {
    case 0: addr = AddrIndX(); break;
    case 1: addr = AddrZp(); break;
    case 2: immediate = true; imm = Fetch(); break;
    case 3: addr = AddrAbs(); break;
    case 4: addr = AddrIndY(); break;
    case 5: addr = AddrZpX(); break;
    case 6: addr = AddrAbsY(); break;
    default: addr = AddrAbsX(); break;
    }
    if (which == 4) {
        Write(addr, a);
        return;
    }
    const uint8_t m = immediate ? imm : Read(addr);
    switch (which) {
    case 0: a = static_cast<uint8_t>(a | m); Zn(a); break;
    case 1: a = static_cast<uint8_t>(a & m); Zn(a); break;
    case 2: a = static_cast<uint8_t>(a ^ m); Zn(a); break;
    case 3: Adc(m); break;
    case 5: a = m; Zn(a); break;
    case 6: Cmp(a, m); break;
    default: Sbc(m); break;
    }
}

void Step() {
    const uint8_t op = Fetch();
    if ((op & 0x03) == 0x01) {
        Alu(op);
        return;
    }
    switch (op) {
    case 0x06: Shift(AddrZp(), 0, false); break;
    case 0x0A: Shift(0, 0, true); break;
    case 0x0E: Shift(AddrAbs(), 0, false); break;
    case 0x16: Shift(AddrZpX(), 0, false); break;
    case 0x1E: Shift(AddrAbsX(), 0, false); break;
    case 0x26: Shift(AddrZp(), 1, false); break;
    case 0x2A: Shift(0, 1, true); break;
    case 0x2E: Shift(AddrAbs(), 1, false); break;
    case 0x36: Shift(AddrZpX(), 1, false); break;
    case 0x3E: Shift(AddrAbsX(), 1, false); break;
    case 0x46: Shift(AddrZp(), 2, false); break;
    case 0x4A: Shift(0, 2, true); break;
    case 0x4E: Shift(AddrAbs(), 2, false); break;
    case 0x56: Shift(AddrZpX(), 2, false); break;
    case 0x5E: Shift(AddrAbsX(), 2, false); break;
    case 0x66: Shift(AddrZp(), 3, false); break;
    case 0x6A: Shift(0, 3, true); break;
    case 0x6E: Shift(AddrAbs(), 3, false); break;
    case 0x76: Shift(AddrZpX(), 3, false); break;
    case 0x7E: Shift(AddrAbsX(), 3, false); break;
    case 0x86: Write(AddrZp(), x); break;
    case 0x8E: Write(AddrAbs(), x); break;
    case 0x96: Write(AddrZpY(), x); break;
    case 0xA2: x = Fetch(); Zn(x); break;
    case 0xA6: x = Read(AddrZp()); Zn(x); break;
    case 0xAE: x = Read(AddrAbs()); Zn(x); break;
    case 0xB6: x = Read(AddrZpY()); Zn(x); break;
    case 0xBE: x = Read(AddrAbsY()); Zn(x); break;
    case 0x84: Write(AddrZp(), y); break;
    case 0x8C: Write(AddrAbs(), y); break;
    case 0x94: Write(AddrZpX(), y); break;
    case 0xA0: y = Fetch(); Zn(y); break;
    case 0xA4: y = Read(AddrZp()); Zn(y); break;
    case 0xAC: y = Read(AddrAbs()); Zn(y); break;
    case 0xB4: y = Read(AddrZpX()); Zn(y); break;
    case 0xBC: y = Read(AddrAbsX()); Zn(y); break;
    case 0xC6: IncMem(AddrZp(), -1); break;
    case 0xCE: IncMem(AddrAbs(), -1); break;
    case 0xD6: IncMem(AddrZpX(), -1); break;
    case 0xDE: IncMem(AddrAbsX(), -1); break;
    case 0xE6: IncMem(AddrZp(), 1); break;
    case 0xEE: IncMem(AddrAbs(), 1); break;
    case 0xF6: IncMem(AddrZpX(), 1); break;
    case 0xFE: IncMem(AddrAbsX(), 1); break;
    case 0x24:
    case 0x2C: {
        const uint8_t m = Read(op == 0x24 ? AddrZp() : AddrAbs());
        p = static_cast<uint8_t>((p & ~(FZ | FV | FN)) | ((a & m) == 0 ? FZ : 0) | (m & (FV | FN)));
        break;
    }
    case 0xC0: Cmp(y, Fetch()); break;
    case 0xC4: Cmp(y, Read(AddrZp())); break;
    case 0xCC: Cmp(y, Read(AddrAbs())); break;
    case 0xE0: Cmp(x, Fetch()); break;
    case 0xE4: Cmp(x, Read(AddrZp())); break;
    case 0xEC: Cmp(x, Read(AddrAbs())); break;
    case 0x08: Push(static_cast<uint8_t>(p | 0x30)); break;
    case 0x28: p = static_cast<uint8_t>(Pop() | FU); break;
    case 0x48: Push(a); break;
    case 0x68: a = Pop(); Zn(a); break;
    case 0x18: p = static_cast<uint8_t>(p & ~FC); break;
    case 0x38: p = static_cast<uint8_t>(p | FC); break;
    case 0x58: break;
    case 0x78: break;
    case 0xD8: p = static_cast<uint8_t>(p & ~FD); break;
    case 0xF8: p = static_cast<uint8_t>(p | FD); break;
    case 0xB8: p = static_cast<uint8_t>(p & ~FV); break;
    case 0x8A: a = x; Zn(a); break;
    case 0x98: a = y; Zn(a); break;
    case 0xAA: x = a; Zn(x); break;
    case 0xA8: y = a; Zn(y); break;
    case 0x9A: sp = x; break;
    case 0xBA: x = sp; Zn(x); break;
    case 0xCA: x = static_cast<uint8_t>(x - 1); Zn(x); break;
    case 0xE8: x = static_cast<uint8_t>(x + 1); Zn(x); break;
    case 0x88: y = static_cast<uint8_t>(y - 1); Zn(y); break;
    case 0xC8: y = static_cast<uint8_t>(y + 1); Zn(y); break;
    case 0xEA: break;
    case 0x10: Branch((p & FN) == 0); break;
    case 0x30: Branch((p & FN) != 0); break;
    case 0x50: Branch((p & FV) == 0); break;
    case 0x70: Branch((p & FV) != 0); break;
    case 0x90: Branch((p & FC) == 0); break;
    case 0xB0: Branch((p & FC) != 0); break;
    case 0xD0: Branch((p & FZ) == 0); break;
    case 0xF0: Branch((p & FZ) != 0); break;
    case 0x4C: pc = FetchWord(); break;
    case 0x6C: {
        const uint16_t addr = FetchWord();
        const uint8_t lo = Read(addr);
        const uint8_t hi = Read(static_cast<uint16_t>((addr & 0xFF00) | ((addr + 1) & 0xFF)));
        pc = static_cast<uint16_t>(lo | (hi << 8));
        break;
    }
    case 0x20: {
        const uint16_t target = FetchWord();
        const uint16_t ret = static_cast<uint16_t>(pc - 1);
        Push(static_cast<uint8_t>(ret >> 8));
        Push(static_cast<uint8_t>(ret));
        pc = target;
        break;
    }
    case 0x60: {
        const uint8_t lo = Pop();
        const uint8_t hi = Pop();
        pc = static_cast<uint16_t>(((hi << 8) | lo) + 1);
        break;
    }
    case 0x40: {
        p = static_cast<uint8_t>(Pop() | FU);
        const uint8_t lo = Pop();
        const uint8_t hi = Pop();
        pc = static_cast<uint16_t>(lo | (hi << 8));
        break;
    }
    default:
        Fault(op);
        break;
    }
}

bool Call(uint16_t addr) {
    if (fault) {
        return false;
    }
    sp = 0xFF;
    p = FU;
    Push(0x00);
    Push(0x00);
    pc = addr;
    lastSteps = 0;
    while (pc != 1) {
        Step();
        lastSteps++;
        if (fault) {
            return false;
        }
        if (lastSteps > 200000) {
            fault = true;
            faultOp = 0;
            faultPc = pc;
            return false;
        }
    }
    return true;
}

void Service() {
    if (fault) {
        return;
    }
    if (mode == Mode::Music) {
        Call(0xE006);
        return;
    }
    Call(0xEB03);
    Call(0xEF73);
    Call(0xF073);
}

int16_t NextSample() {
    cycleAcc += kSidClock;
    const int n = cycleAcc / kSampleRate;
    cycleAcc %= kSampleRate;
    int64_t drySum = 0;
    int64_t wetSum = 0;
    for (int i = 0; i < n; ++i) {
        if (framePos == 0) {
            Service();
        }
        sid.Clock();
        int dry = 0;
        int wet = 0;
        sid.Mix(dry, wet);
        drySum += dry;
        wetSum += wet;
        framePos++;
        if (framePos >= kFrameCycles) {
            framePos = 0;
        }
    }
    if (n <= 0) {
        return 0;
    }
    float tone = static_cast<float>(drySum) / static_cast<float>(n);
    if (sid.Route() == 0) {
        tone += static_cast<float>(wetSum) / static_cast<float>(n);
    } else {
        tone += sid.Filter(static_cast<float>(wetSum) / static_cast<float>(n));
    }
    tone *= static_cast<float>(sid.Volume());
    const float input = tone / kOutput;
    // The board is AC coupled. A voice left gated at frequency 0 is only DC.
    float output = input - prevIn + 0.998f * hp;
    prevIn = input;
    hp = output;
    if (output != output || output > 1.0e8f || output < -1.0e8f) {
        output = 0;
        hp = 0;
        prevIn = 0;
    }
    int sample = static_cast<int>(output);
    if (sample > 32767) {
        sample = 32767;
    }
    if (sample < -32768) {
        sample = -32768;
    }
    return static_cast<int16_t>(sample);
}

bool LoadImage() {
    const std::vector<uint8_t> data = LoadFile(L"sound.bin", 65536);
    if (data.size() != 65536) {
        return false;
    }
    std::memcpy(mem, data.data(), 65536);
    sid.Reset();
    fault = false;
    faultOp = 0;
    framePos = 0;
    cycleAcc = 0;
    hp = 0;
    prevIn = 0;
    mode = Mode::Effects;
    return true;
}

bool StartDevice() {
    HRESULT hr = XAudio2Create(&xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        xa = nullptr;
        return false;
    }
    hr = xa->CreateMasteringVoice(&master, 2, kSampleRate);
    if (FAILED(hr)) {
        xa->Release();
        xa = nullptr;
        return false;
    }
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kSampleRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = kSampleRate * 2;
    hr = xa->CreateSourceVoice(&source, &fmt);
    if (FAILED(hr)) {
        master->DestroyVoice();
        master = nullptr;
        xa->Release();
        xa = nullptr;
        return false;
    }
    return true;
}

void AudioMain() {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comOk = SUCCEEDED(co);
    const bool ok = comOk && StartDevice();
    {
        std::lock_guard<std::mutex> lock(mu);
        deviceOk = ok;
        ready = true;
    }
    started.notify_one();
    if (!ok) {
        if (comOk) {
            CoUninitialize();
        }
        return;
    }
    source->Start(0, XAUDIO2_COMMIT_NOW);
    std::vector<int16_t> bufs[4];
    for (std::vector<int16_t>& block : bufs) {
        block.assign(kBlock, 0);
    }
    int slot = 0;
    while (run.load()) {
        XAUDIO2_VOICE_STATE state{};
        source->GetState(&state);
        if (state.BuffersQueued >= 3) {
            Sleep(2);
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(mu);
            // A pause keeps the chip on this sample. Silence fills the
            // queue so the voice does not underrun, and the tune does not
            // run on ahead.
            if (held.load()) {
                for (int i = 0; i < kBlock; ++i) {
                    bufs[slot][static_cast<size_t>(i)] = 0;
                }
            } else {
                for (int i = 0; i < kBlock; ++i) {
                    bufs[slot][static_cast<size_t>(i)] = NextSample();
                }
            }
        }
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = kBlock * sizeof(int16_t);
        buffer.pAudioData = reinterpret_cast<const BYTE*>(bufs[slot].data());
        source->SubmitSourceBuffer(&buffer);
        slot = (slot + 1) & 3;
    }
    source->Stop(0, XAUDIO2_COMMIT_NOW);
    source->FlushSourceBuffers();
    source->DestroyVoice();
    source = nullptr;
    master->DestroyVoice();
    master = nullptr;
    xa->Release();
    xa = nullptr;
    if (comOk) {
        CoUninitialize();
    }
}

void Play(int id) {
    if (!audioOn.load() || fault) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu);
    if (!audioOn.load() || fault || mode != Mode::Effects) {
        return;
    }
    y = static_cast<uint8_t>(id);
    Call(0x2509);
}

void Silence() {
    if (!audioOn.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu);
    if (!audioOn.load()) {
        return;
    }
    mode = Mode::Effects;
    Call(0x255E);
}

void StartTune() {
    if (!audioOn.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu);
    if (!audioOn.load()) {
        return;
    }
    Call(0x255E);
    mem[0xE314] = 3;
    mem[0xE870] = 0;
    mem[0xE871] = 0xFF;
    Call(0xE003);
    mode = Mode::Music;
}

void StopTune() {
    if (!audioOn.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu);
    if (!audioOn.load()) {
        return;
    }
    mode = Mode::Effects;
    Call(0xE000);
    Call(0x255E);
}

bool Start() {
    if (!gConfig.sound || audioOn.load()) {
        return true;
    }
    if (!LoadImage()) {
        return false;
    }
    ready = false;
    deviceOk = false;
    run = true;
    thread = std::thread(AudioMain);
    {
        std::unique_lock<std::mutex> lock(mu);
        started.wait(lock, [] { return ready; });
    }
    if (!deviceOk) {
        run = false;
        if (thread.joinable()) {
            thread.join();
        }
        MessageBoxW(nullptr, L"Audio could not be started. The game will run without sound.",
                    L"Bruce Lee", MB_ICONWARNING);
        return true;
    }
    audioOn = true;
    return true;
}

void Stop() {
    audioOn = false;
    run = false;
    if (thread.joinable()) {
        thread.join();
    }
}

}  // namespace audio

bool StartAudio() {
    return audio::Start();
}

void StopAudio() {
    audio::Stop();
}

void PlayEffect(int id) {
    audio::Play(id);
}

void SilenceSound() {
    audio::Silence();
}

void SetAudioPaused(bool paused) {
    audio::held = paused;
}

void StartMenuMusic() {
    audio::StartTune();
}

void StopMenuMusic() {
    audio::StopTune();
}

#ifdef BRUCE_SOUND_TEST

namespace {

void Expect(bool ok, const char* text, int& fails) {
    if (!ok) {
        std::printf("FAIL %s\n", text);
        fails++;
    }
}

void WriteWav(const char* path, const std::vector<int16_t>& samples) {
    FILE* file = nullptr;
    if (fopen_s(&file, path, "wb") != 0 || file == nullptr) {
        std::printf("could not write %s\n", path);
        return;
    }
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    const uint32_t chunk = 36 + dataBytes;
    const uint16_t channels = 1;
    const uint32_t rate = 48000;
    const uint16_t bits = 16;
    const uint32_t byteRate = rate * 2;
    const uint16_t align = 2;
    std::fwrite("RIFF", 1, 4, file);
    std::fwrite(&chunk, 4, 1, file);
    std::fwrite("WAVEfmt ", 1, 8, file);
    const uint32_t fmtSize = 16;
    const uint16_t pcm = 1;
    std::fwrite(&fmtSize, 4, 1, file);
    std::fwrite(&pcm, 2, 1, file);
    std::fwrite(&channels, 2, 1, file);
    std::fwrite(&rate, 4, 1, file);
    std::fwrite(&byteRate, 4, 1, file);
    std::fwrite(&align, 2, 1, file);
    std::fwrite(&bits, 2, 1, file);
    std::fwrite("data", 1, 4, file);
    std::fwrite(&dataBytes, 4, 1, file);
    std::fwrite(samples.data(), 2, samples.size(), file);
    std::fclose(file);
}

void Level(const std::vector<int16_t>& samples, int& peak, double& rms) {
    double sum = 0;
    peak = 0;
    for (int16_t sample : samples) {
        const int mag = sample < 0 ? -sample : sample;
        if (mag > peak) {
            peak = mag;
        }
        sum += static_cast<double>(sample) * sample;
    }
    rms = std::sqrt(sum / static_cast<double>(samples.size()));
}

uint16_t Note(int index) {
    return static_cast<uint16_t>(audio::mem[0xE200 + index] | (audio::mem[0xE254 + index] << 8));
}

}  // namespace

int main() {
    int fails = 0;
    if (!audio::LoadImage()) {
        std::printf("FAIL sound.bin\n");
        return 1;
    }
    Expect(audio::mem[0x2509] == 0xA9 && audio::mem[0xE000] == 0x4C, "image signature", fails);
    audio::audioOn = true;

    audio::y = 0x84;
    audio::Call(0x2509);
    Expect(audio::mem[0xEB00] == 0x84, "kick queued", fails);
    Expect(audio::mem[0xD418] == 0x15, "volume 5", fails);
    Expect(audio::mem[0x2581] == audio::mem[0x2572 + 4], "kick priority", fails);
    Expect(!audio::fault, "play_sound fault", fails);

    audio::Service();
    const int steps = audio::lastSteps;
    Expect(audio::sid.v[0].freq == Note(0x26), "frame 1 note $26", fails);
    Expect(audio::sid.reg[0x04] == 0x81, "frame 1 wave", fails);
    Expect(audio::sid.reg[0x05] == 0x11 && audio::sid.reg[0x06] == 0x97, "frame 1 adsr", fails);
    Expect(audio::sid.reg[0x18] == 0x15, "frame 1 volume", fails);
    Expect(audio::mem[0xEB00] == 0x01, "slot started", fails);
    for (int i = 0; i < 4; ++i) {
        audio::Service();
    }
    Expect(audio::sid.v[0].freq == Note(0x26), "frame 5 still $26", fails);
    audio::Service();
    Expect(audio::sid.v[0].freq == Note(0x2B), "frame 6 note $2B", fails);
    Expect(!audio::fault, "voice fault", fails);

    for (int i = 0; i < 5000; ++i) {
        audio::sid.Clock();
    }
    Expect(audio::sid.v[0].env > 50, "envelope rose", fails);
    std::printf("kick steps %d freq %04X wave %02X adsr %02X %02X env %d fault %02X@%04X\n",
                steps, audio::sid.v[0].freq, audio::sid.reg[0x04], audio::sid.reg[0x05],
                audio::sid.reg[0x06], audio::sid.v[0].env, audio::faultOp, audio::faultPc);

    audio::sid.Reset();
    audio::hp = 0;
    audio::prevIn = 0;
    audio::framePos = 0;
    audio::cycleAcc = 0;
    audio::mode = audio::Mode::Effects;
    audio::Call(0x255E);
    audio::y = 0x84;
    audio::Call(0x2509);
    std::vector<int16_t> kick(48000);
    for (int i = 0; i < 48000; ++i) {
        kick[static_cast<size_t>(i)] = audio::NextSample();
    }
    int kickPeak = 0;
    double kickRms = 0;
    Level(kick, kickPeak, kickRms);
    WriteWav("tmp/kick.wav", kick);
    Expect(!audio::fault && kickRms > 30.0, "kick audible", fails);
    std::printf("kick rms %.1f peak %d\n", kickRms, kickPeak);

    audio::sid.Reset();
    audio::hp = 0;
    audio::prevIn = 0;
    audio::framePos = 0;
    audio::cycleAcc = 0;
    audio::Call(0x255E);
    audio::mem[0xE314] = 3;
    audio::mem[0xE870] = 0;
    audio::mem[0xE871] = 0xFF;
    audio::Call(0xE003);
    const int tuneSteps = audio::lastSteps;
    audio::mode = audio::Mode::Music;
    std::vector<int16_t> tune(48000 * 4);
    int changes = 0;
    int maxRoute = 0;
    int maxVol = 0;
    uint16_t prev0 = audio::sid.v[0].freq;
    uint16_t prev1 = audio::sid.v[1].freq;
    uint16_t prev2 = audio::sid.v[2].freq;
    uint16_t saw0 = 0;
    uint16_t saw1 = 0;
    uint16_t saw2 = 0;
    bool anyFreq = false;
    std::printf("tune lda %02X %02X %02X / %02X %02X %02X / %02X %02X %02X\n",
                audio::mem[0xE3DE], audio::mem[0xE3DF], audio::mem[0xE3E0],
                audio::mem[0xE3E2], audio::mem[0xE3E3], audio::mem[0xE3E4],
                audio::mem[0xE3E6], audio::mem[0xE3E7], audio::mem[0xE3E8]);
    for (int i = 0; i < static_cast<int>(tune.size()); ++i) {
        tune[static_cast<size_t>(i)] = audio::NextSample();
        if (audio::sid.v[0].freq > saw0) {
            saw0 = audio::sid.v[0].freq;
        }
        if (audio::sid.v[1].freq > saw1) {
            saw1 = audio::sid.v[1].freq;
        }
        if (audio::sid.v[2].freq > saw2) {
            saw2 = audio::sid.v[2].freq;
        }
        if (audio::sid.Volume() > maxVol) {
            maxVol = audio::sid.Volume();
        }
        if ((i % 960) == 959) {
            if (audio::sid.v[0].freq != prev0 || audio::sid.v[1].freq != prev1 ||
                audio::sid.v[2].freq != prev2) {
                changes++;
            }
            prev0 = audio::sid.v[0].freq;
            prev1 = audio::sid.v[1].freq;
            prev2 = audio::sid.v[2].freq;
            if (prev0 || prev1 || prev2) {
                anyFreq = true;
            }
            if (audio::sid.Route() > maxRoute) {
                maxRoute = audio::sid.Route();
            }
        }
    }
    int tunePeak = 0;
    double tuneRms = 0;
    Level(tune, tunePeak, tuneRms);
    WriteWav("tmp/tune.wav", tune);
    Expect(!audio::fault, "tune fault", fails);
    Expect(anyFreq && changes > 0, "tune moves", fails);
    Expect(tuneRms > 30.0, "tune audible", fails);
    std::printf("tune steps %d changes %d route %d vol %d rms %.1f peak %d freq %04X %04X %04X max %04X %04X %04X wave %02X %02X %02X fault %02X@%04X\n",
                tuneSteps, changes, maxRoute, maxVol, tuneRms, tunePeak, prev0, prev1, prev2,
                saw0, saw1, saw2, audio::sid.reg[0x04], audio::sid.reg[0x0B], audio::sid.reg[0x12],
                audio::faultOp, audio::faultPc);
    if (audio::fault) {
        const int at = audio::faultPc;
        std::printf("bytes");
        for (int i = 0; i < 8; ++i) {
            std::printf(" %02X", audio::mem[static_cast<uint16_t>(at + i)]);
        }
        std::printf("\n");
    }
    if (fails == 0) {
        std::printf("sound test ok\n");
    }
    return fails == 0 ? 0 : 1;
}

#endif
