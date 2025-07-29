//
// RT64
//

#include "rt64_render_target_manager.h"

#include "xxHash/xxh3.h"

namespace RT64 {
    // RenderTargetKey

    RenderTargetKey::RenderTargetKey(uint32_t address, uint32_t width, uint32_t siz, Framebuffer::Type fbType) {
        this->address = address;
        this->width = width;
        this->siz = siz;
        this->fbType = fbType;
    }

    uint64_t RenderTargetKey::hash() const {
        return XXH3_64bits(this, sizeof(RenderTargetKey));
    }

    bool RenderTargetKey::isEmpty() const {
        return (width == 0) || (fbType == Framebuffer::Type::None);
    }
    
    // RenderTargetManager

    RenderTargetManager::RenderTargetManager() { }

    void RenderTargetManager::setMultisampling(const RenderMultisampling &multisampling) {
        this->multisampling = multisampling;
    }

    void RenderTargetManager::setUsesHDR(bool usesHDR) {
        this->usesHDR = usesHDR;
    }

    RenderTarget &RenderTargetManager::get(const RenderTargetKey &key, bool ignoreOverrides) {
        const uint64_t keyHash = key.hash();
        if (!ignoreOverrides) {
            auto overrideIt = overrideMap.find(keyHash);
            if (overrideIt != overrideMap.end()) {
                return *overrideIt->second;
            }
        }

        std::unique_ptr<RenderTarget> &target = targetMap[keyHash];
        if (target != nullptr) {
            return *target;
        }

        target = std::make_unique<RenderTarget>(key.address, key.fbType, multisampling, usesHDR);
        return *target;
    }
    
    void RenderTargetManager::destroyAll() {
        targetMap.clear();
    }

    void RenderTargetManager::setOverride(const RenderTargetKey &key, RenderTarget *target) {
        overrideMap[key.hash()] = target;
    }

    void RenderTargetManager::removeOverride(const RenderTargetKey &key) {
        overrideMap.erase(key.hash());
    }

    // RenderFramebufferKey

    uint64_t RenderFramebufferKey::hash() const {
        return XXH3_64bits(this, sizeof(RenderFramebufferKey));
    }

    // RenderFramebufferStorage

    void RenderFramebufferStorage::setup(RenderDevice *device, const RenderFramebufferKey &framebufferKey, RenderTarget *colorTarget, RenderTarget *depthTarget) {
        if ((depthTarget != nullptr) && (colorTarget != nullptr)) {
            assert((depthTarget->width >= colorTarget->width) && (depthTarget->height >= colorTarget->height) && "Depth target must be equal or bigger to the color target.");
        }

        this->colorTarget = colorTarget;
        this->depthTarget = depthTarget;
        this->framebufferKey = framebufferKey;
        
        colorTargetRevision = (colorTarget != nullptr) ? colorTarget->textureRevision : 0;
        depthTargetRevision = (depthTarget != nullptr) ? depthTarget->textureRevision : 0;

        RenderFramebufferDesc framebufferDesc;
        const RenderTexture *colorTargetPtr = (colorTarget != nullptr) ? colorTarget->texture.get() : nullptr;
        framebufferDesc.colorAttachments = &colorTargetPtr;
        framebufferDesc.colorAttachmentsCount = (colorTarget != nullptr) ? 1 : 0;
        framebufferDesc.depthAttachment = (depthTarget != nullptr) ? depthTarget->texture.get() : nullptr;
        colorDepthWrite = device->createFramebuffer(framebufferDesc);

        if (framebufferDesc.depthAttachment != nullptr) {
            // Create the depth read-only version of the framebuffer.
            framebufferDesc.depthAttachmentReadOnly = true;
            colorWriteDepthRead = device->createFramebuffer(framebufferDesc);
        }
    }

    // RenderFramebufferManager

    RenderFramebufferManager::RenderFramebufferManager(RenderDevice *device) {
        assert(device != nullptr);

        this->device = device;
    }

    RenderFramebufferStorage &RenderFramebufferManager::get(const RenderFramebufferKey &framebufferKey, RenderTarget *colorTarget, RenderTarget *depthTarget) {
        assert(framebufferKey.colorTargetKey.isEmpty() || (colorTarget != nullptr));
        assert(framebufferKey.depthTargetKey.isEmpty() || (depthTarget != nullptr));

        const uint64_t keyHash = framebufferKey.hash();
        RenderFramebufferStorage &storage = storageMap[keyHash];
        if (storage.colorTarget != nullptr || storage.depthTarget != nullptr) {
            assert((storage.colorTarget == colorTarget) && "Storage does not match the requested color target.");
            assert((storage.depthTarget == depthTarget) && "Storage does not match the requested depth target.");
            assert((colorTarget == nullptr) || (storage.colorTargetRevision == colorTarget->textureRevision) && "Storage does not match the requested color texture revision.");
            assert((depthTarget == nullptr) || (storage.depthTargetRevision == depthTarget->textureRevision) && "Storage does not match the requested depth texture revision.");
            return storage;
        }

        storage.setup(device, framebufferKey, colorTarget, depthTarget);

        return storage;
    }

    void RenderFramebufferManager::destroyAll() {
        storageMap.clear();
    }

    void RenderFramebufferManager::destroyAllWithRenderTarget(RenderTarget *renderTarget) {
        auto it = storageMap.begin();
        while (it != storageMap.end()) {
            if ((it->second.colorTarget == renderTarget) || (it->second.depthTarget == renderTarget)) {
                it = storageMap.erase(it);
            }
            else {
                it++;
            }
        }
    }
};