// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

struct GlobalFns {
  PFN_vkCreateInstance vkCreateInstance = nullptr;
  PFN_vkEnumerateInstanceExtensionProperties
      vkEnumerateInstanceExtensionProperties = nullptr;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties =
      nullptr;
};

struct InstanceFns {
  PFN_vkDestroyInstance vkDestroyInstance = nullptr;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
  PFN_vkCreateDevice vkCreateDevice = nullptr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
};

struct DeviceFns {
  PFN_vkDestroyDevice vkDestroyDevice = nullptr;
  PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
  PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
  PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
  PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
  PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
  PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
  PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
};

bool LoadGlobalFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                   GlobalFns* fns) {
  if (get_instance_proc_addr == nullptr || fns == nullptr) {
    return false;
  }

  fns->vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      get_instance_proc_addr(nullptr, "vkCreateInstance"));
  fns->vkEnumerateInstanceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          get_instance_proc_addr(nullptr,
                                 "vkEnumerateInstanceExtensionProperties"));
  fns->vkEnumerateInstanceLayerProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          get_instance_proc_addr(nullptr, "vkEnumerateInstanceLayerProperties"));

  return fns->vkCreateInstance != nullptr;
}

bool LoadInstanceFns(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                     VkInstance instance, InstanceFns* fns) {
  if (get_instance_proc_addr == nullptr || instance == VK_NULL_HANDLE ||
      fns == nullptr) {
    return false;
  }

  fns->vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      get_instance_proc_addr(instance, "vkDestroyInstance"));
  fns->vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices"));
  fns->vkGetPhysicalDeviceQueueFamilyProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          get_instance_proc_addr(instance,
                                 "vkGetPhysicalDeviceQueueFamilyProperties"));
  fns->vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
      get_instance_proc_addr(instance, "vkCreateDevice"));
  fns->vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      get_instance_proc_addr(instance, "vkGetDeviceProcAddr"));

  return fns->vkDestroyInstance != nullptr &&
         fns->vkEnumeratePhysicalDevices != nullptr &&
         fns->vkGetPhysicalDeviceQueueFamilyProperties != nullptr &&
         fns->vkCreateDevice != nullptr && fns->vkGetDeviceProcAddr != nullptr;
}

bool LoadDeviceFns(PFN_vkGetDeviceProcAddr get_device_proc_addr,
                   VkDevice device, DeviceFns* fns) {
  if (get_device_proc_addr == nullptr || device == VK_NULL_HANDLE ||
      fns == nullptr) {
    return false;
  }

  fns->vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
      get_device_proc_addr(device, "vkDestroyDevice"));
  fns->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
      get_device_proc_addr(device, "vkGetDeviceQueue"));
  fns->vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(
      get_device_proc_addr(device, "vkCreateShaderModule"));
  fns->vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(
      get_device_proc_addr(device, "vkDestroyShaderModule"));
  fns->vkCreatePipelineLayout =
      reinterpret_cast<PFN_vkCreatePipelineLayout>(
          get_device_proc_addr(device, "vkCreatePipelineLayout"));
  fns->vkDestroyPipelineLayout =
      reinterpret_cast<PFN_vkDestroyPipelineLayout>(
          get_device_proc_addr(device, "vkDestroyPipelineLayout"));
  fns->vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
      get_device_proc_addr(device, "vkCreateRenderPass"));
  fns->vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(
      get_device_proc_addr(device, "vkDestroyRenderPass"));
  fns->vkCreateGraphicsPipelines =
      reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
          get_device_proc_addr(device, "vkCreateGraphicsPipelines"));
  fns->vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(
      get_device_proc_addr(device, "vkDestroyPipeline"));

  return fns->vkDestroyDevice != nullptr && fns->vkGetDeviceQueue != nullptr &&
         fns->vkCreateShaderModule != nullptr &&
         fns->vkDestroyShaderModule != nullptr &&
         fns->vkCreatePipelineLayout != nullptr &&
         fns->vkDestroyPipelineLayout != nullptr &&
         fns->vkCreateRenderPass != nullptr &&
         fns->vkDestroyRenderPass != nullptr &&
         fns->vkCreateGraphicsPipelines != nullptr &&
         fns->vkDestroyPipeline != nullptr;
}

std::vector<uint32_t> CompileToSpirv(const char* source, const char* entry) {
  auto program = wgx::Program::Parse(source);
  if (program == nullptr || program->GetDiagnosis().has_value()) {
    return {};
  }

  wgx::SpirvOptions options;
  auto result = program->WriteToSpirv(entry, options);
  if (!result.success) {
    return {};
  }
  return result.spirv;
}

class VulkanTestContext {
 public:
  ~VulkanTestContext() { Reset(); }

  bool Initialize() {
    if (!LoadGlobalFns(vkGetInstanceProcAddr, &global_fns_)) {
      return false;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "wgx_vulkan_pipeline_test";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "wgx";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    if (global_fns_.vkCreateInstance(&instance_info, nullptr, &instance_) !=
        VK_SUCCESS) {
      return false;
    }

    if (!LoadInstanceFns(vkGetInstanceProcAddr, instance_, &instance_fns_)) {
      return false;
    }

    uint32_t physical_device_count = 0;
    if (instance_fns_.vkEnumeratePhysicalDevices(instance_, &physical_device_count,
                                                 nullptr) != VK_SUCCESS ||
        physical_device_count == 0) {
      return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count,
                                                   VK_NULL_HANDLE);
    if (instance_fns_.vkEnumeratePhysicalDevices(instance_,
                                                 &physical_device_count,
                                                 physical_devices.data()) !=
        VK_SUCCESS) {
      return false;
    }
    physical_device_ = physical_devices[0];
    if (physical_device_ == VK_NULL_HANDLE) {
      return false;
    }

    uint32_t queue_family_count = 0;
    instance_fns_.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
      return false;
    }

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    instance_fns_.vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device_, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
      if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        graphics_queue_family_index_ = i;
        break;
      }
    }
    if (graphics_queue_family_index_ == UINT32_MAX) {
      return false;
    }

    const float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = graphics_queue_family_index_;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;

    if (instance_fns_.vkCreateDevice(physical_device_, &device_info, nullptr,
                                     &device_) != VK_SUCCESS) {
      return false;
    }

    if (!LoadDeviceFns(instance_fns_.vkGetDeviceProcAddr, device_,
                       &device_fns_)) {
      return false;
    }

    device_fns_.vkGetDeviceQueue(device_, graphics_queue_family_index_, 0,
                                 &graphics_queue_);
    return graphics_queue_ != VK_NULL_HANDLE;
  }

  VkShaderModule CreateShaderModule(const std::vector<uint32_t>& spirv) const {
    if (device_ == VK_NULL_HANDLE || spirv.empty()) {
      return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo module_info = {};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = spirv.size() * sizeof(uint32_t);
    module_info.pCode = spirv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (device_fns_.vkCreateShaderModule(device_, &module_info, nullptr,
                                         &module) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return module;
  }

  VkPipelineLayout CreatePipelineLayout() const {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if (device_fns_.vkCreatePipelineLayout(device_, &pipeline_layout_info,
                                           nullptr,
                                           &pipeline_layout) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return pipeline_layout;
  }

  VkRenderPass CreateSimpleColorRenderPass() const {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (device_fns_.vkCreateRenderPass(device_, &render_pass_info, nullptr,
                                       &render_pass) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return render_pass;
  }

  VkPipeline CreateSimpleGraphicsPipeline(VkShaderModule vertex_module,
                                          const char* vertex_entry,
                                          VkShaderModule fragment_module,
                                          const char* fragment_entry,
                                          VkPipelineLayout pipeline_layout,
                                          VkRenderPass render_pass) const {
    if (vertex_module == VK_NULL_HANDLE || fragment_module == VK_NULL_HANDLE ||
        pipeline_layout == VK_NULL_HANDLE || render_pass == VK_NULL_HANDLE) {
      return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    shader_stages[0].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vertex_module;
    shader_stages[0].pName = vertex_entry;
    shader_stages[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = fragment_module;
    shader_stages[1].pName = fragment_entry;

    VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
    vertex_input_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
    input_assembly_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = 1.0f;
    viewport.height = 1.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent.width = 1;
    scissor.extent.height = 1;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_state = {};
    rasterization_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = VK_CULL_MODE_NONE;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_state = {};
    multisample_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state = {};
    color_blend_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &color_blend_attachment;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pColorBlendState = &color_blend_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (device_fns_.vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
                                              &pipeline_info, nullptr,
                                              &pipeline) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return pipeline;
  }

  void DestroyShaderModule(VkShaderModule module) const {
    if (module != VK_NULL_HANDLE) {
      device_fns_.vkDestroyShaderModule(device_, module, nullptr);
    }
  }

  void DestroyPipelineLayout(VkPipelineLayout pipeline_layout) const {
    if (pipeline_layout != VK_NULL_HANDLE) {
      device_fns_.vkDestroyPipelineLayout(device_, pipeline_layout, nullptr);
    }
  }

  void DestroyRenderPass(VkRenderPass render_pass) const {
    if (render_pass != VK_NULL_HANDLE) {
      device_fns_.vkDestroyRenderPass(device_, render_pass, nullptr);
    }
  }

  void DestroyPipeline(VkPipeline pipeline) const {
    if (pipeline != VK_NULL_HANDLE) {
      device_fns_.vkDestroyPipeline(device_, pipeline, nullptr);
    }
  }

 private:
  void Reset() {
    if (device_ != VK_NULL_HANDLE) {
      device_fns_.vkDestroyDevice(device_, nullptr);
      device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
      instance_fns_.vkDestroyInstance(instance_, nullptr);
      instance_ = VK_NULL_HANDLE;
    }
  }

  GlobalFns global_fns_ = {};
  InstanceFns instance_fns_ = {};
  DeviceFns device_fns_ = {};
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  uint32_t graphics_queue_family_index_ = UINT32_MAX;
};

class WgxVulkanPipelineTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(context_.Initialize()); }

  VulkanTestContext context_;
};

}  // namespace

TEST_F(WgxVulkanPipelineTest, CreatesGraphicsPipelineForStructInterfaceShaders) {
  const char* vertex_source = R"(
struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) color: vec4<f32>,
};

@vertex
fn vs_main() -> VertexOutput {
  var output: VertexOutput;
  output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  output.color = vec4<f32>(1.0, 0.0, 0.0, 1.0);
  return output;
}
)";

  const char* fragment_source = R"(
struct FragmentInput {
  @location(0) color: vec4<f32>,
};

struct FragmentOutput {
  @location(0) color: vec4<f32>,
};

@fragment
fn fs_main(input: FragmentInput) -> FragmentOutput {
  var output: FragmentOutput;
  var i: i32 = 0;
  output.color = input.color;
  while (i < 1) {
    i = i + 1;
    output.color = input.color;
  }
  return output;
}
)";

  std::vector<uint32_t> vertex_spirv = CompileToSpirv(vertex_source, "vs_main");
  std::vector<uint32_t> fragment_spirv =
      CompileToSpirv(fragment_source, "fs_main");
  ASSERT_FALSE(vertex_spirv.empty());
  ASSERT_FALSE(fragment_spirv.empty());

  VkShaderModule vertex_module = context_.CreateShaderModule(vertex_spirv);
  VkShaderModule fragment_module = context_.CreateShaderModule(fragment_spirv);
  ASSERT_NE(vertex_module, VK_NULL_HANDLE);
  ASSERT_NE(fragment_module, VK_NULL_HANDLE);

  VkPipelineLayout pipeline_layout = context_.CreatePipelineLayout();
  ASSERT_NE(pipeline_layout, VK_NULL_HANDLE);

  VkRenderPass render_pass = context_.CreateSimpleColorRenderPass();
  ASSERT_NE(render_pass, VK_NULL_HANDLE);

  VkPipeline pipeline = context_.CreateSimpleGraphicsPipeline(
      vertex_module, "vs_main", fragment_module, "fs_main", pipeline_layout,
      render_pass);
  ASSERT_NE(pipeline, VK_NULL_HANDLE);

  context_.DestroyPipeline(pipeline);
  context_.DestroyRenderPass(render_pass);
  context_.DestroyPipelineLayout(pipeline_layout);
  context_.DestroyShaderModule(fragment_module);
  context_.DestroyShaderModule(vertex_module);
}
