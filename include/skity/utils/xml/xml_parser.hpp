// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_UTILS_XML_XML_PARSER_HPP
#define INCLUDE_SKITY_UTILS_XML_XML_PARSER_HPP

#include <skity/macros.hpp>
#include <string>

namespace skity {

class SKITY_API XMLParserError {
 public:
  enum ErrorCode {
    kNoError,
    kEmptyFile,
    kUnknownElement,
    kUnknownAttributeName,
    kErrorInAttributeValue,
    kDuplicateIDs,
    kUnknownError,
  };

  XMLParserError();
  virtual ~XMLParserError() = default;

  ErrorCode GetErrorCode() const { return code_; }
  virtual std::string GetErrorString() const;
  int32_t GetLineNumber() const { return line_number_; }
  bool HasError() const { return code_ != kNoError; }
  bool HasNoun() const { return !noun_.empty(); }
  void Reset();
  void SetCode(ErrorCode code) { code_ = code; }
  void SetNoun(const char* str) { noun_ = str; }

 protected:
  ErrorCode code_;

 private:
  int32_t line_number_;
  int32_t native_code_;
  std::string noun_;
  friend class XMLParser;
};

class SKITY_API XMLParser {
 public:
  explicit XMLParser(XMLParserError* error = nullptr);
  virtual ~XMLParser() = default;

  bool Parse(const char* doc, size_t len);
  bool Parse(std::string const& doc);

  bool StartElement(const char* elem);
  bool AddAttribute(const char* name, const char* value);
  bool EndElement(const char* elem);
  bool Text(const char* text, int32_t len);

 protected:
  virtual bool OnStartElement(const char* elem);
  virtual bool OnAddAttribute(const char* name, const char* value);
  virtual bool OnEndElement(const char* elem);
  virtual bool OnText(const char* text, int32_t len);

 private:
  XMLParserError* error_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_UTILS_XML_XML_PARSER_HPP
