//
// RT64
//

#pragma once

#include <unordered_map>

#include "common/rt64_common.h"
#include "hle/rt64_framebuffer.h"

#include "rt64_render_target.h"

namespace RT64 {
    struct RenderTargetKey {
        uint32_t address = 0;
        uint32_t width = 0;
        uint32_t siz = 0;
        Framebuffer::Type fbType = Framebuffer::Type::None;

        RenderTargetKey() = default;
        RenderTargetKey(uint32_t address, uint32_t width, uint32_t siz, Framebuffer::Type fbType);
        uint64_t hash() const;
        bool isEmpty() const;
    };

    struct RenderTargetManager {
        std::unordered_map<uint64_t, std::unique_ptr<RenderTarget>> targetMap;
        std::unordered_map<uint64_t, RenderTarget *> overrideMap;
        RenderMultisampling multisampling;
        bool usesHDR = false;

        RenderTargetManager();
        void setMultisampling(const RenderMultisampling &multisampling);
        void setUsesHDR(bool usesHDR);
        RenderTarget &get(const RenderTargetKey &key, bool ignoreOverrides = false);
        void destroyAll();
        void setOverride(const RenderTargetKey &key, RenderTarget *target);
        void removeOverride(const RenderTargetKey &key);
    };

    struct RenderFramebufferKey {
        RenderTargetKey colorTargetKey;
        RenderTargetKey depthTargetKey;
        uint32_t modifierKey = 0;

        uint64_t hash() const;
    };

    struct RenderFramebufferStorage {
        RenderFramebufferKey framebufferKey;
        RenderTarget *colorTarget = nullptr;
        RenderTarget *depthTarget = nullptr;
        std::unique_ptr<RenderFramebuffer> colorDepthWrite;
        std::unique_ptr<RenderFramebuffer> colorWriteDepthRead;
        uint64_t colorTargetRevision = 0;
        uint64_t depthTargetRevision = 0;

        void setup(RenderDevice *device, const RenderFramebufferKey &framebufferKey, RenderTarget *colorTarget, RenderTarget *depthTarget);
    };

    struct RenderFramebufferManager {
        RenderDevice *device = nullptr;
        std::unordered_map<uint64_t, RenderFramebufferStorage> storageMap;

        RenderFramebufferManager(RenderDevice *device);
        RenderFramebufferStorage &get(const RenderFramebufferKey &framebufferKey, RenderTarget *colorTarget, RenderTarget *depthTarget);
        void destroyAll();
        void destroyAllWithRenderTarget(RenderTarget *renderTarget);
    };
};