#pragma once
#include <Windows.h>
#include "common_types.h"

namespace RPGMaker {
struct Color {
    u8 r{};
    u8 g{};
    u8 b{};
    u8 a{255};

    void Randomize() {
        r = rand() & 255;
        g = rand() & 255;
        b = rand() & 255;
        a = 255;
    }
};
static_assert(sizeof(Color) == 4, "Color is an invalid size");

class Bitmap {
public:
    Bitmap(EngineAddr base);
    ~Bitmap();

    void WriteLine(void* data, std::size_t size, u32 line);
    bool WriteBuffer(void* data, std::size_t size);
    bool WriteBufferFlipped(void* data, std::size_t size);
    std::size_t GetWidth() const;
    std::size_t GetHeight() const;
    bool IsDisposed() const;

private:
    std::size_t GetLineIndex(u32 line) const;
    std::size_t width{};
    std::size_t height{};
    struct BitmapInfo {
        u32 unknown_0x0{}; // 0x0
        u32 width{};       // 0x4
        u32 height{};      // 0x8
    };
    static_assert(sizeof(BitmapInfo) == 0xc, "BitmapInfo is an invalid size");

    struct BitmapObject {
        void* unknown_0x0{nullptr};       // 0x0
        void* unknown_0x4{nullptr};       // 0x4
        BitmapInfo* bitmap_info{nullptr}; // 0x8
        void* bitmap_data{nullptr};       // 0xc
    };
    static_assert(sizeof(BitmapObject) == 0x10, "BitmapObject is an invalid size");

    struct Base {
        u32 unknown_0x0{};          // 0x0
        u32 unknown_0x4{};          // 0x4
        BitmapObject* obj{nullptr}; // 0x8
    };
    static_assert(sizeof(Base) == 0xc, "Base is an invalid size");

    struct MemoryLayout {
        u32 object_id{};            // 0x0
        void* unknown_0x4{nullptr}; // 0x4
        u32 unknown_0x8{};          // 0x8
        void* unknown_0xc{nullptr}; // 0xc
        Base* base{nullptr};        // 0x10
    };
    static_assert(sizeof(MemoryLayout) == 0x14, "MemoryLayout is an invalid size");

    MemoryLayout* layout = nullptr;
};

} // namespace RPGMaker
