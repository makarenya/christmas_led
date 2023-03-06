#include <cstdint>
#include <random>
#include <memory>

class IMode {
public:
    virtual ~IMode()  = default;
    virtual double step(std::mt19937& generator) = 0;
};

std::shared_ptr<IMode> CreateStatic();
std::shared_ptr<IMode> CreateDynamic(uint32_t period);
std::shared_ptr<IMode> CreateFireplace();
std::shared_ptr<IMode> CreateCandle();
