/*
 * Copyright 2011 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/android_fonts_parser.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <skity/io/data.hpp>
#include <skity/utils/xml/xml_parser.hpp>
#include <string>
#include <utility>
#include <vector>

#include "src/base/fixed_types.hpp"
#include "src/logging.hpp"

#define LMP_SYSTEM_FONTS_FILE "/system/etc/fonts.xml"
#define FONT_FILE_PATH "/fonts/"

/** Parses a null terminated string into an integer type, checking for overflow.
 *  http://www.w3.org/TR/html-markup/datatypes.html#common.data.integer.non-negative-def
 *
 *  If the string cannot be parsed into 'value', returns false and does not
 * change 'value'.
 */
template <typename T>
static bool parse_non_negative_integer(const char* s, T* value) {
  static_assert(std::numeric_limits<T>::is_integer, "T_must_be_integer");

  if (*s == '\0') {
    return false;
  }

  const T nMax = std::numeric_limits<T>::max() / 10;
  const T dMax = std::numeric_limits<T>::max() - (nMax * 10);
  T n = 0;
  for (; *s; ++s) {
    // Check if digit
    if (*s < '0' || '9' < *s) {
      return false;
    }
    T d = *s - '0';
    // Check for overflow
    if (n > nMax || (n == nMax && d > dMax)) {
      return false;
    }
    n = (n * 10) + d;
  }
  *value = n;
  return true;
}

/** Parses a null terminated string into a signed fixed point value with bias N.
 *
 *  Like http://www.w3.org/TR/html-markup/datatypes.html#common.data.float-def ,
 *  but may start with '.' and does not support 'e'.
 * '-?((:digit:+(.:digit:+)?)|(.:digit:+))'
 *
 *  Checks for overflow.
 *  Low bit rounding is not defined (is currently truncate).
 *  Bias (N) required to allow for the sign bit and 4 bits of integer.
 *
 *  If the string cannot be parsed into 'value', returns false and does not
 * change 'value'.
 */
template <int N, typename T>
static bool parse_fixed(const char* s, T* value) {
  static_assert(std::numeric_limits<T>::is_integer, "T_must_be_integer");
  static_assert(std::numeric_limits<T>::is_signed, "T_must_be_signed");
  static_assert(sizeof(T) * CHAR_BIT - N >= 5,
                "N_must_leave_four_bits_plus_sign");

  bool negate = false;
  if (*s == '-') {
    ++s;
    negate = true;
  }
  if (*s == '\0') {
    return false;
  }

  const T nMax = (std::numeric_limits<T>::max() >> N) / 10;
  const T dMax = (std::numeric_limits<T>::max() >> N) - (nMax * 10);
  T n = 0;
  T frac = 0;
  for (; *s; ++s) {
    // Check if digit
    if (*s < '0' || '9' < *s) {
      // If it wasn't a digit, check if it is a '.' followed by something.
      if (*s != '.' || s[1] == '\0') {
        return false;
      }
      // Find the end, verify digits.
      for (++s; *s; ++s) {
        if (*s < '0' || '9' < *s) {
          return false;
        }
      }
      // Read back toward the '.'.
      for (--s; *s != '.'; --s) {
        T d = *s - '0';
        frac = (frac + (d << N)) / 10;  // This requires four bits overhead.
      }
      break;
    }
    T d = *s - '0';
    // Check for overflow
    if (n > nMax || (n == nMax && d > dMax)) {
      return false;
    }
    n = (n * 10) + d;
  }
  if (negate) {
    n = -n;
    frac = -frac;
  }
  *value = (int32_t)((uint32_t)n << N) + frac;
  return true;
}

static inline void ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

static inline void rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

static inline void trim(std::string& s) {
  ltrim(s);
  rtrim(s);
}

static inline void lowercase(std::string& s) {
  // Convert ASCII characters to lower case.
  std::transform(s.begin(), s.end(), s.begin(),
                 [](char c) { return (c & 0x80) ? c : ::tolower(c); });
}

namespace skity {

enum class NodeType {
  kUnknown,
  kFamilySet,
  kFamily,
  kFont,
  kAlias,
  kAxis,
};

class Node {
 public:
  explicit Node(std::string tag) : tag_(std::move(tag)) {}
  virtual ~Node() = default;

  virtual NodeType Type() { return NodeType::kUnknown; }
  virtual bool StartElement() { return true; }
  virtual bool Attribute(const char*, const char*) { return true; }
  virtual bool Text(const char* text, int32_t) {
    if (text_.size() > 0) {
      // ERROR
      // assert(text_.size() == 0);
    } else {
      text_ = text;
      trim(text_);
    }
    return true;
  }
  virtual bool EndElement() { return true; }

  void SetParent(Node* parent) {
    if (parent_) {
      // ERROR
      assert(!parent_);
    } else {
      parent_ = parent;
    }
  }

  void AddChildNode(std::unique_ptr<Node> child) {
    children_.push_back(std::move(child));
  }

  std::string Tag() { return tag_; }

 private:
  std::string tag_;
  Node* parent_ = nullptr;
  std::vector<std::unique_ptr<Node>> children_;

  std::string text_;

  friend class FontsXmlParser;
};  // namespace skity

using RootNode = Node;

class FamilySetNode : public Node {
 public:
  explicit FamilySetNode(std::string tag) : Node(std::move(tag)) {}
  ~FamilySetNode() override = default;

  NodeType Type() override { return NodeType::kFamilySet; }

  bool Attribute(const char* name, const char* value) override {
    if (!strcmp(name, "version")) {
      parse_non_negative_integer(value, &version_);
    }
    return true;
  }

 private:
  int version_ = -1;

  friend class FontsXmlParser;
};

class FamilyNode : public Node {
 public:
  explicit FamilyNode(std::string tag) : Node(std::move(tag)) {}
  ~FamilyNode() override = default;

  NodeType Type() override { return NodeType::kFamily; }

  bool Attribute(const char* name, const char* value) override {
    if (!strcmp(name, "name")) {
      name_ = value;
      lowercase(name_);
      is_fallback_ = false;
    } else if (!strcmp(name, "lang")) {
      lang_ = value;
    } else if (!strcmp(name, "variant")) {
      variant_ = value;
    }
    return true;
  }

 private:
  std::string name_;
  bool is_fallback_ = true;

  std::string lang_;
  std::string variant_;

  friend class FontsXmlParser;
};

class FontNode : public Node {
 public:
  explicit FontNode(std::string tag) : Node(std::move(tag)) {}
  ~FontNode() override = default;

  NodeType Type() override { return NodeType::kFont; }

  bool Attribute(const char* name, const char* value) override {
    if (!strcmp(name, "index")) {
      if (!parse_non_negative_integer(value, &index_)) {
        LOGE("'%s' is an invalid index", value);
      }
    } else if (!strcmp(name, "weight")) {
      if (!parse_non_negative_integer(value, &weight_)) {
        LOGE("'%s' is an invalid wight", value);
      }
    } else if (!strcmp(name, "style")) {
      if (!strcmp("normal", value)) {
        style_ = FontFileInfo::Style::kNormal;
      } else if (!strcmp("italic", value)) {
        style_ = FontFileInfo::Style::kItalic;
      }
    } else if (!strcmp(name, "fallbackFor")) {
      fallback_for_ = value;
    }
    return true;
  }

 private:
  int index_ = 0;
  int weight_ = 0;
  FontFileInfo::Style style_ = FontFileInfo::Style::kAuto;
  std::string fallback_for_;

  friend class FontsXmlParser;
};

class AliasNode : public Node {
 public:
  explicit AliasNode(std::string tag) : Node(std::move(tag)) {}
  ~AliasNode() override = default;

  NodeType Type() override { return NodeType::kAlias; }

  bool Attribute(const char* name, const char* value) override {
    if (!strcmp(name, "name")) {
      name_ = value;
      lowercase(name_);
    } else if (!strcmp(name, "to")) {
      to_ = value;
    } else if (!strcmp(name, "weight")) {
      if (!parse_non_negative_integer(value, &weight_)) {
        weight_ = 0;
        LOGE("'%s' is an invalid weight", value);
      }
    }
    return true;
  }

 private:
  std::string name_;
  std::string to_;
  int weight_ = 0;

  friend class FontsXmlParser;
  friend void HandleAliasNode(AliasNode* alias_node,
                              std::vector<FontFamily>* font_families);
};

class AxisNode : public Node {
 public:
  explicit AxisNode(std::string tag) : Node(std::move(tag)) {}
  ~AxisNode() override = default;

  NodeType Type() override { return NodeType::kAxis; }

  bool Attribute(const char* name, const char* value) override {
    if (!strcmp(name, "tag")) {
      tag_ = value;
    } else if (!strcmp(name, "stylevalue")) {
      int32_t fixed = 0;
      if (parse_fixed<16>(value, &fixed)) {
        style_value_ = FixedDot16ToFloat(fixed);
      } else {
        LOGE("'%s' is an invalid stylevalue", value);
      }
    }
    return true;
  }

 private:
  std::string tag_;
  float style_value_ = 0.f;

  friend class FontsXmlParser;
};

class FontsXmlParser : public XMLParser {
 public:
  FontsXmlParser(std::string font_path, XMLParserError* error);
  ~FontsXmlParser() override;

  std::vector<FontFamily> BuildFontFamilies();

 protected:
  bool OnStartElement(const char* elem) override;
  bool OnAddAttribute(const char* name, const char* value) override;
  bool OnEndElement(const char* elem) override;
  bool OnText(const char* text, int32_t len) override;

 private:
  void PushNode(Node* node);
  void PopNode();
  Node* ProcessingNode();

  void HandleFamilySetNode(FamilySetNode* family_set_node,
                           std::vector<FontFamily>* font_families);
  void HandleFamilyNode(FamilyNode* family_node,
                        std::vector<FontFamily>* font_families);
  void HandleAliasNode(AliasNode* alias_node,
                       std::vector<FontFamily>* font_families);
  void HandleFontNode(FontNode* font_node,
                      std::vector<FontFamily>* font_families);
  void HandleAxisNode(AxisNode* axis_node, FontFileInfo* file_info,
                      std::vector<FontFamily>* font_families);

  std::vector<Node*> nodes_stack_;
  std::unique_ptr<RootNode> root_;

  std::string font_path_;
};

FontsXmlParser::FontsXmlParser(std::string font_path, XMLParserError* error)
    : XMLParser(error), font_path_(std::move(font_path)) {
  root_ = std::make_unique<RootNode>("root");
  PushNode(root_.get());
}

FontsXmlParser::~FontsXmlParser() = default;

bool FontsXmlParser::OnStartElement(const char* elem) {
  std::unique_ptr<Node> current_node;
  if (!strcmp(elem, "familyset")) {
    current_node.reset(new FamilySetNode("familyset"));
  } else if (!strcmp(elem, "family")) {
    current_node.reset(new FamilyNode("family"));
  } else if (!strcmp(elem, "font")) {
    current_node.reset(new FontNode("font"));
  } else if (!strcmp(elem, "alias")) {
    current_node.reset(new AliasNode("alias"));
  } else if (!strcmp(elem, "axis")) {
    current_node.reset(new AxisNode("axis"));
  }

  if (current_node) {
    Node* pending_push = current_node.get();
    ProcessingNode()->AddChildNode(std::move(current_node));
    PushNode(pending_push);
  }

  return true;
}

bool FontsXmlParser::OnAddAttribute(const char* name, const char* value) {
  return ProcessingNode()->Attribute(name, value);
}

bool FontsXmlParser::OnEndElement(const char* elem) {
  // Why has empty end tag?
  if (strlen(elem) == 0) return true;
  if (strcmp(ProcessingNode()->Tag().c_str(), elem)) {
    assert(!strcmp(ProcessingNode()->Tag().c_str(), elem));
  }

  bool result = ProcessingNode()->EndElement();
  PopNode();
  return result;
}

bool FontsXmlParser::OnText(const char* text, int32_t len) {
  return ProcessingNode()->Text(text, len);
  return true;
}

void FontsXmlParser::PushNode(Node* node) { nodes_stack_.push_back(node); }

void FontsXmlParser::PopNode() { nodes_stack_.pop_back(); }

Node* FontsXmlParser::ProcessingNode() { return nodes_stack_.back(); }

std::vector<FontFamily> FontsXmlParser::BuildFontFamilies() {
  if (root_ && root_->children_.empty() &&
      root_->children_[0]->Type() != NodeType::kFamilySet) {
    return {};
  }
  FamilySetNode* family_set_node =
      static_cast<FamilySetNode*>(root_->children_[0].get());
  std::vector<FontFamily> font_families;

  HandleFamilySetNode(family_set_node, &font_families);

  return font_families;
}

void FontsXmlParser::HandleFamilySetNode(
    FamilySetNode* family_set_node, std::vector<FontFamily>* font_families) {
  if (family_set_node->version_ >= 21) {
    // new version
    for (auto& child_node : family_set_node->children_) {
      if (child_node->Type() == NodeType::kFamily) {
        FamilyNode* family_node = static_cast<FamilyNode*>(child_node.get());
        HandleFamilyNode(family_node, font_families);
      } else if (child_node->Type() == NodeType::kAlias) {
        AliasNode* alias_node = static_cast<AliasNode*>(child_node.get());
        HandleAliasNode(alias_node, font_families);
      }
    }
  } else {
    // old version
  }
}

void FontsXmlParser::HandleFamilyNode(FamilyNode* family_node,
                                      std::vector<FontFamily>* font_families) {
  font_families->emplace_back(font_path_, true);
  FontFamily& current = font_families->back();
  if (family_node->name_.empty()) {
    current.is_fallback_font = true;
  } else {
    current.names.push_back(family_node->name_);
    current.is_fallback_font = false;
  }
  if (!family_node->lang_.empty()) {
    current.languages.push_back(family_node->lang_);
  }
  if (family_node->variant_ == "elegant") {
    current.variant = FontVariants::kElegant_FontVariant;
  } else if (family_node->variant_ == "compact") {
    current.variant = FontVariants::kCompact_FontVariant;
  }

  for (auto& child_node : family_node->children_) {
    if (child_node->Type() == NodeType::kFont) {
      FontNode* font_node = static_cast<FontNode*>(child_node.get());
      HandleFontNode(font_node, font_families);
    }
  }
}

static FontFamily* find_family(const std::vector<FontFamily>& font_families,
                               const std::string& familyName) {
  for (size_t i = 0; i < font_families.size(); i++) {
    const FontFamily* candidate = &font_families[i];
    for (size_t j = 0; j < candidate->names.size(); j++) {
      if (candidate->names[j] == familyName) {
        return const_cast<FontFamily*>(candidate);
      }
    }
  }
  return nullptr;
}

void FontsXmlParser::HandleAliasNode(AliasNode* alias_node,
                                     std::vector<FontFamily>* font_families) {
  if (!find_family(*font_families, alias_node->to_)) {
    LOGE("'%s' alias target not found", alias_node->to_);
    return;
  }
  if (alias_node->weight_) {
    font_families->emplace_back(font_path_, false);
    FontFamily& family = font_families->back();
    family.names.push_back(alias_node->name_);

    FontFamily* target_family = find_family(*font_families, alias_node->to_);
    for (auto& font : target_family->fonts) {
      if (font.weight == alias_node->weight_) {
        family.fonts.push_back(font);
      }
    }
  } else {
    // has no weight
    FontFamily* target_family = find_family(*font_families, alias_node->to_);
    target_family->names.push_back(alias_node->name_);
  }
}

void FontsXmlParser::HandleFontNode(FontNode* font_node,
                                    std::vector<FontFamily>* font_families) {
  FontFileInfo file_info{font_node->text_,
                         font_node->index_,
                         font_node->weight_,
                         font_node->style_,
                         {}};
  FontFamily& current_font_family = font_families->back();
  FontFileInfo* current_font_file = nullptr;
  if (font_node->fallback_for_.empty()) {
    current_font_family.fonts.push_back(file_info);
    current_font_file = &(current_font_family.fonts.back());
  } else {
    auto fallback_family_iter =
        current_font_family.fallback_families.find(font_node->fallback_for_);
    if (fallback_family_iter == current_font_family.fallback_families.end()) {
      std::unique_ptr<FontFamily> fallback_family =
          std::make_unique<FontFamily>(font_path_, true);
      fallback_family->languages = current_font_family.languages;
      fallback_family->variant = current_font_family.variant;
      fallback_family->order = current_font_family.order;
      fallback_family->fallback_for = font_node->fallback_for_;
      fallback_family->fonts.push_back(file_info);
      current_font_file = &(fallback_family->fonts.back());
      current_font_family.fallback_families.emplace(font_node->fallback_for_,
                                                    std::move(fallback_family));
    } else {
      fallback_family_iter->second->fonts.push_back(file_info);
      current_font_file = &(fallback_family_iter->second->fonts.back());
    }
  }

  for (auto& child_node : font_node->children_) {
    if (child_node->Type() == NodeType::kAxis) {
      AxisNode* axis_node = static_cast<AxisNode*>(child_node.get());
      HandleAxisNode(axis_node, current_font_file, font_families);
    }
  }
}

void FontsXmlParser::HandleAxisNode(AxisNode* axis_node,
                                    FontFileInfo* file_info,
                                    std::vector<FontFamily>*) {
  auto axis_tag_iter = file_info->axis_tags.find(axis_node->tag_);
  if (axis_tag_iter == file_info->axis_tags.end()) {
    file_info->axis_tags.emplace(axis_node->tag_, axis_node->style_value_);
  } else {
    LOGE("'%s' axis specified more than once", axis_node->tag_);
  }
}

//  FontsContent  //
std::unique_ptr<FontResources> FontResources::MakeFromFile() {
  std::string root(getenv("ANDROID_ROOT"));
  std::string font_path(root + FONT_FILE_PATH);
  auto data = Data::MakeFromFileName(LMP_SYSTEM_FONTS_FILE);
  if (!data) {
    return nullptr;
  }

  const char* raw_data = static_cast<const char*>(data->RawData());
  size_t len = data->Size();
  if (raw_data == nullptr || len == 0) {
    return nullptr;
  }

  XMLParserError error;

  FontsXmlParser xml_parser(font_path, &error);

  if (!xml_parser.Parse(raw_data, len)) {
    return nullptr;
  }

  std::unique_ptr<FontResources> font_resources{
      new FontResources(xml_parser.BuildFontFamilies())};

  return font_resources;
}

FontResources::FontResources(std::vector<FontFamily> font_families)
    : font_families_(std::move(font_families)) {}

}  // namespace skity
