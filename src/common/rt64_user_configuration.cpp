//
// RT64
//

#include "rt64_user_configuration.h"

#include <iomanip>

namespace RT64 {
#if defined(_WIN32)
    UserConfiguration::GraphicsAPI UserConfiguration::DefaultGraphicsAPI = UserConfiguration::GraphicsAPI::D3D12;
#else
    UserConfiguration::GraphicsAPI UserConfiguration::DefaultGraphicsAPI = UserConfiguration::GraphicsAPI::Vulkan;
#endif

    void to_json(json &j, const UserConfiguration &cfg) {
        j["graphicsAPI"] = cfg.graphicsAPI;
        j["resolution"] = cfg.resolution;
        j["antialiasing"] = cfg.antialiasing;
        j["resolutionMultiplier"] = cfg.resolutionMultiplier;
        j["downsampleMultiplier"] = cfg.downsampleMultiplier;
        j["filtering"] = cfg.filtering;
        j["aspectRatio"] = cfg.aspectRatio;
        j["aspectTarget"] = cfg.aspectTarget;
        j["extAspectRatio"] = cfg.extAspectRatio;
        j["extAspectTarget"] = cfg.extAspectTarget;
        j["upscale2D"] = cfg.upscale2D;
        j["threePointFiltering"] = cfg.threePointFiltering;
        j["refreshRate"] = cfg.refreshRate;
        j["refreshRateTarget"] = cfg.refreshRateTarget;
        j["internalColorFormat"] = cfg.internalColorFormat;
        j["idleWorkActive"] = cfg.idleWorkActive;
        j["developerMode"] = cfg.developerMode;
    }

    void from_json(const json &j, UserConfiguration &cfg) {
        UserConfiguration defaultCfg;
        cfg.graphicsAPI = j.value("graphicsAPI", defaultCfg.graphicsAPI);
        cfg.resolution = j.value("resolution", defaultCfg.resolution);
        cfg.antialiasing = j.value("antialiasing", defaultCfg.antialiasing);
        cfg.resolutionMultiplier = j.value("resolutionMultiplier", defaultCfg.resolutionMultiplier);
        cfg.downsampleMultiplier = j.value("downsampleMultiplier", defaultCfg.downsampleMultiplier);
        cfg.filtering = j.value("filtering", defaultCfg.filtering);
        cfg.aspectRatio = j.value("aspectRatio", defaultCfg.aspectRatio);
        cfg.aspectTarget = j.value("aspectTarget", defaultCfg.aspectTarget);
        cfg.extAspectRatio = j.value("extAspectRatio", defaultCfg.extAspectRatio);
        cfg.extAspectTarget = j.value("extAspectTarget", defaultCfg.extAspectTarget);
        cfg.upscale2D = j.value("upscale2D", defaultCfg.upscale2D);
        cfg.threePointFiltering = j.value("threePointFiltering", defaultCfg.threePointFiltering);
        cfg.refreshRate = j.value("refreshRate", defaultCfg.refreshRate);
        cfg.refreshRateTarget = j.value("refreshRateTarget", defaultCfg.refreshRateTarget);
        cfg.internalColorFormat = j.value("internalColorFormat", defaultCfg.internalColorFormat);
        cfg.idleWorkActive = j.value("idleWorkActive", defaultCfg.idleWorkActive);
        cfg.developerMode = j.value("developerMode", defaultCfg.developerMode);
    }

    template <typename T>
    void clampEnum(T &e) {
        e = std::clamp(e, T(0), T(int(T::OptionCount) - 1));
    }

    // Configuration
    
    const int UserConfiguration::ResolutionMultiplierLimit = 32;

    UserConfiguration::UserConfiguration() {
        graphicsAPI = DefaultGraphicsAPI;
        resolution = Resolution::WindowIntegerScale;
        antialiasing = Antialiasing::None;
        resolutionMultiplier = 2.0f;
        downsampleMultiplier = 1;
        filtering = Filtering::AntiAliasedPixelScaling;
        aspectRatio = AspectRatio::Original;
        aspectTarget = 16.0f / 9.0f;
        extAspectRatio = AspectRatio::Original;
        extAspectTarget = 16.0f / 9.0f;
        upscale2D = Upscale2D::ScaledOnly;
        threePointFiltering = true;
        refreshRate = RefreshRate::Original;
        refreshRateTarget = 60;
        internalColorFormat = InternalColorFormat::Automatic;
        idleWorkActive = true;
        developerMode = false;
    }

    void UserConfiguration::validate() {
        clampEnum<GraphicsAPI>(graphicsAPI);
        clampEnum<Resolution>(resolution);
        clampEnum<Antialiasing>(antialiasing);
        clampEnum<Filtering>(filtering);
        clampEnum<AspectRatio>(aspectRatio);
        clampEnum<AspectRatio>(extAspectRatio);
        clampEnum<Upscale2D>(upscale2D);
        clampEnum<RefreshRate>(refreshRate);
        clampEnum<InternalColorFormat>(internalColorFormat);
        resolutionMultiplier = std::clamp<double>(resolutionMultiplier, 0.0f, ResolutionMultiplierLimit);
        downsampleMultiplier = std::clamp<int>(downsampleMultiplier, 1, ResolutionMultiplierLimit);
        aspectTarget = std::clamp<double>(aspectTarget, 0.1f, 100.0f);
        extAspectTarget = std::clamp<double>(extAspectTarget, 0.1f, 100.0f);
        refreshRateTarget = std::clamp<int>(refreshRateTarget, 10, 1000);
    }

    uint32_t UserConfiguration::msaaSampleCount() const {
        return UserConfiguration::msaaSampleCount(antialiasing);
    }

    uint32_t UserConfiguration::msaaSampleCount(Antialiasing antialiasing) {
        switch (antialiasing) {
        case Antialiasing::MSAA2X:
            return 2;
        case Antialiasing::MSAA4X:
            return 4;
        case Antialiasing::MSAA8X:
            return 8;
        default:
            return 1;
        }
    }

    // ConfigurationJSON

    bool ConfigurationJSON::read(UserConfiguration &cfg, std::istream &stream) {
        try {
            json jroot;
            stream >> jroot;
            cfg = jroot.value("configuration", UserConfiguration());
            return !stream.bad();
        }
        catch (const nlohmann::detail::exception &e) {
            fprintf(stderr, "JSON parsing error: %s\n", e.what());
            cfg = UserConfiguration();
            return false;
        }
    }

    bool ConfigurationJSON::write(const UserConfiguration &cfg, std::ostream &stream) {
        json jroot;
        jroot["configuration"] = cfg;
        stream << std::setw(4) << jroot << std::endl;
        return !stream.bad();
    }
};