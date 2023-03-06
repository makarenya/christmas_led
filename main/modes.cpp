#include "modes.h"

#include <random>
#include <cmath>
#include <memory>

class Static : public IMode {
public:
    double step(std::mt19937& generator) override {
        return 1.0;
    }
};

class Dynamic : public IMode {
    uint32_t Duration = 0;
    uint32_t Period = 0;

public:
    explicit Dynamic(uint32_t period) : Period{period} {
    }

    double step(std::mt19937& generator) override {
        if (++Duration == Period) {
            Duration = 0;
        }
        float x = static_cast<float>(Duration) / static_cast<float>(Period);
        return 0.5 * sin(2 * M_PI * x) + 0.5;
    }
};

class Perlin {
    double Start;
    double Stop;
    uint32_t Duration;
    uint32_t Period;

public:
    explicit Perlin(uint32_t period)
            : Start{0}
            , Stop{0}
            , Duration{0}
            , Period{period} {
    }

    double step(std::mt19937& generator) {
        ++Duration;
        double x = static_cast<float>(Duration) / static_cast<float>(Period);
        double value = 2 * (Start - Stop) * x * x * x * x - (3 * Start - 5 * Stop) * x * x * x - 3 * Stop * x * x + Start * x;
        if (Duration >= Period) {
            Duration -= Period;
            Start = Stop;
            Stop = std::normal_distribution<double>()(generator);
        }
        return value;
    }
};

class Fireplace : public IMode {
    Perlin slow{120};
    Perlin middle{60};
    Perlin fast{30};

public:
    double step(std::mt19937& generator) override {
        return slow.step(generator) * .12 + middle.step(generator) * .12 + fast.step(generator) * .12 + .7;
    }
};

class PidNoise {
    double Proportional = 0;
    double Differential = 0;
    double Scale = 0;
    double Velocity = 0;
    double Value = 0;

public:
    void setup(double proportional, double differential, double scale) {
        Proportional = proportional;
        Differential = differential;
        Scale = scale;
    }

    double step(std::mt19937& generator) {
        auto rnd = std::normal_distribution<double> ()(generator);
        Velocity += rnd * Scale - Differential * Velocity - Proportional * Value;
        Value += Velocity;
        return Value;
    }
};

const static int Kernel[] = {
    70, 17, 9, 3, 1,
    30, 55, 10, 4, 1,
    7, 15, 70, 6, 2,
    7, 18, 20, 50, 5,
    10, 30, 28, 20, 2
};

class Candle : public IMode {
    uint32_t Mode = 0;
    uint32_t Latency = 0;

    PidNoise fast{};
    PidNoise slow{};

public:
    double step(std::mt19937& generator) override {
        if (Latency++ == 100) {
            Latency = 0;
            Mode = searchMode(generator);
            setFast(Mode);
            setSlow(Mode);
        }
        return fast.step(generator) + slow.step(generator) + .6;
    }

private:
    uint32_t searchMode(std::mt19937& generator) const {
        auto r = std::uniform_int_distribution<uint32_t>(0, 99)(generator);
        for (uint32_t i = 0; i < 5; ++i) {
            int k = Kernel[i + Mode * 5];
            if (r < k)
                return i;
            r -= k;
        }
        return 0;
    }

    void setFast(uint32_t mode) {
        switch(mode) {
            default: fast.setup(.001,  .08,   0); break;
            case 1: fast.setup(.008, .06,  .0003); break;
            case 2: fast.setup(.02,  .04,  .001); break;
            case 3: fast.setup(.05,   .02,  .002); break;
            case 4: fast.setup(.2, .01, .01); break;
        }
    }

    void setSlow(uint32_t mode) {
        switch(mode) {
            default: slow.setup(.00001,   .01, .000005); break;
            case 1:  slow.setup(.0001,    .01, .00003); break;
            case 2:  slow.setup(.0003,    .01, .00005); break;
            case 3:  slow.setup(.0005,    .01, .00007); break;
            case 4:  slow.setup(.001,     .01, .0001); break;
        }
    }
};

std::shared_ptr<IMode> CreateStatic() {
    return std::make_shared<Static>();
}

std::shared_ptr<IMode> CreateDynamic(uint32_t period) {
    return std::make_shared<Dynamic>(period);
}

std::shared_ptr<IMode> CreateFireplace() {
    return std::make_shared<Fireplace>();
}

std::shared_ptr<IMode> CreateCandle() {
    return std::make_shared<Candle>();
}