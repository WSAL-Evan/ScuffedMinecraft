#pragma once

namespace WillowVox
{
    enum class VertexAttributeType
    {
        FLOAT32,
        FLOAT32_2,
        FLOAT32_3,
        FLOAT32_4,

        INT32,
        INT32_2,
        INT32_3,
        INT32_4,

        UINT32,
        UINT32_2,
        UINT32_3,
        UINT32_4,

        INT16,
        INT16_2,
        INT16_3,
        INT16_4,

        UINT16,
        UINT16_2,
        UINT16_3,
        UINT16_4,

        INT8,
        INT8_2,
        INT8_3,
        INT8_4,

        UINT8,
        UINT8_2,
        UINT8_3,
        UINT8_4
    };

    struct VertexAttribute
    {
        uint32_t location;
        VertexAttributeType format;
        uint32_t offset;
    };
}