//
// RT64
//

#include "rt64_draw_call.h"

#include <cassert>
#include <memory>

namespace RT64 {
    // DrawCall

    std::string DrawCall::attributeName(DrawAttribute a) {
        switch (a) {
        case DrawAttribute::Zero:
            return "Zero";
        case DrawAttribute::UID:
            return "UID";
        case DrawAttribute::Tris:
            return "Tris";
        case DrawAttribute::Scissor:
            return "Scissor";
        case DrawAttribute::Combine:
            return "Combine";
        case DrawAttribute::Texture:
            return "Texture";
        case DrawAttribute::OtherMode:
            return "OtherMode";
        case DrawAttribute::GeometryMode:
            return "GeometryMode";
        case DrawAttribute::PrimColor:
            return "PrimColor";
        case DrawAttribute::EnvColor:
            return "EnvColor";
        case DrawAttribute::FogColor:
            return "FogColor";
        case DrawAttribute::FillColor:
            return "FillColor";
        case DrawAttribute::BlendColor:
            return "BlendColor";
        case DrawAttribute::Lights:
            return "Lights";
        case DrawAttribute::ExtendedType:
            return "Extended";
        default:
            return "Unknown";
        }
    }
    
    bool DrawCall::identityRectScale() const {
        const int16_t IdentityScale = 1024;
        return ((rectDsdx == IdentityScale) || (rectDsdx == -IdentityScale)) && ((rectDtdy == IdentityScale) || (rectDtdy == -IdentityScale));
    }

    // DrawStatus

    DrawStatus::DrawStatus() {
        reset();
    }

    DrawStatus::DrawStatus(uint32_t v) {
        changed = v;
    }

    void DrawStatus::reset() {
        clearChanges();
    }

    void DrawStatus::clearChanges() {
        changed = 0;
    }

    void DrawStatus::clearChange(DrawAttribute attribute) {
        assert(attribute < DrawAttribute::Count);
        changed &= ~(1U << static_cast<uint32_t>(attribute));
    }

    void DrawStatus::setChanged(DrawAttribute attribute) {
        assert(attribute < DrawAttribute::Count);
        changed |= (1U << static_cast<uint32_t>(attribute));
    }

    bool DrawStatus::isChanged(DrawAttribute attribute) const {
        assert(attribute < DrawAttribute::Count);
        return (changed & (1U << static_cast<uint32_t>(attribute))) != 0;
    }

    bool DrawStatus::isChanged() const {
        return changed != 0;
    }
};