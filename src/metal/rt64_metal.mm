//
// Created by David Chavez on 5/13/24.
//

#include "rt64_metal.h"

namespace RT64 {
    // VulkanInterface

    MetalInterface::MetalInterface() {
        // We only have one device on Metal atm, so we create it here.
        device = MTLCreateSystemDefaultDevice();
        capabilities.shaderFormat = RenderShaderFormat::METAL;
    }

    MetalInterface::~MetalInterface() {
        // TODO: Unimplemented.
    }

    std::unique_ptr<RenderDevice> MetalInterface::createDevice() {
        // TODO: Should we error if more than one device is attempted to be created?
        std::unique_ptr<MetalDevice> createdDevice = std::make_unique<MetalDevice>(this);
        return createdDevice->isValid() ? std::move(createdDevice) : nullptr;
    }

    const RenderInterfaceCapabilities &MetalInterface::getCapabilities() const {
        return capabilities;
    }

    bool MetalInterface::isValid() const {
        // TODO: Unimplemented.
        return true;
    }

    // Global creation function.

    std::unique_ptr<RenderInterface> CreateVulkanInterface() {
        std::unique_ptr<MetalInterface> createdInterface = std::make_unique<MetalInterface>();
        return createdInterface->isValid() ? std::move(createdInterface) : nullptr;
    }
}