//
// RT64
//

#pragma once

#include <json/json.hpp>

using json = nlohmann::json;

namespace RT64 {
    struct UserConfiguration {
        static const int ResolutionMultiplierLimit;

        enum class GraphicsAPI {
            D3D12,
            Vulkan,
            OptionCount
        };

        static GraphicsAPI DefaultGraphicsAPI;

        enum class Resolution {
            Original,
            WindowIntegerScale,
            Manual,
            OptionCount
        };

        enum class Antialiasing {
            None,
            MSAA2X,
            MSAA4X,
            MSAA8X,
            OptionCount
        };

        enum class Filtering {
            Nearest,
            Linear,
            AntiAliasedPixelScaling,
            OptionCount
        };

        enum class AspectRatio {
            Original,
            Expand,
            Manual,
            OptionCount
        };

        enum class Upscale2D {
            Original,
            ScaledOnly,
            All,
            OptionCount
        };

        enum class RefreshRate {
            Original,
            Display,
            Manual,
            OptionCount
        };

        GraphicsAPI graphicsAPI;
        Resolution resolution;
        Antialiasing antialiasing;
        double resolutionMultiplier;
        int downsampleMultiplier;
        Filtering filtering;
        AspectRatio aspectRatio;
        double aspectTarget;
        AspectRatio extAspectRatio;
        double extAspectTarget;
        Upscale2D upscale2D;
        bool threePointFiltering;
        RefreshRate refreshRate;
        int refreshRateTarget;
        bool idleWorkActive;
        bool developerMode;

        UserConfiguration();
        void validate();
        uint32_t msaaSampleCount() const;
        static uint32_t msaaSampleCount(Antialiasing antialiasing);
    };

    extern void to_json(json &j, const UserConfiguration &cfg);
    extern void from_json(const json &j, UserConfiguration &cfg);

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::GraphicsAPI, {
        { UserConfiguration::GraphicsAPI::D3D12, "D3D12" },
        { UserConfiguration::GraphicsAPI::Vulkan, "Vulkan" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::Resolution, {
        { UserConfiguration::Resolution::Original, "Original" },
        { UserConfiguration::Resolution::WindowIntegerScale, "WindowIntegerScale" },
        { UserConfiguration::Resolution::Manual, "Manual" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::AspectRatio, {
        { UserConfiguration::AspectRatio::Original, "Original" },
        { UserConfiguration::AspectRatio::Expand, "Expand" },
        { UserConfiguration::AspectRatio::Manual, "Manual" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::Antialiasing, {
        { UserConfiguration::Antialiasing::None, "None" },
        { UserConfiguration::Antialiasing::MSAA2X, "MSAA2X" },
        { UserConfiguration::Antialiasing::MSAA4X, "MSAA4X" },
        { UserConfiguration::Antialiasing::MSAA8X, "MSAA8X" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::Filtering, {
        { UserConfiguration::Filtering::Nearest, "Nearest" },
        { UserConfiguration::Filtering::Linear, "Linear" },
        { UserConfiguration::Filtering::AntiAliasedPixelScaling, "AntiAliasedPixelScaling" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::Upscale2D, {
        { UserConfiguration::Upscale2D::Original, "Original" },
        { UserConfiguration::Upscale2D::ScaledOnly, "ScaledOnly" },
        { UserConfiguration::Upscale2D::All, "All" }
    });

    NLOHMANN_JSON_SERIALIZE_ENUM(UserConfiguration::RefreshRate, {
        { UserConfiguration::RefreshRate::Original, "Original" },
        { UserConfiguration::RefreshRate::Display, "Display" },
        { UserConfiguration::RefreshRate::Manual, "Manual" }
    });

    struct ConfigurationJSON {
        static bool read(UserConfiguration &cfg, std::istream &stream);
        static bool write(const UserConfiguration &cfg, std::ostream &stream);
    };
};