## WGSLCross

A self implement [wgsl](https://www.w3.org/TR/WGSL/) parser and compiler.

### Different from other implementation

1. Do not throw parser error message in details
2. Do not support [global_directive](https://www.w3.org/TR/WGSL/#recursive-descent-syntax-global_directive)
3. Do not support `override`
   in [global_value_decl](https://www.w3.org/TR/WGSL/#recursive-descent-syntax-global_value_decl)
4. Do not support `const_assert` since this compiler do not do any static analyze
5. [attribute](https://www.w3.org/TR/WGSL/#recursive-descent-syntax-attribute) only support `const literal expression`.
6. Do not support automatic type inference, which means, every variable must be annotated with type.
7. Do not support `Compute Shader` for now.

### Example

```
const char* source = R"(
// comments
struct VertexInput {
    @location(0) position: vec4<f32>,
    @location(1) color: vec3<f32>,
};

/**
 * multi line comments
 * line 2
 * line three
 */
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec3<f32>,
};

struct UserMatrix {
    transform: mat4x4<f32>,
};

@group(0) @binding(0)
var<uniform> transform: UserMatrix;

@vertex
fn vs_main(vertex: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = transform.transform * vertex.position;
    out.color = vertex.color;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color, 1.0);
}
)";

auto program = wgx::Program::Parse(std::string(source));

wgx::GlslOptions options{};
options.standard = wgx::GlslOptions::Standard::kDesktop;
options.major_version = 4;
options.minor_version = 1;

auto result = program->WriteToGlsl("vs_main", options);

if (result) {
  std::cout << "glsl:\n" << result.content << std::endl;
}

```

This should output :
```
#version 410 core

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 out_color;

struct VertexInput {
	vec4 position;
	vec3 color;
};
struct VertexOutput {
	vec4 position;
	vec3 color;
};
struct UserMatrix {
	mat4 transform;
};
layout ( std140 ) uniform transformblock_ubo {
UserMatrix inner ;
} transform;
VertexOutput vs_main(VertexInput vertex)
{
VertexOutput out_1;
out_1.position = transform.inner.transform * vertex.position;
out_1.color = vertex.color;
return out_1;
}

void main() {
VertexInput vertex;
vertex.position = in_position;
vertex.color = in_color;

VertexOutput entry_point_out = vs_main(vertex);
gl_Position = entry_point_out.position;
out_color = entry_point_out.color;
}
```