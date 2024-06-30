//
// RT64
//

#include "rt64_common.h"

#include <cassert>
#include <cmath>

namespace RT64 {
    FILE *GlobalLogFile = nullptr;
    std::string GlobalLastError = "";

    // FixedRect

    FixedRect::FixedRect() {
        reset();
    }

    FixedRect::FixedRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
        this->ulx = ulx;
        this->uly = uly;
        this->lrx = lrx;
        this->lry = lry;
    }

    void FixedRect::reset() {
        ulx = INT32_MAX;
        uly = INT32_MAX;
        lrx = INT32_MIN;
        lry = INT32_MIN;
    }

    bool FixedRect::isEmpty() const {
        return isNull() || (lrx == ulx) || (lry == uly);
    }

    bool FixedRect::isNull() const {
        return (ulx > lrx) || (uly > lry);
    }

    void FixedRect::merge(const FixedRect &rect) {
        assert(!rect.isNull());

        ulx = std::min(ulx, rect.ulx);
        uly = std::min(uly, rect.uly);
        lrx = std::max(lrx, rect.lrx);
        lry = std::max(lry, rect.lry);
    }

    FixedRect FixedRect::scaled(float x, float y) const {
        assert(!isNull());
        assert(x >= 0.0f);
        assert(y >= 0.0f);

        return FixedRect(
            int32_t(floorf(left(false) * x)) << 2,
            int32_t(floorf(top(false) * y)) << 2,
            int32_t(ceilf(right(true) * x)) << 2,
            int32_t(ceilf(bottom(true) * y)) << 2
        );
    }

    FixedRect FixedRect::intersection(const FixedRect &rect) const {
        if (!isNull() && !rect.isNull()) {
            return {
                std::max(ulx, rect.ulx),
                std::max(uly, rect.uly),
                std::min(lrx, rect.lrx),
                std::min(lry, rect.lry)
            };
        }
        else {
            return FixedRect();
        }
    }

    bool FixedRect::contains(int32_t x, int32_t y) const {
        if (!isNull()) {
            return (x >= ulx) && (x <= lrx) && (y >= uly) && (y <= lry);
        }
        else {
            return false;
        }
    }

    bool FixedRect::fullyInside(const FixedRect &rect) const {
        assert(!isNull());
        assert(!rect.isNull());
        return (rect.ulx >= ulx) && (rect.uly >= uly) && (rect.lrx <= lrx) && (rect.lry <= lry);
    }

    int32_t FixedRect::left(bool ceil) const {
        assert(!isNull());
        return (ulx + (ceil ? 3 : 0)) >> 2;
    }

    int32_t FixedRect::top(bool ceil) const {
        assert(!isNull());
        return (uly + (ceil ? 3 : 0)) >> 2;
    }

    int32_t FixedRect::right(bool ceil) const {
        assert(!isNull());
        return (lrx + (ceil ? 3 : 0)) >> 2;
    }

    int32_t FixedRect::bottom(bool ceil) const {
        assert(!isNull());
        return (lry + (ceil ? 3 : 0)) >> 2;
    }

    int32_t FixedRect::width(bool leftCeil, bool rightCeil) const {
        assert(!isNull());
        return right(rightCeil) - left(leftCeil);
    }

    int32_t FixedRect::height(bool topCeil, bool bottomCeil) const {
        assert(!isNull());
        return bottom(bottomCeil) - top(topCeil);
    }

    // FixedMatrix

    float FixedMatrix::toFloat(uint32_t i, uint32_t j) const {
        const int xorJ = j ^ 1;
        return FixedMatrix::fixedToFloat(integer[i][xorJ], frac[i][xorJ]);
    }

    hlslpp::float4x4 FixedMatrix::toMatrix4x4() const {
        return hlslpp::float4x4(
            toFloat(0, 0), toFloat(0, 1), toFloat(0, 2), toFloat(0, 3),
            toFloat(1, 0), toFloat(1, 1), toFloat(1, 2), toFloat(1, 3),
            toFloat(2, 0), toFloat(2, 1), toFloat(2, 2), toFloat(2, 3),
            toFloat(3, 0), toFloat(3, 1), toFloat(3, 2), toFloat(3, 3)
        );
    }

    float FixedMatrix::fixedToFloat(int16_t integerValue, uint16_t fracValue) {
        const uint32_t fullWord = (uint32_t(integerValue) << 16) | fracValue;
        return int32_t(fullWord) / 65536.0f;
    }

    void FixedMatrix::modifyMatrix4x4Integer(hlslpp::float4x4 &matrix, uint32_t i, uint32_t j, int16_t value) {
        const int32_t fixedValue = int32_t(matrix[i][j] * 65536.0f);
        matrix[i][j] = fixedToFloat(value, uint16_t(fixedValue & 0xFFFFU));
    }

    void FixedMatrix::modifyMatrix4x4Fraction(hlslpp::float4x4 &matrix, uint32_t i, uint32_t j, uint16_t value) {
        const int32_t fixedValue = int32_t(matrix[i][j] * 65536.0f);
        matrix[i][j] = fixedToFloat(int16_t((fixedValue >> 16) & 0xFFFF), value);
    }
};