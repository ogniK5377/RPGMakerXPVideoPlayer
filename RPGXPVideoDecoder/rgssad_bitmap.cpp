#include <algorithm>
#include "rgssad_bitmap.h"

namespace RPGMaker {

Bitmap::Bitmap(EngineAddr base) {
    layout = static_cast<MemoryLayout*>(base);
    width = layout->base->obj->bitmap_info->width;
    height = layout->base->obj->bitmap_info->height;
}
Bitmap::~Bitmap() = default;

void Bitmap::WriteLine(void* data, std::size_t size, u32 line) {
    if (line >= height) {
        return;
    }

    if (IsDisposed()) {
        return;
    }

    const auto copy_size = std::min<std::size_t>(size, width * sizeof(Color));
    const auto line_index = GetLineIndex(line);

    std::memcpy(static_cast<char*>(layout->base->obj->bitmap_data) - line_index, data, copy_size);
}

bool Bitmap::WriteBuffer(void* data, std::size_t size) {
    if (IsDisposed()) {
        return false;
    }
    const auto line_index = GetLineIndex(height - 1);
    const auto copy_size = std::min<std::size_t>(size, height * width * sizeof(Color));

    std::memcpy(static_cast<char*>(layout->base->obj->bitmap_data) - line_index, data, copy_size);

    return true;
}

bool Bitmap::WriteBufferFlipped(void* data, std::size_t size) {
    if (IsDisposed()) {
        return false;
    }

    if (size != (width * height * sizeof(Color))) {
        return false;
    }

    const auto line_copy_size = width * sizeof(Color);
    for (std::size_t y = 0; y < height; y++) {
        const auto line_index = GetLineIndex(y);
        const auto reversed_line_index = GetLineIndex(height - 1 - y);

        std::memcpy(static_cast<char*>(layout->base->obj->bitmap_data) - line_index,
                    static_cast<char*>(data) + line_index, line_copy_size);
    }
    return true;
}

std::size_t Bitmap::GetWidth() const {
    return width;
}

std::size_t Bitmap::GetHeight() const {
    return height;
}

bool Bitmap::IsDisposed() const {
    return layout->base->obj == nullptr;
}

std::size_t Bitmap::GetLineIndex(u32 line) const {
    return (width * sizeof(Color)) * line;
}

} // namespace RPGMaker
