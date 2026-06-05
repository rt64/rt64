//
// RT64
//

#pragma once

#include "common/rt64_plume.h"
#include "common/rt64_user_configuration.h"
#include "render/rt64_render_worker.h"

namespace RT64 {
    // Based off libra preset param but also keeps track of current value alongside initial to support minimal reset to default
    struct LibraRuntimeParam {
        std::string name;
        std::string description;
        float initial;
        float min;
        float max;
        float step;
        float current_value;

        LibraRuntimeParam(std::string n, std::string d, float def, float min_, float max_, float step_)
            : name(std::move(n)),
            description(std::move(d)),
            initial(def),
            min(min_), max(max_),
            step(step_),
            current_value(def) {}
    };

    struct Librashader {
        struct LibraFrameParams {
            RenderCommandList *commandList = nullptr;
            RenderFramebuffer* outputFramebuffer = nullptr;
            RenderTexture *inputTexture = nullptr;
            RenderTexture *outputTexture = nullptr;
            RenderWorker *worker = nullptr;
            RenderRect scissor;
            RenderViewport viewport;
            size_t frameCount;
        };

        UserConfiguration::GraphicsAPI gfxAPI;

        Librashader(UserConfiguration::GraphicsAPI api);
        ~Librashader();
        std::string getCurrentShader();
        std::vector<LibraRuntimeParam>& getRuntimeParams();
        bool isLoaded();
        void updateRuntimeParam(const LibraRuntimeParam param);
        bool updateShader(RenderDevice* device, const std::string desiredShader);
        bool ready();
        void reset();
        bool setup(RenderDevice* device, std::string path);
        void postprocess(const LibraFrameParams &p);
    };
}
