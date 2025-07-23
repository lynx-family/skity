// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <pugixml.hpp>
#include <skity/utils/xml/xml_parser.hpp>

namespace skity {

static bool recursive_visit_node(pugi::xml_node const& node,
                                 XMLParser* parser) {
  // begin
  if (!parser->StartElement(node.name())) {
    return false;
  }

  // attribute
  for (auto attr = node.first_attribute(); attr; attr = attr.next_attribute()) {
    if (!parser->AddAttribute(attr.name(), attr.value())) {
      return false;
    }
  }

  // text
  const char* text = node.text().as_string();
  if (!parser->Text(text, strlen(text))) {
    return false;
  }

  // children
  for (auto child = node.first_child(); child; child = child.next_sibling()) {
    if (!recursive_visit_node(child, parser)) {
      return false;
    }
  }

  // end
  if (!parser->EndElement(node.name())) {
    return false;
  }

  return true;
}

static const char* const g_error_strings[] = {
    "empty or missing file ",    "unknown element ", "unknown attribute name ",
    "error in attribute value ", "duplicate ID ",    "unknown error "};

XMLParserError::XMLParserError()
    : code_(kNoError), line_number_(-1), native_code_(-1) {
  Reset();
}

std::string XMLParserError::GetErrorString() const {
  if (code_ != kNoError) {
    std::string ret;
    if (static_cast<uint32_t>(code_) < 6) {
      ret += g_error_strings[code_ - 1];
    }
    ret += noun_;
    return ret;
  } else {
    return "";
  }
}

void XMLParserError::Reset() {
  code_ = kNoError;
  line_number_ = -1;
  native_code_ = -1;
  pugi::xml_document doc;
  doc.load_string("tree.xml");
}

XMLParser::XMLParser(XMLParserError* error) : error_(error) {}

bool XMLParser::Parse(const std::string& doc) {
  return Parse(doc.c_str(), doc.size());
}

bool XMLParser::Parse(const char* doc, size_t len) {
  pugi::xml_document document;
  pugi::xml_parse_result result = document.load_buffer(doc, len);
  if (!result) {
    error_->code_ = XMLParserError::kUnknownError;
    error_->SetNoun(result.description());
    return false;
  }

  pugi::xml_node root = document.first_child();

  bool ret = recursive_visit_node(root, this);

  return error_->code_ == XMLParserError::kNoError && ret;
}

bool XMLParser::StartElement(const char* elem) {
  return this->OnStartElement(elem);
}

bool XMLParser::AddAttribute(const char* name, const char* value) {
  return this->OnAddAttribute(name, value);
}

bool XMLParser::EndElement(const char* elem) {
  return this->OnEndElement(elem);
}

bool XMLParser::Text(const char* text, int32_t len) {
  return this->OnText(text, len);
}

bool XMLParser::OnStartElement(const char*) { return false; }
bool XMLParser::OnAddAttribute(const char*, const char*) { return false; }
bool XMLParser::OnEndElement(const char*) { return false; }
bool XMLParser::OnText(const char*, int32_t) { return false; }

}  // namespace skity
