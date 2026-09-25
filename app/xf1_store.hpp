#pragma once

#include "xf1.hpp"

#include <filesystem>
#include <mutex>

namespace rauch
{
class IXf1Store
{
public:
    virtual ~IXf1Store()                                                                = default;
    virtual std::filesystem::path store(const Xf1Frame& frame, const Xf1Record& record) = 0;
};

class Xf1FileStore final : public IXf1Store
{
public:
    explicit Xf1FileStore(std::filesystem::path directory);
    std::filesystem::path store(const Xf1Frame& frame, const Xf1Record& record) override;

private:
    std::filesystem::path directory_;
    std::mutex            mutex_;
};
}  // namespace rauch
