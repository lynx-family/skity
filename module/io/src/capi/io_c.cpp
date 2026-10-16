// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// C ABI for the io module (Picture serialization + streams). Compiled only
// when SKITY_IO_MODULE is on: in the normal build it is part of skity-capi
// (linking skity-io); in the unified-lib experiment the io module is folded
// into libskity, so the entry points resolve inside the single .so.

#include <skity_c/skity_picture.h>
#include <skity_c/skity_stream.h>

#include <memory>
#include <skity/gpu/gpu_context.hpp>
#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>
#include <skity/recorder/display_list.hpp>
#include <skity/render/canvas.hpp>

#include "handle.hpp"

namespace {

skity::ReadStream* reader_of(skity_read_stream handle) {
  auto* w = skity::capi::resolve<skity_read_stream_s>(
      handle, SKITY_OBJECT_TYPE_READ_STREAM);
  return w ? static_cast<skity::ReadStream*>(w->impl.get()) : nullptr;
}

skity::WriteStream* writer_of(skity_write_stream handle) {
  auto* w = skity::capi::resolve<skity_write_stream_s>(
      handle, SKITY_OBJECT_TYPE_WRITE_STREAM);
  return w ? static_cast<skity::WriteStream*>(w->impl.get()) : nullptr;
}

skity::Picture* picture_of(skity_picture handle) {
  auto* w =
      skity::capi::resolve<skity_picture_s>(handle, SKITY_OBJECT_TYPE_PICTURE);
  return w ? static_cast<skity::Picture*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_read_stream skity_read_stream_create_from_file(const char* path) {
  if (path == nullptr) {
    return nullptr;
  }
  auto impl = skity::ReadStream::CreateFromFile(path);
  if (impl == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::ReadStream> shared(impl.release());
  return skity::capi::alloc_handle<skity_read_stream_s>(
      SKITY_OBJECT_TYPE_READ_STREAM, SKITY_HANDLE_OWNING, std::move(shared));
}

void skity_read_stream_destroy(skity_read_stream stream) {
  skity::capi::destroy_handle<skity_read_stream_s>(
      stream, SKITY_OBJECT_TYPE_READ_STREAM);
}

size_t skity_read_stream_read(skity_read_stream stream, void* buffer,
                              size_t size) {
  auto* s = reader_of(stream);
  return s ? s->Read(buffer, size) : 0;
}

size_t skity_read_stream_peek(skity_read_stream stream, void* buffer,
                              size_t size) {
  auto* s = reader_of(stream);
  return s ? s->Peek(buffer, size) : 0;
}

uint32_t skity_read_stream_is_at_end(skity_read_stream stream) {
  auto* s = reader_of(stream);
  return (s != nullptr && s->IsAtEnd()) ? 1u : 0u;
}

uint32_t skity_read_stream_rewind(skity_read_stream stream) {
  auto* s = reader_of(stream);
  return (s != nullptr && s->Rewind()) ? 1u : 0u;
}

skity_write_stream skity_write_stream_create_from_file(const char* path) {
  if (path == nullptr) {
    return nullptr;
  }
  auto impl = skity::WriteStream::CreateFileStream(path);
  if (impl == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::WriteStream> shared(impl.release());
  return skity::capi::alloc_handle<skity_write_stream_s>(
      SKITY_OBJECT_TYPE_WRITE_STREAM, SKITY_HANDLE_OWNING, std::move(shared));
}

void skity_write_stream_destroy(skity_write_stream stream) {
  skity::capi::destroy_handle<skity_write_stream_s>(
      stream, SKITY_OBJECT_TYPE_WRITE_STREAM);
}

uint32_t skity_write_stream_write(skity_write_stream stream, const void* buffer,
                                  size_t size) {
  auto* s = writer_of(stream);
  return (s != nullptr && s->Write(buffer, size)) ? 1u : 0u;
}

uint32_t skity_write_stream_flush(skity_write_stream stream) {
  auto* s = writer_of(stream);
  return (s != nullptr && s->Flush()) ? 1u : 0u;
}

size_t skity_write_stream_bytes_written(skity_write_stream stream) {
  auto* s = writer_of(stream);
  return s ? s->BytesWritten() : 0;
}

skity_picture skity_picture_create_from_display_list(skity_display_list dl) {
  auto list = skity::capi::get_impl<skity_display_list_s, skity::DisplayList>(
      dl, SKITY_OBJECT_TYPE_DISPLAY_LIST);
  if (list == nullptr) {
    return nullptr;
  }
  auto impl = skity::Picture::MakeFromDisplayList(list.get());
  if (impl == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Picture> shared(impl.release());
  return skity::capi::alloc_handle<skity_picture_s>(
      SKITY_OBJECT_TYPE_PICTURE, SKITY_HANDLE_OWNING, std::move(shared));
}

skity_picture skity_picture_create_from_stream(skity_read_stream stream) {
  auto* s = reader_of(stream);
  if (s == nullptr) {
    return nullptr;
  }
  auto impl = skity::Picture::MakeFromStream(*s);
  if (impl == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Picture> shared(impl.release());
  return skity::capi::alloc_handle<skity_picture_s>(
      SKITY_OBJECT_TYPE_PICTURE, SKITY_HANDLE_OWNING, std::move(shared));
}

void skity_picture_destroy(skity_picture picture) {
  skity::capi::destroy_handle<skity_picture_s>(picture,
                                               SKITY_OBJECT_TYPE_PICTURE);
}

skity_rect skity_picture_get_cull_rect(skity_picture picture) {
  auto* p = picture_of(picture);
  if (p == nullptr) {
    return skity_rect{0.f, 0.f, 0.f, 0.f};
  }
  skity::Rect rect = p->GetCullRect();
  return skity_rect{rect.Left(), rect.Top(), rect.Right(), rect.Bottom()};
}

void skity_picture_play_back(skity_picture picture, skity_canvas canvas) {
  auto* p = picture_of(picture);
  auto c = skity::capi::get_impl<skity_canvas_s, skity::Canvas>(
      canvas, SKITY_OBJECT_TYPE_CANVAS);
  if (p != nullptr && c != nullptr) {
    p->PlayBack(c.get());
  }
}

void skity_picture_serialize(skity_picture picture, skity_write_stream stream,
                             const skity_serial_proc* proc) {
  auto* p = picture_of(picture);
  auto* w = writer_of(stream);
  if (p == nullptr || w == nullptr) {
    return;
  }
  skity::SerialProc legacy_proc{};
  if (proc != nullptr) {
    if (proc->gpu_context != nullptr) {
      legacy_proc.gpu_context =
          skity::capi::get_impl<skity_context_s, skity::GPUContext>(
              proc->gpu_context, SKITY_OBJECT_TYPE_CONTEXT)
              .get();
    }
    if (proc->image_proc != nullptr) {
      legacy_proc.image_proc =
          [fn = proc->image_proc, userdata = proc->userdata](
              const skity::Image* image) -> std::shared_ptr<skity::Pixmap> {
        // Bridge the C callback: hand it a non-owning image handle and
        // unwrap the returned owning pixmap handle.
        std::shared_ptr<skity::Image> view(const_cast<skity::Image*>(image),
                                           [](skity::Image*) {});
        skity_image image_handle = skity::capi::alloc_handle<skity_image_s>(
            SKITY_OBJECT_TYPE_IMAGE, 0, std::move(view));
        skity_pixmap pixmap_handle = fn(image_handle, userdata);
        skity::capi::destroy_handle<skity_image_s>(image_handle,
                                                   SKITY_OBJECT_TYPE_IMAGE);
        if (pixmap_handle == nullptr) {
          return nullptr;
        }
        auto pixmap = skity::capi::get_impl<skity_pixmap_s, skity::Pixmap>(
            pixmap_handle, SKITY_OBJECT_TYPE_PIXMAP);
        skity::capi::destroy_handle<skity_pixmap_s>(pixmap_handle,
                                                    SKITY_OBJECT_TYPE_PIXMAP);
        return pixmap;
      };
    }
  }
  p->Serialize(*w, proc != nullptr ? &legacy_proc : nullptr);
}

}  // extern "C"
