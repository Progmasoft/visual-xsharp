// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "WireFuzz.hpp"

namespace
{
    // An explicit PRNG makes every CI failure reproducible on either host.
    class Random final
    {
    public:
        explicit Random(std::uint64_t seed)
            : state_(seed)
        {}

        [[nodiscard]] auto
        Next() -> std::uint64_t
        {
            state_ ^= state_ << 13U;
            state_ ^= state_ >> 7U;
            state_ ^= state_ << 17U;
            return state_;
        }

        [[nodiscard]] auto
        Index(std::size_t maximum) -> std::size_t
        {
            return static_cast<std::size_t>(Next() % maximum);
        }

    private:
        std::uint64_t state_;
    };

    void
    Mutate(std::vector<std::uint8_t> &bytes, Random &random)
    {
        // Preserve the format selector most of the time so mutations reach
        // fields after the magic and version instead of another decoder.
        if (bytes.size() == 1U)
            bytes.push_back(static_cast<std::uint8_t>(random.Next()));
        constexpr std::size_t offset = 1U;
        switch (random.Index(6U))
        {
            case 0U:
                bytes[random.Index(bytes.size())]
                    ^= static_cast<std::uint8_t>(1U << random.Index(8U));
                break;
            case 1U:
                bytes[offset + random.Index(bytes.size() - offset)]
                    = static_cast<std::uint8_t>(random.Next());
                break;
            case 2U:
                bytes.resize(offset + random.Index(bytes.size() - offset));
                break;
            case 3U:
                bytes.insert(
                    bytes.begin()
                        + static_cast<std::ptrdiff_t>(
                            offset + random.Index(bytes.size() - offset + 1U)),
                    static_cast<std::uint8_t>(random.Next()));
                break;
            case 4U:
                if (bytes.size() > offset + 1U)
                    bytes.erase(
                        bytes.begin()
                        + static_cast<std::ptrdiff_t>(
                            offset + random.Index(bytes.size() - offset)));
                break;
            default:
                // All-one counts and scalar values test checked allocations.
                bytes[offset + random.Index(bytes.size() - offset)] = 0xffU;
                break;
        }
    }

    void
    ShowFailure(const std::vector<std::uint8_t> &bytes,
                std::size_t seed,
                std::size_t iteration)
    {
        std::cerr << "wire fuzz failure: seed=" << seed
                  << " iteration=" << iteration << " input=";
        for (const auto byte : bytes)
            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(byte);
        std::cerr << std::dec << '\n';
    }

    void
    WriteCorpus(const std::vector<std::vector<std::uint8_t>> &seeds,
                const std::filesystem::path &directory)
    {
        constexpr std::string_view names[]{ "core", "coreprep", "xpp", "xmm" };
        std::filesystem::create_directories(directory);
        if (!std::filesystem::is_empty(directory))
            throw std::logic_error("Fuzz corpus destination must be empty");
        for (std::size_t index = 0U; index < seeds.size(); ++index)
        {
            std::ofstream output(directory / names[index], std::ios::binary);
            if (!output)
                throw std::runtime_error("Could not create fuzz corpus seed");
            for (const auto byte : seeds[index])
                output.put(static_cast<char>(byte));
            if (!output)
                throw std::runtime_error("Could not finish fuzz corpus seed");
        }
    }
} // namespace

int
main(int argc, char **argv)
{
    try
    {
        if (argc != 1
            && (argc != 3 || argv[1] != std::string_view("-Write-Corpus")))
            throw std::invalid_argument(
                "Use wire_fuzz_smoke [-Write-Corpus EMPTY_DIRECTORY]");
        const auto seeds = Visual::XSharp::Fuzzing::WireSeeds();
        if (seeds.size() != 4U)
            throw std::logic_error("Expected one valid seed per wire stage");
        if (argc == 3)
        {
            WriteCorpus(seeds, argv[2]);
            std::cout << "Wrote four valid wire corpus seeds\n";
            return 0;
        }

        std::size_t cases = 0U;
        for (std::size_t seedIndex = 0U; seedIndex < seeds.size(); ++seedIndex)
        {
            Random random(0x56'58'53'46'55'5a'5a'31ULL + seedIndex);
            for (std::size_t iteration = 0U; iteration < 1024U; ++iteration)
            {
                auto bytes = seeds[seedIndex];
                if (iteration != 0U)
                {
                    const auto mutations = 1U + random.Index(4U);
                    for (std::size_t index = 0U; index < mutations; ++index)
                        Mutate(bytes, random);
                }
                try
                {
                    Visual::XSharp::Fuzzing::ExerciseWire(bytes);
                }
                catch (...)
                {
                    ShowFailure(bytes, seedIndex, iteration);
                    throw;
                }
                ++cases;
            }
        }
        std::cout << "Wire mutation smoke: " << cases
                  << " deterministic cases across Core, CorePrep, Xpp, "
                     "and Xmm\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
