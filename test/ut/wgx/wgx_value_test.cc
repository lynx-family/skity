/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "ir/value.h"

using namespace wgx::ir;

TEST(ValueTest, NoneValueIsInvalid) {
  Value v = Value::None();
  EXPECT_FALSE(v.IsValid());
  EXPECT_FALSE(v.IsConstant());
  EXPECT_FALSE(v.IsSSA());
  EXPECT_FALSE(v.IsVariable());
}

TEST(ValueTest, ConstantF32) {
  TypeId f32_type = 1;  /** Mock type id */
  Value v = Value::ConstantF32(f32_type, 3.14f);
  
  EXPECT_TRUE(v.IsValid());
  EXPECT_TRUE(v.IsConstant());
  EXPECT_TRUE(v.IsInlineConstant());
  EXPECT_EQ(v.type, f32_type);
  EXPECT_FLOAT_EQ(v.GetF32(), 3.14f);
  EXPECT_FLOAT_EQ(v.GetF32Unchecked(), 3.14f);
}

TEST(ValueTest, ConstantVec4F32) {
  TypeId vec4_type = 2;
  Value v = Value::ConstantVec4F32(vec4_type, 1.0f, 2.0f, 3.0f, 4.0f);
  
  EXPECT_TRUE(v.IsValid());
  EXPECT_TRUE(v.IsConstant());
  EXPECT_TRUE(v.IsInlineConstant());
  EXPECT_EQ(v.type, vec4_type);
  
  auto vec = v.GetVec4F32();
  EXPECT_FLOAT_EQ(vec[0], 1.0f);
  EXPECT_FLOAT_EQ(vec[1], 2.0f);
  EXPECT_FLOAT_EQ(vec[2], 3.0f);
  EXPECT_FLOAT_EQ(vec[3], 4.0f);
}

TEST(ValueTest, SSAValue) {
  TypeId vec4_type = 2;
  Value v = Value::SSA(vec4_type, 42);
  
  EXPECT_TRUE(v.IsValid());
  EXPECT_TRUE(v.IsSSA());
  EXPECT_FALSE(v.IsConstant());
  EXPECT_FALSE(v.IsVariable());
  EXPECT_EQ(v.type, vec4_type);
  
  auto ssa_id = v.GetSSAId();
  EXPECT_TRUE(ssa_id.has_value());
  EXPECT_EQ(ssa_id.value(), 42);
  EXPECT_EQ(v.GetIdUnchecked(), 42);
}

TEST(ValueTest, VariableValue) {
  TypeId vec4_type = 2;
  Value v = Value::Variable(vec4_type, 10);
  
  EXPECT_TRUE(v.IsValid());
  EXPECT_TRUE(v.IsVariable());
  EXPECT_TRUE(v.IsAddress());
  EXPECT_FALSE(v.IsValue());
  EXPECT_EQ(v.type, vec4_type);
  
  auto var_id = v.GetVarId();
  EXPECT_TRUE(var_id.has_value());
  EXPECT_EQ(var_id.value(), 10);
}

TEST(ValueTest, SafeAccessorsReturnNulloptForWrongType) {
  TypeId f32_type = 1;
  Value constant = Value::ConstantF32(f32_type, 1.0f);
  
  /** Constant should not have SSA id or Var id */
  EXPECT_FALSE(constant.GetSSAId().has_value());
  EXPECT_FALSE(constant.GetVarId().has_value());
  EXPECT_FALSE(constant.GetPoolIndex().has_value());
}

TEST(ValueTest, ExprResultValue) {
  TypeId f32_type = 1;
  Value v = Value::ConstantF32(f32_type, 2.0f);
  ExprResult result = ExprResult::ValueResult(v);
  
  EXPECT_TRUE(result.IsValid());
  EXPECT_TRUE(result.IsValue());
  EXPECT_FALSE(result.IsAddress());
  EXPECT_EQ(result.GetType(), f32_type);
}

TEST(ValueTest, ExprResultAddress) {
  TypeId vec4_type = 2;
  Value var = Value::Variable(vec4_type, 5);
  ExprResult result = ExprResult::AddressResult(var);
  
  EXPECT_TRUE(result.IsValid());
  EXPECT_TRUE(result.IsAddress());
  EXPECT_FALSE(result.IsValue());
  EXPECT_EQ(result.GetType(), vec4_type);
}
