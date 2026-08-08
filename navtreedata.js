/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "MayaFlux", "index.html", [
    [ "MayaFlux: Getting Started", "index.html", "index" ],
    [ "MayaFlux", "md_README.html", [
      [ "The Architecture", "md_README.html#autotoc_md78", null ],
      [ "Processing Model", "md_README.html#autotoc_md80", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md82", [
        [ "Nodes", "md_README.html#autotoc_md83", null ],
        [ "Buffers", "md_README.html#autotoc_md84", null ],
        [ "IO and Containers", "md_README.html#autotoc_md85", null ],
        [ "Graphics", "md_README.html#autotoc_md86", null ],
        [ "Portal::Text", "md_README.html#autotoc_md87", null ],
        [ "Portal::Forma", "md_README.html#autotoc_md88", null ],
        [ "Portal::System", "md_README.html#autotoc_md89", null ],
        [ "Coroutines", "md_README.html#autotoc_md90", null ],
        [ "Viewport Navigation", "md_README.html#autotoc_md91", null ],
        [ "Networking", "md_README.html#autotoc_md92", null ],
        [ "Nexus", "md_README.html#autotoc_md93", null ],
        [ "Yantra (Offline Compute)", "md_README.html#autotoc_md94", null ],
        [ "Lila (Live Coding)", "md_README.html#autotoc_md95", null ],
        [ "Audio Backends", "md_README.html#autotoc_md96", null ],
        [ "MIDI Backends", "md_README.html#autotoc_md97", null ],
        [ "Windowing Backends", "md_README.html#autotoc_md98", null ],
        [ "Input", "md_README.html#autotoc_md99", null ],
        [ "Kinesis", "md_README.html#autotoc_md100", null ],
        [ "Build and CI", "md_README.html#autotoc_md101", null ]
      ] ],
      [ "Quick Start (Projects) — Weave", "md_README.html#autotoc_md103", [
        [ "Management Mode", "md_README.html#autotoc_md104", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md105", null ]
      ] ],
      [ "Quick Start (Developer)", "md_README.html#autotoc_md107", [
        [ "Requirements", "md_README.html#autotoc_md108", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md109", null ],
        [ "Build", "md_README.html#autotoc_md110", null ]
      ] ],
      [ "Releases and Builds", "md_README.html#autotoc_md112", [
        [ "Stable Releases", "md_README.html#autotoc_md113", null ],
        [ "Development Builds", "md_README.html#autotoc_md114", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md116", [
        [ "Basic Application Structure", "md_README.html#autotoc_md117", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md119", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md121", [
        [ "Tutorials", "md_README.html#autotoc_md122", null ],
        [ "API Documentation", "md_README.html#autotoc_md123", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md125", null ],
      [ "Philosophy", "md_README.html#autotoc_md127", null ],
      [ "For Researchers and Developers", "md_README.html#autotoc_md129", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md131", [
        [ "Phase 1 (Complete)", "md_README.html#autotoc_md132", null ],
        [ "Phase 2 (Complete)", "md_README.html#autotoc_md133", null ],
        [ "Phase 3 (Complete)", "md_README.html#autotoc_md134", null ],
        [ "Phase 4 (0.4, current)", "md_README.html#autotoc_md135", null ],
        [ "Phase 5 (0.5)", "md_README.html#autotoc_md136", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md138", null ],
      [ "Contributing", "md_README.html#autotoc_md140", null ],
      [ "Authorship and Ethics", "md_README.html#autotoc_md142", null ],
      [ "Contact", "md_README.html#autotoc_md144", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md149", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md151", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md153", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md156", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md158", null ],
      [ "🧠 AI-Assisted Contributions", "md_CONTRIBUTING.html#autotoc_md160", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md162", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md164", null ]
    ] ],
    [ "MayaFlux: The Computational Substrate", "md_docs_2Digital__Architecture.html", [
      [ "Everything Is Numbers", "md_docs_2Digital__Architecture.html#autotoc_md168", null ],
      [ "The Real-Time Core: Nodes, Buffers, Vruta, Kriya", "md_docs_2Digital__Architecture.html#autotoc_md170", [
        [ "Nodes", "md_docs_2Digital__Architecture.html#autotoc_md171", null ],
        [ "Buffers", "md_docs_2Digital__Architecture.html#autotoc_md172", null ],
        [ "Processors", "md_docs_2Digital__Architecture.html#autotoc_md173", null ],
        [ "Vruta", "md_docs_2Digital__Architecture.html#autotoc_md174", null ],
        [ "Kriya", "md_docs_2Digital__Architecture.html#autotoc_md175", null ]
      ] ],
      [ "Kakshya: The Data Layer", "md_docs_2Digital__Architecture.html#autotoc_md177", null ],
      [ "Yantra: The Offline Computation Universe", "md_docs_2Digital__Architecture.html#autotoc_md179", null ],
      [ "The Visual Pipeline", "md_docs_2Digital__Architecture.html#autotoc_md181", [
        [ "Portal", "md_docs_2Digital__Architecture.html#autotoc_md182", null ],
        [ "Registry", "md_docs_2Digital__Architecture.html#autotoc_md183", null ],
        [ "Core and Subsystems", "md_docs_2Digital__Architecture.html#autotoc_md184", null ]
      ] ],
      [ "Nexus: Spatial Entity Simulation", "md_docs_2Digital__Architecture.html#autotoc_md186", null ],
      [ "Kinesis: The Mathematical Substrate", "md_docs_2Digital__Architecture.html#autotoc_md188", null ],
      [ "IO: Reading and Writing the World", "md_docs_2Digital__Architecture.html#autotoc_md190", null ],
      [ "Journal: Structured Logging", "md_docs_2Digital__Architecture.html#autotoc_md192", null ],
      [ "Transitive: Framework-Independent Utilities", "md_docs_2Digital__Architecture.html#autotoc_md194", null ],
      [ "Lila: The Live Layer", "md_docs_2Digital__Architecture.html#autotoc_md196", null ],
      [ "Tokens and Domain", "md_docs_2Digital__Architecture.html#autotoc_md198", null ],
      [ "How the System Composes", "md_docs_2Digital__Architecture.html#autotoc_md200", null ]
    ] ],
    [ "Yantra Compute Guide", "md_docs_2GPGPU-Guide.html", [
      [ "The Compute Chain", "md_docs_2GPGPU-Guide.html#autotoc_md230", null ],
      [ "Data Layer (Kakshya)", "md_docs_2GPGPU-Guide.html#autotoc_md232", [
        [ "DataVariant", "md_docs_2GPGPU-Guide.html#autotoc_md233", null ],
        [ "DataModality", "md_docs_2GPGPU-Guide.html#autotoc_md234", null ],
        [ "DataDimension", "md_docs_2GPGPU-Guide.html#autotoc_md235", null ],
        [ "Datum<T>", "md_docs_2GPGPU-Guide.html#autotoc_md236", null ],
        [ "Container types", "md_docs_2GPGPU-Guide.html#autotoc_md237", null ],
        [ "GpuBufferBinding", "md_docs_2GPGPU-Guide.html#autotoc_md239", null ]
      ] ],
      [ "Executor Layer", "md_docs_2GPGPU-Guide.html#autotoc_md240", [
        [ "GpuComputeConfig", "md_docs_2GPGPU-Guide.html#autotoc_md241", null ],
        [ "GpuExecutionContext<InputType, OutputType>", "md_docs_2GPGPU-Guide.html#autotoc_md242", null ],
        [ "Multi-stage and chained dispatch (GpuDispatchCore)", "md_docs_2GPGPU-Guide.html#autotoc_md243", null ],
        [ "ShaderExecutionContext<InputType, OutputType>", "md_docs_2GPGPU-Guide.html#autotoc_md244", null ],
        [ "TextureExecutionContext", "md_docs_2GPGPU-Guide.html#autotoc_md245", null ]
      ] ],
      [ "Declarative Shaders (ShaderSpec)", "md_docs_2GPGPU-Guide.html#autotoc_md247", [
        [ "Bridging ShaderSpec to Yantra executors", "md_docs_2GPGPU-Guide.html#autotoc_md248", null ]
      ] ],
      [ "Operation Layer", "md_docs_2GPGPU-Guide.html#autotoc_md249", [
        [ "ComputeOperation<InputType, OutputType>", "md_docs_2GPGPU-Guide.html#autotoc_md250", null ],
        [ "Universal* types (CPU + optional GPU)", "md_docs_2GPGPU-Guide.html#autotoc_md251", null ],
        [ "Gpu* types (GPU only)", "md_docs_2GPGPU-Guide.html#autotoc_md252", null ]
      ] ],
      [ "Orchestration Layer", "md_docs_2GPGPU-Guide.html#autotoc_md254", [
        [ "ComputeMatrix", "md_docs_2GPGPU-Guide.html#autotoc_md255", null ],
        [ "ComputationPipeline<InputType, OutputType>", "md_docs_2GPGPU-Guide.html#autotoc_md256", null ],
        [ "ComputationGrammar", "md_docs_2GPGPU-Guide.html#autotoc_md257", null ]
      ] ],
      [ "Workflows", "md_docs_2GPGPU-Guide.html#autotoc_md259", [
        [ "GranularWorkflow", "md_docs_2GPGPU-Guide.html#autotoc_md260", null ]
      ] ],
      [ "Writing a Compute Shader for Yantra", "md_docs_2GPGPU-Guide.html#autotoc_md262", null ],
      [ "Decision Reference", "md_docs_2GPGPU-Guide.html#autotoc_md264", null ]
    ] ],
    [ "CPU-GPU IO in MayaFlux", "md_docs_2GPU-CPU-IO-Map.html", [
      [ "How to use this document", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md266", null ],
      [ "Contents", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md267", null ],
      [ "1. The three tiers of readback", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md269", null ],
      [ "2. Yantra: the Datum-driven GPGPU layer", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md271", [
        [ "Regular per-unit SSBO", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md272", null ],
        [ "Shared SSBO", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md273", null ],
        [ "GpuExecutionContext", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md274", null ],
        [ "TextureExecutionContext", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md275", null ]
      ] ],
      [ "3. VKBuffer processors: the real-time-friendly layer", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md277", [
        [ "Push constants", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md278", null ],
        [ "UBO / SSBO descriptor bindings", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md279", null ],
        [ "Textures", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md280", null ],
        [ "Getting bytes out: where the manual step lives", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md281", null ]
      ] ],
      [ "4. The raw layer underneath: Portal", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md283", null ],
      [ "5. Binding data by name: descriptors, push constants, nodes", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md285", [
        [ "Choosing an upload path", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md286", null ]
      ] ],
      [ "6. Getting data out by descriptor name, and moving whole back_buffers sets", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md288", [
        [ "Download by descriptor name: <tt>ShaderProcessor::download_bound</tt>", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md289", null ],
        [ "Download by raw handle, outside the named-binding system: <tt>StagingUtils::download_back_buffer</tt>", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md290", null ],
        [ "Whole back_buffers sets, every processing cycle: <tt>configure_back_buffers</tt> on both processors", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md291", null ]
      ] ],
      [ "7. Specialized buffer classes and why they differ", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md293", [
        [ "ComputeMeshBuffer: tier 0, by design", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md294", null ],
        [ "RelaxationGridBuffer: tier 2, request-gated", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md295", null ],
        [ "Why the three differ", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md296", null ]
      ] ],
      [ "8. Decision table", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md298", null ],
      [ "9. Design boundaries and genuine gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md300", [
        [ "Design boundaries, not gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md301", null ],
        [ "Genuine gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md302", null ]
      ] ],
      [ "How to use this document", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md303", null ],
      [ "Contents", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md304", null ],
      [ "1. The three tiers of readback", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md306", null ],
      [ "2. Yantra: the Datum-driven GPGPU layer", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md308", [
        [ "Regular per-unit SSBO", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md309", null ],
        [ "Shared SSBO", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md310", null ],
        [ "GpuExecutionContext", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md311", null ],
        [ "TextureExecutionContext", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md312", null ]
      ] ],
      [ "3. VKBuffer processors: the raw layer", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md314", [
        [ "Push constants", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md315", null ],
        [ "UBO / SSBO descriptor bindings", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md316", null ],
        [ "Textures", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md317", null ],
        [ "Getting bytes out: where the manual step lives", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md318", null ]
      ] ],
      [ "4. Binding data by name: descriptors, push constants, nodes", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md320", null ],
      [ "5. Getting data out by descriptor name, and moving whole back_buffers sets", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md322", [
        [ "Download by descriptor name: <tt>ShaderProcessor::download_bound</tt>", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md323", null ],
        [ "Download by raw handle, outside the named-binding system: <tt>StagingUtils::download_back_buffer</tt>", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md324", null ],
        [ "Whole back_buffers sets, every processing cycle: <tt>configure_back_buffers</tt> on both processors", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md325", null ],
        [ "What still does not exist", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md326", null ]
      ] ],
      [ "6. Specialized buffer classes and why they differ", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md328", [
        [ "ComputeMeshBuffer: tier 0, by design", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md329", null ],
        [ "RelaxationGridBuffer: tier 2, request-gated", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md330", null ],
        [ "Why the three differ", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md331", null ]
      ] ],
      [ "7. Decision table", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md333", null ],
      [ "8. Design boundaries and genuine gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md335", [
        [ "Design boundaries, not gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md336", null ],
        [ "Genuine gaps", "md_docs_2GPU-CPU-IO-Map.html#autotoc_md337", null ]
      ] ]
    ] ],
    [ "OpenCL to Yantra: Concept Map and Migration", "md_docs_2OpenCL-Migration.html", [
      [ "Why Yantra Over OpenCL", "md_docs_2OpenCL-Migration.html#autotoc_md339", null ],
      [ "Concept Map", "md_docs_2OpenCL-Migration.html#autotoc_md342", null ],
      [ "Step 1: Write the Shader in GLSL", "md_docs_2OpenCL-Migration.html#autotoc_md344", null ],
      [ "Step 2: Configure the Executor", "md_docs_2OpenCL-Migration.html#autotoc_md346", null ],
      [ "Step 3: Bind Buffers", "md_docs_2OpenCL-Migration.html#autotoc_md348", null ],
      [ "Step 4: Execute", "md_docs_2OpenCL-Migration.html#autotoc_md350", null ],
      [ "Step 5: Read Results", "md_docs_2OpenCL-Migration.html#autotoc_md352", null ],
      [ "Pattern Translations", "md_docs_2OpenCL-Migration.html#autotoc_md354", [
        [ "In-place modification", "md_docs_2OpenCL-Migration.html#autotoc_md355", null ],
        [ "Multiple kernels in sequence", "md_docs_2OpenCL-Migration.html#autotoc_md356", null ],
        [ "Multi-pass dispatch (e.g. bitonic sort)", "md_docs_2OpenCL-Migration.html#autotoc_md357", null ],
        [ "Image processing", "md_docs_2OpenCL-Migration.html#autotoc_md358", null ],
        [ "Processing a sub-range", "md_docs_2OpenCL-Migration.html#autotoc_md359", null ]
      ] ],
      [ "Raw GPU Access: Below the Yantra Layer", "md_docs_2OpenCL-Migration.html#autotoc_md361", [
        [ "ComputeProcessor: Buffer-attached Compute", "md_docs_2OpenCL-Migration.html#autotoc_md362", null ],
        [ "ComputePress: Raw Vulkan Compute", "md_docs_2OpenCL-Migration.html#autotoc_md363", null ],
        [ "Which Level to Use", "md_docs_2OpenCL-Migration.html#autotoc_md364", null ]
      ] ],
      [ "What Yantra Does Not Replace", "md_docs_2OpenCL-Migration.html#autotoc_md366", null ]
    ] ],
    [ "Portal::Forma::Plot", "md_docs_2Portal-Forma-Plot.html", [
      [ "Building a data source", "md_docs_2Portal-Forma-Plot.html#autotoc_md369", null ],
      [ "Describing the geometry", "md_docs_2Portal-Forma-Plot.html#autotoc_md371", [
        [ "Waveform (LINE_STRIP)", "md_docs_2Portal-Forma-Plot.html#autotoc_md372", null ],
        [ "Scatter (POINT_LIST)", "md_docs_2Portal-Forma-Plot.html#autotoc_md373", null ],
        [ "Bars (TRIANGLE_LIST)", "md_docs_2Portal-Forma-Plot.html#autotoc_md374", null ],
        [ "Multiple series, multiple colors", "md_docs_2Portal-Forma-Plot.html#autotoc_md375", null ],
        [ "Auto-scaling axis", "md_docs_2Portal-Forma-Plot.html#autotoc_md376", null ],
        [ "Background quad", "md_docs_2Portal-Forma-Plot.html#autotoc_md377", null ]
      ] ],
      [ "Placing onto a surface", "md_docs_2Portal-Forma-Plot.html#autotoc_md379", [
        [ "The manual path", "md_docs_2Portal-Forma-Plot.html#autotoc_md380", null ],
        [ "The convenience overload", "md_docs_2Portal-Forma-Plot.html#autotoc_md381", null ]
      ] ],
      [ "Raw geometry function", "md_docs_2Portal-Forma-Plot.html#autotoc_md383", null ],
      [ "Driving data", "md_docs_2Portal-Forma-Plot.html#autotoc_md385", null ],
      [ "Existing surface", "md_docs_2Portal-Forma-Plot.html#autotoc_md387", null ],
      [ "Adornments", "md_docs_2Portal-Forma-Plot.html#autotoc_md388", [
        [ "Bounds", "md_docs_2Portal-Forma-Plot.html#autotoc_md389", null ],
        [ "Tick labels", "md_docs_2Portal-Forma-Plot.html#autotoc_md390", null ],
        [ "Legend", "md_docs_2Portal-Forma-Plot.html#autotoc_md391", null ],
        [ "Free text labels", "md_docs_2Portal-Forma-Plot.html#autotoc_md392", null ],
        [ "Complete example", "md_docs_2Portal-Forma-Plot.html#autotoc_md393", null ]
      ] ],
      [ "Quick examples", "md_docs_2Portal-Forma-Plot.html#autotoc_md395", [
        [ "Static sine waveform", "md_docs_2Portal-Forma-Plot.html#autotoc_md396", null ],
        [ "Live FM oscillator", "md_docs_2Portal-Forma-Plot.html#autotoc_md397", null ],
        [ "Lissajous scatter", "md_docs_2Portal-Forma-Plot.html#autotoc_md398", null ],
        [ "Volume bars from mic input", "md_docs_2Portal-Forma-Plot.html#autotoc_md399", null ],
        [ "Three oscillators, shared Y axis, auto-scale", "md_docs_2Portal-Forma-Plot.html#autotoc_md400", null ],
        [ "Multiple X and Y on the same plot", "md_docs_2Portal-Forma-Plot.html#autotoc_md401", null ],
        [ "Raw lambda", "md_docs_2Portal-Forma-Plot.html#autotoc_md403", null ]
      ] ]
    ] ],
    [ "Portal::Forma - UI Patterns", "md_docs_2Portal-Forma-UI-Patterns.html", [
      [ "Mental model shift", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md406", null ],
      [ "Layout", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md408", null ],
      [ "ImGui::Text / label", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md410", null ],
      [ "ImGui::Button", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md412", null ],
      [ "ImGui::Checkbox / toggle", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md414", null ],
      [ "ImGui::SliderFloat / horizontal fader", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md416", null ],
      [ "ImGui::CollapsingHeader / tree node", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md418", null ],
      [ "ImGui grouped value readout panel", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md420", null ],
      [ "Drawable canvas / array editor", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md422", null ],
      [ "Visibility and z-order", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md424", null ],
      [ "Scrollable regions", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md426", null ],
      [ "Key differences from ImGui", "md_docs_2Portal-Forma-UI-Patterns.html#autotoc_md428", null ]
    ] ],
    [ "Portal::Forma", "md_docs_2Portal-Forma.html", [
      [ "Concepts", "md_docs_2Portal-Forma.html#autotoc_md431", null ],
      [ "Creating a surface", "md_docs_2Portal-Forma.html#autotoc_md433", null ],
      [ "Static geometry, manual buffer", "md_docs_2Portal-Forma.html#autotoc_md435", null ],
      [ "Kinesis::Geometry2D - static vertex data", "md_docs_2Portal-Forma.html#autotoc_md437", [
        [ "Filled shapes - <tt>Kakshya::Vertex</tt>, TRIANGLE_LIST", "md_docs_2Portal-Forma.html#autotoc_md438", null ],
        [ "Outlines - <tt>Kakshya::LineVertex</tt>, LINE_LIST", "md_docs_2Portal-Forma.html#autotoc_md439", null ],
        [ "Path sampling", "md_docs_2Portal-Forma.html#autotoc_md440", null ]
      ] ],
      [ "Geometry functions for Mapped<T>", "md_docs_2Portal-Forma.html#autotoc_md442", [
        [ "Controls", "md_docs_2Portal-Forma.html#autotoc_md443", null ],
        [ "Readouts", "md_docs_2Portal-Forma.html#autotoc_md444", null ],
        [ "Positional", "md_docs_2Portal-Forma.html#autotoc_md445", null ],
        [ "Drawable canvas", "md_docs_2Portal-Forma.html#autotoc_md446", null ]
      ] ],
      [ "Element spatial description", "md_docs_2Portal-Forma.html#autotoc_md448", null ],
      [ "Pointer events", "md_docs_2Portal-Forma.html#autotoc_md450", null ],
      [ "Keyboard focus and key events", "md_docs_2Portal-Forma.html#autotoc_md452", null ],
      [ "Texture and text on an Element", "md_docs_2Portal-Forma.html#autotoc_md454", null ],
      [ "Mapped<T> : typed dynamic geometry", "md_docs_2Portal-Forma.html#autotoc_md456", null ],
      [ "Slot : post-registration mutations", "md_docs_2Portal-Forma.html#autotoc_md458", null ],
      [ "Bridge : two-way binding", "md_docs_2Portal-Forma.html#autotoc_md460", [
        [ "Inbound : node or callable drives the element", "md_docs_2Portal-Forma.html#autotoc_md461", null ],
        [ "Outbound : element value drives a node or shader", "md_docs_2Portal-Forma.html#autotoc_md462", null ],
        [ "Chaining both directions", "md_docs_2Portal-Forma.html#autotoc_md463", null ],
        [ "Draggable fader writing to a node : full example", "md_docs_2Portal-Forma.html#autotoc_md464", null ]
      ] ],
      [ "Quick examples", "md_docs_2Portal-Forma.html#autotoc_md466", [
        [ "Mouse follower", "md_docs_2Portal-Forma.html#autotoc_md467", null ],
        [ "Radial indicator", "md_docs_2Portal-Forma.html#autotoc_md468", null ],
        [ "Horizontal fader", "md_docs_2Portal-Forma.html#autotoc_md469", null ],
        [ "2D position picker", "md_docs_2Portal-Forma.html#autotoc_md470", null ],
        [ "Labeled interactive button", "md_docs_2Portal-Forma.html#autotoc_md471", null ]
      ] ],
      [ "Pure hit-test regions", "md_docs_2Portal-Forma.html#autotoc_md473", null ]
    ] ],
    [ "Portal::Text", "md_docs_2Portal-Text.html", [
      [ "Coordinate system", "md_docs_2Portal-Text.html#autotoc_md476", null ],
      [ "Coordinate conversion, the API functions", "md_docs_2Portal-Text.html#autotoc_md478", [
        [ "Pixel position → NDC position", "md_docs_2Portal-Text.html#autotoc_md479", null ],
        [ "NDC position → pixel position", "md_docs_2Portal-Text.html#autotoc_md480", null ],
        [ "NDC extent → pixel dimensions (for render_bounds)", "md_docs_2Portal-Text.html#autotoc_md481", null ]
      ] ],
      [ "render_bounds vs. set_scale", "md_docs_2Portal-Text.html#autotoc_md483", null ],
      [ "Placing text at a specific pixel position", "md_docs_2Portal-Text.html#autotoc_md485", [
        [ "Common placements", "md_docs_2Portal-Text.html#autotoc_md486", null ]
      ] ],
      [ "Working with a window object", "md_docs_2Portal-Text.html#autotoc_md488", null ],
      [ "Moving text", "md_docs_2Portal-Text.html#autotoc_md490", null ],
      [ "Updating text content", "md_docs_2Portal-Text.html#autotoc_md492", null ],
      [ "press as VKImage (for FormaBuffer)", "md_docs_2Portal-Text.html#autotoc_md494", null ],
      [ "budget_h", "md_docs_2Portal-Text.html#autotoc_md496", null ]
    ] ],
    [ "MayaFlux Engine Configuration", "md_docs_2Settings.html", [
      [ "Configuration via JSON file", "md_docs_2Settings.html#autotoc_md499", null ],
      [ "Audio stream: <tt>GlobalStreamInfo</tt>", "md_docs_2Settings.html#autotoc_md501", null ],
      [ "Graphics: <tt>GlobalGraphicsConfig</tt>", "md_docs_2Settings.html#autotoc_md503", null ],
      [ "Input: <tt>GlobalInputConfig</tt>", "md_docs_2Settings.html#autotoc_md505", null ],
      [ "Network: <tt>GlobalNetworkConfig</tt>", "md_docs_2Settings.html#autotoc_md507", null ],
      [ "Journal (logging)", "md_docs_2Settings.html#autotoc_md509", null ],
      [ "Reading config at runtime", "md_docs_2Settings.html#autotoc_md511", null ],
      [ "Common configurations", "md_docs_2Settings.html#autotoc_md513", null ]
    ] ],
    [ "ViewTransform", "md_docs_2Viewport.html", [
      [ "The coordinate system", "md_docs_2Viewport.html#autotoc_md516", null ],
      [ "eye, target, up", "md_docs_2Viewport.html#autotoc_md518", null ],
      [ "FOV, aspect, near, far", "md_docs_2Viewport.html#autotoc_md520", null ],
      [ "Placing the camera to see your geometry", "md_docs_2Viewport.html#autotoc_md522", null ],
      [ "Aligning geometry to the camera", "md_docs_2Viewport.html#autotoc_md524", null ],
      [ "Multiple buffers, one viewpoint", "md_docs_2Viewport.html#autotoc_md526", null ],
      [ "Interactive navigation: bind_viewport_preset", "md_docs_2Viewport.html#autotoc_md528", null ],
      [ "Nexus: Locus", "md_docs_2Viewport.html#autotoc_md530", null ],
      [ "Audio-driven viewpoint", "md_docs_2Viewport.html#autotoc_md532", null ]
    ] ],
    [ "Todo List", "todo.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"API_2Random_8cpp.html",
"BufferInputControl_8hpp.html",
"CompositeGeometryBuffer_8cpp.html",
"Core_8hpp.html",
"Emitter_8hpp_source.html",
"FormaBuffer_8cpp_source.html",
"GlobalGraphicsInfo_8hpp_source.html",
"Graph_8hpp.html#af8a61468789afc248ee2f890176d0fcd",
"InputAudioBuffer_8cpp_source.html",
"Keys_8hpp.html#a26e87bc07718489a8bb883da3bd4216eab1ca34f82e83c52b010f86955f264e05",
"MathematicalTransformer_8hpp.html#a48e533bd2ca1105afab7227143eb9f42ac9c9c146c630ca5ef9197c73c032f4a6",
"NavigationState_8hpp.html#ac32839d2593eb11735b40cd2829b72a3",
"OpticalFlow_8hpp.html",
"ProcessingTokens_8hpp.html#a77f95e569fd1fad7cb269c075de408c8a001596ccc0054dcb06d49d6819b26f99",
"RootBuffer_8hpp_source.html",
"Sinks_8cpp.html#af91b9d6cc18e3069164b9a8c0eac027d",
"Stochastic_8hpp.html#ae30c6bf7c61418c95fe386079d91ff48a87a489dcdf02ac2b243374ca9cc2d3b6",
"TokenUnitManager_8hpp.html#a9834dbbd44ea8a4e40d76837acfd8cbd",
"VKInstance_8cpp_source.html",
"VisionOp_8hpp.html#aa08821254b3e9e047b89a44e49db501eaa45a17f18403288cff3c1121c3064b72",
"Yantra_8hpp.html#a16c4f634b28da9a06b7edca8f7e9fcde",
"classLila_1_1Lila_ab866b1593249b5f8d207ec835b3bf828.html#ab866b1593249b5f8d207ec835b3bf828",
"classMayaFlux_1_1Buffers_1_1AudioBuffer_ae951dfe24fa0cdbf254622e694f0da8e.html#ae951dfe24fa0cdbf254622e694f0da8e",
"classMayaFlux_1_1Buffers_1_1BufferManager_ac9bcede0b7f987c60b4b33f1254153d3.html#ac9bcede0b7f987c60b4b33f1254153d3",
"classMayaFlux_1_1Buffers_1_1BufferTokenDistributor_a43fef03e0b2432629d213174b1028215.html#a43fef03e0b2432629d213174b1028215",
"classMayaFlux_1_1Buffers_1_1ComputeProcessor_aef0abe04f4545e1701e0e1c1273e61b2.html#aef0abe04f4545e1701e0e1c1273e61b2",
"classMayaFlux_1_1Buffers_1_1FilterProcessor_af38aee4c0b2dcdbd98dff56b4d4a8b79.html#af38aee4c0b2dcdbd98dff56b4d4a8b79",
"classMayaFlux_1_1Buffers_1_1ImageCVProcessor_ac1701002e9f4b2fca47762bffc3da8c1.html#ac1701002e9f4b2fca47762bffc3da8c1",
"classMayaFlux_1_1Buffers_1_1MeshBuffer_a9c5c6cf9cc8c12935a9dbdea04a88044.html#a9c5c6cf9cc8c12935a9dbdea04a88044",
"classMayaFlux_1_1Buffers_1_1NetworkTextureBuffer_a0fedfa39434a9275336f625b34c1ba2d.html#a0fedfa39434a9275336f625b34c1ba2d",
"classMayaFlux_1_1Buffers_1_1PresentProcessor_aa85ed35ff90f9646d187857614e28da3.html#aa85ed35ff90f9646d187857614e28da3",
"classMayaFlux_1_1Buffers_1_1RootAudioBuffer.html",
"classMayaFlux_1_1Buffers_1_1SDFPrepProcessor_aeebc0bc905e4f68a06ca08a82bc3b336.html#aeebc0bc905e4f68a06ca08a82bc3b336",
"classMayaFlux_1_1Buffers_1_1SoundStreamWriter_a709423fc62bda853de9cefc65026c939.html#a709423fc62bda853de9cefc65026c939",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_a012057e576d30bf8dce1f3870d59a4dd.html#a012057e576d30bf8dce1f3870d59a4dd",
"classMayaFlux_1_1Buffers_1_1VKBuffer_a82cf27c82341d3cd5af761af88a1e08f.html#a82cf27c82341d3cd5af761af88a1e08f",
"classMayaFlux_1_1Core_1_1AudioSubsystem_a901fabae6c3bd8f099ab2381ba358ac4.html#a901fabae6c3bd8f099ab2381ba358ac4",
"classMayaFlux_1_1Core_1_1DescriptorUpdateBatch_a59216dc6ae970cccaeee02da88ed7c5c.html#a59216dc6ae970cccaeee02da88ed7c5c",
"classMayaFlux_1_1Core_1_1HIDBackend_a7fa1f678f140a64df0eae495b48adde5.html#a7fa1f678f140a64df0eae495b48adde5",
"classMayaFlux_1_1Core_1_1InputManager_a417f5c180483dbf99d928ee7f53893e0.html#a417f5c180483dbf99d928ee7f53893e0",
"classMayaFlux_1_1Core_1_1NodeProcessingHandle_a6a99663ddec0b3a56151138b5abeffff.html#a6a99663ddec0b3a56151138b5abeffff",
"classMayaFlux_1_1Core_1_1TCPBackend_a5a7c58ced329ff4baa4c19873f189e65.html#a5a7c58ced329ff4baa4c19873f189e65",
"classMayaFlux_1_1Core_1_1VKComputePipeline_a93460232cb244acac2afdfb9e5b57a26.html#a93460232cb244acac2afdfb9e5b57a26",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a4013f50f5c9fb9cfacca4aa2e6932fa9.html#a4013f50f5c9fb9cfacca4aa2e6932fa9",
"classMayaFlux_1_1Core_1_1VKRenderPass_ae5747cbc6c391f9fc86237003cfee1e1.html#ae5747cbc6c391f9fc86237003cfee1e1",
"classMayaFlux_1_1Core_1_1WindowManager_a2b26339c90e40db5b9bab159be57a6e7.html#a2b26339c90e40db5b9bab159be57a6e7",
"classMayaFlux_1_1IO_1_1AudioEncodeContext_a65e8da1658966599181cc1eb02f3dfe2.html#a65e8da1658966599181cc1eb02f3dfe2",
"classMayaFlux_1_1IO_1_1FFmpegMuxContext_aa6ac5e91b58c8250b2cbf57cfe5be24d.html#aa6ac5e91b58c8250b2cbf57cfe5be24d",
"classMayaFlux_1_1IO_1_1IOManager_afe7cf13d38047732cf278c8e4d57657b.html#afe7cf13d38047732cf278c8e4d57657b",
"classMayaFlux_1_1IO_1_1SoundFileReader_a4f7a6ceb98290998b95c4f95b79aa51c.html#a4f7a6ceb98290998b95c4f95b79aa51c",
"classMayaFlux_1_1IO_1_1VideoFileReader_a1883155dfaa25f21f0bbc71fa28e1796.html#a1883155dfaa25f21f0bbc71fa28e1796",
"classMayaFlux_1_1IO_1_1VideoStreamContext_a54791e6d40323a14ae2da74a1db4c74c.html#a54791e6d40323a14ae2da74a1db4c74c",
"classMayaFlux_1_1Kakshya_1_1CameraContainer_a55cca5bc67bab9ac239a43d15646419e.html#a55cca5bc67bab9ac239a43d15646419e",
"classMayaFlux_1_1Kakshya_1_1DynamicRegionProcessor_a11876d04bdc74ab49edcc80f38eab4e9.html#a11876d04bdc74ab49edcc80f38eab4e9",
"classMayaFlux_1_1Kakshya_1_1MeshInsertion_a22fb32878373c43a18944736827c9fcf.html#a22fb32878373c43a18944736827c9fcf",
"classMayaFlux_1_1Kakshya_1_1PlotContainer_ada434c5519b35edfb4f165961720e068.html#ada434c5519b35edfb4f165961720e068",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_aa742b31e5906da1efdac0637d2ad40ff.html#aa742b31e5906da1efdac0637d2ad40ff",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_adab595bb0b0f3acc2900e680a4d0ad63.html#adab595bb0b0f3acc2900e680a4d0ad63",
"classMayaFlux_1_1Kakshya_1_1TextureContainer_a7ca2c16eb765824ba49bb14a323c647f.html#a7ca2c16eb765824ba49bb14a323c647f",
"classMayaFlux_1_1Kakshya_1_1VideoStreamContainer_a70d9c6ce9bff02add373061e2d273aa1.html#a70d9c6ce9bff02add373061e2d273aa1",
"classMayaFlux_1_1Kakshya_1_1WindowContainer_a259bddb0afab69e8b4084468068401bd.html#a259bddb0afab69e8b4084468068401bd",
"classMayaFlux_1_1Kinesis_1_1Stochastic_1_1Stochastic_a3046045334b43a91988a468821bd56f7.html#a3046045334b43a91988a468821bd56f7",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a6d1bbc9b23723e197df1d8a35ae02d58.html#a6d1bbc9b23723e197df1d8a35ae02d58",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a728c8e0438ddb2a2334b343b8b7af760.html#a728c8e0438ddb2a2334b343b8b7af760",
"classMayaFlux_1_1Kriya_1_1SamplingPipeline_a6dfecafed93d9c7ba07efb32ab33d5f5.html#a6dfecafed93d9c7ba07efb32ab33d5f5",
"classMayaFlux_1_1Memory_1_1RingBuffer_a7064798e685f76f06fda85292b5e3e87.html#a7064798e685f76f06fda85292b5e3e87",
"classMayaFlux_1_1Nexus_1_1Agent_ae0c6c27c0fe5274a824d5c321eb65419.html#ae0c6c27c0fe5274a824d5c321eb65419",
"classMayaFlux_1_1Nexus_1_1Fabric_aca5573b30bbff2acdd5a8321ad509115.html#aca5573b30bbff2acdd5a8321ad509115",
"classMayaFlux_1_1Nexus_1_1Tapestry_af51e64ea70bcded0fe2e04b5949ec8c7.html#af51e64ea70bcded0fe2e04b5949ec8c7",
"classMayaFlux_1_1Nodes_1_1ChainNode_a11b4161bd4cb8ffd6fea27f987c261d0.html#a11b4161bd4cb8ffd6fea27f987c261d0",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_abafc4764937a7175c7c5ac50e265d6ee.html#abafc4764937a7175c7c5ac50e265d6ee",
"classMayaFlux_1_1Nodes_1_1Generator_1_1LogicContextGpu_ac43d5e79030c986e4bd98e5155d079a4.html#ac43d5e79030c986e4bd98e5155d079a4",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a5d26d5499d5188485df59983fb3110ad.html#a5d26d5499d5188485df59983fb3110ad",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Sine_a5109ee00efbeeca62a1be0399ce6fec4.html#a5109ee00efbeeca62a1be0399ce6fec4",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1GlyphGeometryNode_ace11f527bc2eb1592c751a794ca3eb39.html#ace11f527bc2eb1592c751a794ca3eb39",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PathGeneratorNode_afb639e46c6bbb66eb7ceb43fb3b9da00.html#afb639e46c6bbb66eb7ceb43fb3b9da00",
"classMayaFlux_1_1Nodes_1_1GpuVectorData_abb103f22a2e373a6940f2959875e4307.html#abb103f22a2e373a6940f2959875e4307",
"classMayaFlux_1_1Nodes_1_1Network_1_1FieldOperator_a8579f3270427c401621ea74baa4d7644.html#a8579f3270427c401621ea74baa4d7644",
"classMayaFlux_1_1Nodes_1_1Network_1_1MeshNetwork_a5b42c177730808da49c55a79922d28ac.html#a5b42c177730808da49c55a79922d28ac",
"classMayaFlux_1_1Nodes_1_1Network_1_1NetworkOperator_ae4e127405c4de3658a7857fae8e14daa.html#ae4e127405c4de3658a7857fae8e14daa",
"classMayaFlux_1_1Nodes_1_1Network_1_1PathOperator_a2bfd2bf60eae17e7ba7bd5eebcebc318.html#a2bfd2bf60eae17e7ba7bd5eebcebc318",
"classMayaFlux_1_1Nodes_1_1Network_1_1PointCloudNetwork_a687e6a3c8fe36dd7177d76f5f52b8e72.html#a687e6a3c8fe36dd7177d76f5f52b8e72",
"classMayaFlux_1_1Nodes_1_1Network_1_1WaveguideNetwork_a4701320c67ce40f2f4b66b9705fc6053.html#a4701320c67ce40f2f4b66b9705fc6053",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_af36a43f11ffe7356d9ef06260866f5f1.html#af36a43f11ffe7356d9ef06260866f5f1",
"classMayaFlux_1_1Platform_1_1SystemConfig_a2391ac046f63f2050d9d0e9f19fc2545.html#a2391ac046f63f2050d9d0e9f19fc2545",
"classMayaFlux_1_1Portal_1_1Forma_1_1Inspector_a5ca05fabd37053054234e93871b9c551.html#a5ca05fabd37053054234e93871b9c551",
"classMayaFlux_1_1Portal_1_1Forma_1_1Plot_1_1Series_aa8cbdf47e6698bfe8fc36097976f5174.html#aa8cbdf47e6698bfe8fc36097976f5174",
"classMayaFlux_1_1Portal_1_1Graphics_1_1SamplerForge_a1f850f0d077848db00815e87087c3765.html#a1f850f0d077848db00815e87087c3765",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_aee28a298127f15b6af5ceb8d62c5fdf9.html#aee28a298127f15b6af5ceb8d62c5fdf9aac404d7ce6ca5862e9b8a2641fbafd90",
"classMayaFlux_1_1Portal_1_1Text_1_1GlyphAtlas_ae3d70472f0d76785ba89fbbeb784d980.html#ae3d70472f0d76785ba89fbbeb784d980",
"classMayaFlux_1_1Vruta_1_1EventSource.html",
"classMayaFlux_1_1Vruta_1_1IClock_a8c7d05c75e9ebac85e2a3ea253be7e1a.html#a8c7d05c75e9ebac85e2a3ea253be7e1a",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a64c2a1b202d3cf66d57f633a383640fe.html#a64c2a1b202d3cf66d57f633a383640fe",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_a3c47752c035032798cdf20c24e12cd0f.html#a3c47752c035032798cdf20c24e12cd0f",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer_aecc4ab5034386f7a907c980901219bed.html#aecc4ab5034386f7a907c980901219bed",
"classMayaFlux_1_1Yantra_1_1GpuDispatchCore_ae5e1220e72a191e7b1200563b38e2c64.html#ae5e1220e72a191e7b1200563b38e2c64",
"classMayaFlux_1_1Yantra_1_1OpUnit_a8c286ae2dd1ef8d109c11a480909f349.html#a8c286ae2dd1ef8d109c11a480909f349",
"classMayaFlux_1_1Yantra_1_1StandardSorter_a387fd994e714a76dddfd84a8f8deae19.html#a387fd994e714a76dddfd84a8f8deae19",
"classMayaFlux_1_1Yantra_1_1UniversalAnalyzer_a260f520e4f6bf1aa309439aa084069e8.html#a260f520e4f6bf1aa309439aa084069e8",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_a4b29bc78fc586c1a515295eea9602833.html#a4b29bc78fc586c1a515295eea9602833",
"dir_30c551cdb1b5e5e8d34a68131522bc06.html",
"md_README.html#autotoc_md82",
"namespaceColors.html",
"namespaceMayaFlux_1_1IO_a36a3852542b551e90c97c41c9ed56f62.html#a36a3852542b551e90c97c41c9ed56f62a37f438df6a6d5ba4c17ef8ca58562f00",
"namespaceMayaFlux_1_1Journal_ad918cd0db16e3f2184007817220b837f.html#ad918cd0db16e3f2184007817220b837fae45d507b768320ca2a65da43ce67fe17",
"namespaceMayaFlux_1_1Kinesis_1_1Discrete_a36874cd6d5db1b2e1769c5fcfd562e3e.html#a36874cd6d5db1b2e1769c5fcfd562e3e",
"namespaceMayaFlux_1_1Kinesis_a244d0940ab7ca614369c46022d56b2d1.html#a244d0940ab7ca614369c46022d56b2d1aaac544aacc3615aada24897a215f5046",
"namespaceMayaFlux_1_1Nodes_1_1Generator_a171991cadaf912b6ee99413edf9d5bd9.html#a171991cadaf912b6ee99413edf9d5bd9a520d95b44042c835ac7ac09539f5c757",
"namespaceMayaFlux_1_1Portal_1_1Graphics.html",
"namespaceMayaFlux_1_1Portal_1_1Text_ab942e659934e6d68aa5f94300b7ed912.html#ab942e659934e6d68aa5f94300b7ed912",
"namespaceMayaFlux_1_1Yantra_a6b2561cf59d7422a997753a425decd3b.html#a6b2561cf59d7422a997753a425decd3b",
"namespaceMayaFlux_a08fa9b6412b634215b72b52676318691.html#a08fa9b6412b634215b72b52676318691",
"namespaceMayaFlux_adbd735038cb59c42bd089be2299188df.html#adbd735038cb59c42bd089be2299188df",
"structMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_1_1DescriptorBinding_a8eef28db371207b406b9096997c534f6.html#a8eef28db371207b406b9096997c534f6",
"structMayaFlux_1_1Buffers_1_1SDFFieldProcessor_1_1PC_a21b006d019e95038a621a04502fd0a52.html#a21b006d019e95038a621a04502fd0a52",
"structMayaFlux_1_1Core_1_1ColorBlendAttachment_ad750dc9b53267399e2acd91e66970a62.html#ad750dc9b53267399e2acd91e66970a62",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_abde54239b8ff211b42ab35ad102dfc61.html#abde54239b8ff211b42ab35ad102dfc61a47428d834e54119100c608d0d1448e51",
"structMayaFlux_1_1Core_1_1GraphicsSurfaceInfo_a30c1bd5133612e8fd6dc83de0de6a784.html#a30c1bd5133612e8fd6dc83de0de6a784aee96bf7dfce2074d244fee481058adca",
"structMayaFlux_1_1Core_1_1InputValue_a15c8d662a26a110fac92e92c0e4de474.html#a15c8d662a26a110fac92e92c0e4de474",
"structMayaFlux_1_1Core_1_1SubpassDependency_a13fb7d64d104af3283151782c0cdd415.html#a13fb7d64d104af3283151782c0cdd415",
"structMayaFlux_1_1Core_1_1WindowCreateInfo_a2c50d7e19b834ff135f6208857c8dcf7.html#a2c50d7e19b834ff135f6208857c8dcf7",
"structMayaFlux_1_1IO_1_1ImageData_a956b30b2aa8da62a47ad1d388a7d68d6.html#a956b30b2aa8da62a47ad1d388a7d68d6",
"structMayaFlux_1_1Kakshya_1_1DataConverter_3_01T_00_01T_01_4_a8968aa05ac8a3a3a7cdf1bf9cd78bef4.html#a8968aa05ac8a3a3a7cdf1bf9cd78bef4",
"structMayaFlux_1_1Kakshya_1_1PlotProcessor_1_1SeriesBinding_a36c04c5d1748ecf7b85d85d0e6214a21.html#a36c04c5d1748ecf7b85d85d0e6214a21",
"structMayaFlux_1_1Kakshya_1_1TextureAccess.html",
"structMayaFlux_1_1Kinesis_1_1NavigationState.html",
"structMayaFlux_1_1Kinesis_1_1ViewTransform_a661d52f115a3aa395e56c1cf53c64c2c.html#a661d52f115a3aa395e56c1cf53c64c2c",
"structMayaFlux_1_1Kriya_1_1BufferPipeline_1_1BranchInfo.html",
"structMayaFlux_1_1Nexus_1_1InfluenceContext_a62e94c9bd9cd514d73d5e647e1ea313c.html#a62e94c9bd9cd514d73d5e647e1ea313c",
"structMayaFlux_1_1Nexus_1_1Wiring_1_1KeyTrigger_a10dcf457d4dfcde0b2aeb3eee15f8e9b.html#a10dcf457d4dfcde0b2aeb3eee15f8e9b",
"structMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_1_1ModalNode_a73d85c951516c061de38bcb5e45a3b7e.html#a73d85c951516c061de38bcb5e45a3b7e",
"structMayaFlux_1_1Portal_1_1Forma_1_1Element_ab9fad7653854864f95ac48846f023b5c.html#ab9fad7653854864f95ac48846f023b5c",
"structMayaFlux_1_1Portal_1_1Forma_1_1ValueSpec_a91744a363d41eda841fa32d72b707afa.html#a91744a363d41eda841fa32d72b707afa",
"structMayaFlux_1_1Portal_1_1Graphics_1_1RenderPipelineConfig_ac18d7589084a9fcb407563cb5d25fed0.html#ac18d7589084a9fcb407563cb5d25fed0",
"structMayaFlux_1_1Reflect_1_1is__optional.html",
"structMayaFlux_1_1Vruta_1_1routine__promise_a27b829c293e7ede098041506a1668d8e.html#a27b829c293e7ede098041506a1668d8e",
"structMayaFlux_1_1Yantra_1_1ExecutionContext_a55fe2a9fbb20f27d4ccb5e38df77a320.html#a55fe2a9fbb20f27d4ccb5e38df77a320",
"structMayaFlux_1_1Yantra_1_1extraction__traits_3_01Kakshya_1_1DataVariant_01_4_a1a9746682b8c5d117fe3cbe68fcf4d7f.html#a1a9746682b8c5d117fe3cbe68fcf4d7f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';