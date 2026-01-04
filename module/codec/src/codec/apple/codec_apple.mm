// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "skity/codec/codec.hpp"
#import "skity/io/data.hpp"
#import "skity/io/pixmap.hpp"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

namespace skity {

namespace {

bool DataIsGIF(const uint8_t* data, size_t size) {
  if (size < 6) return false;
  return (memcmp(data, "GIF89a", 6) == 0 || memcmp(data, "GIF87a", 6) == 0);
}

std::shared_ptr<Pixmap> CGImageToPixmap(CGImageRef cg_image) {
  size_t width = CGImageGetWidth(cg_image);
  size_t height = CGImageGetHeight(cg_image);

  size_t bytesPerRow = width * 4;

  size_t total_size = width * height * 4;
  auto p = malloc(total_size);

  if (p == nullptr) {
    return {};
  }

  auto data = Data::MakeFromMalloc(p, total_size);

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

  // RGBA8888, little-endian unpremultiplied alpha
  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaLast;

  CGContextRef ctx = CGBitmapContextCreate(const_cast<void*>(data->RawData()),  // dst buffer
                                           width, height,
                                           8,  // bits per component
                                           bytesPerRow, colorSpace, bitmapInfo);

  CGColorSpaceRelease(colorSpace);

  if (ctx == nil) {
    return {};
  }

  CGContextTranslateCTM(ctx, 0, height);
  CGContextScaleCTM(ctx, 1.0, -1.0);

  CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cg_image);

  CGContextRelease(ctx);

  // default is unpremultiplied RGBA format
  auto pixmap = std::make_shared<Pixmap>(std::move(data), width, height);

  return pixmap;
}

}  // namespace

class MultiFrameDecoderApple : public MultiFrameDecoder {
 public:
  MultiFrameDecoderApple(CGImageSourceRef source) : source_(source) {
    frame_count_ = static_cast<uint32_t>(CGImageSourceGetCount(source_));

    CFDictionaryRef properties = CGImageSourceCopyProperties(source_, NULL);

    CFDictionaryRef gif_dict =
        (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary);

    CFNumberRef widthNum =
        (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth);

    CFNumberRef heightNum =
        (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight);

    CFNumberGetValue(widthNum, kCFNumberIntType, &width_);
    CFNumberGetValue(heightNum, kCFNumberIntType, &height_);

    for (int32_t i = 0; i < frame_count_; i++) {
      CFDictionaryRef frame_props = CGImageSourceCopyPropertiesAtIndex(source_, i, NULL);

      CFDictionaryRef gif_frame_dict =
          (CFDictionaryRef)CFDictionaryGetValue(frame_props, kCGImagePropertyGIFDictionary);

      double duration = 0.0;

      CFNumberRef unclamped =
          (CFNumberRef)CFDictionaryGetValue(gif_frame_dict, kCGImagePropertyGIFUnclampedDelayTime);

      CFNumberRef clamped =
          (CFNumberRef)CFDictionaryGetValue(gif_frame_dict, kCGImagePropertyGIFDelayTime);

      if (unclamped) {
        CFNumberGetValue(unclamped, kCFNumberDoubleType, &duration);
      } else if (clamped) {
        CFNumberGetValue(clamped, kCFNumberDoubleType, &duration);
      }

      // WebKit / Safari requires
      if (duration < 0.01) {
        duration = 0.1;
      }

      CodecFrameInfo info{};
      info.duration = duration;
      info.rect.left = 0;
      info.rect.top = 0;
      info.rect.right = width_;
      info.rect.bottom = height_;

      frames_.emplace_back(CodecFrame(i, info));

      CFRelease(frame_props);
    }

    CFRelease(properties);
  }

  ~MultiFrameDecoderApple() override { CFRelease(source_); }

  int32_t GetWidth() const override { return width_; }

  int32_t GetHeight() const override { return height_; }

  int32_t GetFrameCount() const override { return frame_count_; }

  const CodecFrame* GetFrameInfo(int32_t frame_id) const override {
    if (frame_id >= frames_.size() || frame_id < 0) {
      return nullptr;
    }

    return frames_.data() + frame_id;
  }

  std::shared_ptr<Pixmap> DecodeFrame(const CodecFrame* frame,
                                      std::shared_ptr<Pixmap> prev_pixmap) override {
    if (frame == nullptr) {
      return {};
    }

    if (frame->GetFrameID() >= frame_count_) {
      return {};
    }

    CGImageRef cg_image = CGImageSourceCreateImageAtIndex(source_, frame->GetFrameID(), nullptr);

    if (cg_image == nil) {
      return {};
    }

    auto pixmap = CGImageToPixmap(cg_image);

    CGImageRelease(cg_image);

    return pixmap;
  }

 private:
  CGImageSourceRef source_ = nil;

  std::vector<CodecFrame> frames_ = {};

  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t frame_count_ = 0;
};

enum class TargetImageType {
  kPNG,
  kJPEG,
  kGIF,
  kUnknown,
};

class CodecApple : public Codec {
 public:
  CodecApple(TargetImageType type) : type_(type) {}

  ~CodecApple() override = default;

  std::shared_ptr<Pixmap> Decode() override;

  std::shared_ptr<MultiFrameDecoder> DecodeMultiFrame() override;

  std::shared_ptr<Data> Encode(const Pixmap* pixmap) override;

  bool RecognizeFileType(const char* header, size_t size) override {
    // assume the platform can decode everything
    return true;
  }

 private:
  TargetImageType type_;
  NSData* ns_data_ = nil;
};

std::shared_ptr<Pixmap> CodecApple::Decode() {
  if (data_ == nullptr) {
    return {};
  }

  ns_data_ = [NSData dataWithBytes:data_->RawData() length:data_->Size()];

  if (ns_data_ == nil) {
    return {};
  }

  CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)ns_data_, NULL);

  if (source == nil) {
    return {};
  }

  CGImageRef cg_image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
  CFRelease(source);

  if (cg_image == nil) {
    return {};
  }

  auto pixmap = CGImageToPixmap(cg_image);

  CGImageRelease(cg_image);

  return pixmap;
}

std::shared_ptr<MultiFrameDecoder> CodecApple::DecodeMultiFrame() {
  if (data_ == nullptr) {
    return {};
  }

  if (!DataIsGIF(data_->Bytes(), data_->Size())) {
    // only support GIF in darwin platform codec
    return {};
  }

  ns_data_ = [NSData dataWithBytes:data_->RawData() length:data_->Size()];

  if (ns_data_ == nil) {
    return {};
  }

  CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)ns_data_, NULL);

  if (source == nil) {
    return {};
  }

  size_t frame_count = CGImageSourceGetCount(source);

  if (frame_count <= 1) {
    // not a multi frame image
    CFRelease(source);

    return {};
  }

  return std::make_shared<MultiFrameDecoderApple>(source);
}

std::shared_ptr<Data> CodecApple::Encode(const Pixmap* pixmap) {
  if (pixmap == nullptr || pixmap->Width() == 0 || pixmap->Height() == 0) {
    return {};
  }

  if (type_ == TargetImageType::kGIF) {
    // do not support multiframe image encoding
    return {};
  }

  auto width = pixmap->Width();
  auto height = pixmap->Height();

  CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                               reinterpret_cast<const uint8_t*>(pixmap->Addr()),
                                               width * height * 4, kCFAllocatorNull);

  if (data == nil) {
    return {};
  }

  CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Big | kCGImageAlphaLast;  // RGBA

  CGImageRef image =
      CGImageCreate(width, height,
                    8,          // bits per component
                    32,         // bits per pixel
                    width * 4,  // bytes per row
                    colorSpace, bitmapInfo, provider, NULL, false, kCGRenderingIntentDefault);

  CGColorSpaceRelease(colorSpace);
  CFRelease(provider);
  CFRelease(data);

  if (image == nil) {
    return {};
  }

  CFMutableDataRef out_data = CFDataCreateMutable(NULL, 0);

  CGImageDestinationRef dest = CGImageDestinationCreateWithData(
      out_data, type_ == TargetImageType::kJPEG ? kUTTypeJPEG : kUTTypePNG, 1, NULL);

  CGImageDestinationAddImage(dest, image, NULL);
  CGImageDestinationFinalize(dest);

  CFRelease(dest);

  auto length = CFDataGetLength(out_data);

  auto bytes = CFDataGetBytePtr(out_data);

  auto encoded = Data::MakeWithCopy(bytes, length);

  CFRelease(out_data);
  CGImageRelease(image);

  return encoded;
}

std::shared_ptr<Codec> Codec::MakeFromData(const std::shared_ptr<Data>& data) {
  return std::make_shared<CodecApple>(TargetImageType::kUnknown);
}

std::shared_ptr<Codec> Codec::MakeGIFCodec() {
  return std::make_shared<CodecApple>(TargetImageType::kGIF);
}

std::shared_ptr<Codec> Codec::MakePngCodec() {
  return std::make_shared<CodecApple>(TargetImageType::kPNG);
}

std::shared_ptr<Codec> Codec::MakeJPEGCodec() {
  return std::make_shared<CodecApple>(TargetImageType::kJPEG);
}

std::shared_ptr<Codec> Codec::MakeWebpCodec() {
  // ignore webp when using ImageIO
  return {};
}

}  // namespace skity
