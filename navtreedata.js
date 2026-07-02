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
      [ "Everything Is Numbers", "md_docs_2Digital__Architecture.html#autotoc_md195", null ],
      [ "The Real-Time Core: Nodes, Buffers, Vruta, Kriya", "md_docs_2Digital__Architecture.html#autotoc_md197", [
        [ "Nodes", "md_docs_2Digital__Architecture.html#autotoc_md198", null ],
        [ "Buffers", "md_docs_2Digital__Architecture.html#autotoc_md199", null ],
        [ "Processors", "md_docs_2Digital__Architecture.html#autotoc_md200", null ],
        [ "Vruta", "md_docs_2Digital__Architecture.html#autotoc_md201", null ],
        [ "Kriya", "md_docs_2Digital__Architecture.html#autotoc_md202", null ]
      ] ],
      [ "Kakshya: The Data Layer", "md_docs_2Digital__Architecture.html#autotoc_md204", null ],
      [ "Yantra: The Offline Computation Universe", "md_docs_2Digital__Architecture.html#autotoc_md206", null ],
      [ "The Visual Pipeline", "md_docs_2Digital__Architecture.html#autotoc_md208", [
        [ "Portal", "md_docs_2Digital__Architecture.html#autotoc_md209", null ],
        [ "Registry", "md_docs_2Digital__Architecture.html#autotoc_md210", null ],
        [ "Core and Subsystems", "md_docs_2Digital__Architecture.html#autotoc_md211", null ]
      ] ],
      [ "Nexus: Spatial Entity Simulation", "md_docs_2Digital__Architecture.html#autotoc_md213", null ],
      [ "Kinesis: The Mathematical Substrate", "md_docs_2Digital__Architecture.html#autotoc_md215", null ],
      [ "IO: Reading and Writing the World", "md_docs_2Digital__Architecture.html#autotoc_md217", null ],
      [ "Journal: Structured Logging", "md_docs_2Digital__Architecture.html#autotoc_md219", null ],
      [ "Transitive: Framework-Independent Utilities", "md_docs_2Digital__Architecture.html#autotoc_md221", null ],
      [ "Lila: The Live Layer", "md_docs_2Digital__Architecture.html#autotoc_md223", null ],
      [ "Tokens and Domain", "md_docs_2Digital__Architecture.html#autotoc_md225", null ],
      [ "How the System Composes", "md_docs_2Digital__Architecture.html#autotoc_md227", null ]
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
"CompositeGeometryBuffer_8cpp_source.html",
"Core_8hpp.html#a95b762f8dd1fecebfd279d5d34107d23",
"EnergyAnalyzer_8hpp.html#a0e8b275ccdb5a2b0376057afb9b01897",
"Forma_8cpp.html#a375aa9c35d0d0831a7d4e0ea7107b227",
"GlobalNetworkConfig_8hpp.html#a4d48a7abfbddf63004ecb33fed2a8220a5dc40c76033f8ccb551ebd91b7120cf4",
"GraphicsSubsystem_8hpp_source.html",
"InputEvents_8hpp.html#a01aa3f167bd1f04105be7ff4d6c63862",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62a4787509ad9f9d747a81a30e9dde3d4a7",
"MatrixTransforms_8hpp.html#a03c539da01191aefc423d87290d9d3c2",
"NetworkGeometryProcessor_8cpp.html",
"PanZoom2DState_8cpp.html#a07e10918948daa6b2fcdda2181b6b539",
"ProximityGraphs_8cpp_a99108733d00274978a4979dc072bd513.html#a99108733d00274978a4979dc072bd513",
"SDFNode_8cpp.html",
"SortingHelper_8hpp.html#a8999d56fbd1dacad722668cf3f2c0694",
"SurfaceUtils_8hpp_source.html",
"TypeInfo_8hpp.html#aafabb9cd001f95e3b935f236ca0c7fd3",
"VertexSampler_8hpp.html",
"Windowing_8cpp.html#ab6848a5a849d0ed1d8795ec1d2a37e0e",
"classLila_1_1ClangInterpreter_a6ef635a597980c0cf681f993b4d301a1.html#a6ef635a597980c0cf681f993b4d301a1",
"classLila_a7a579d089ff31a5784ef2ae419e9c9ea.html#a7a579d089ff31a5784ef2ae419e9c9eaabb1ca97ec761fc37101737ba0aa2e7c5",
"classMayaFlux_1_1Buffers_1_1BufferDownloadProcessor_a4c14ae413c71bbefcaeb0faaf1ecd111.html#a4c14ae413c71bbefcaeb0faaf1ecd111",
"classMayaFlux_1_1Buffers_1_1BufferProcessingChain_aed885102ffea0be56c2193c4a0a5506b.html#aed885102ffea0be56c2193c4a0a5506b",
"classMayaFlux_1_1Buffers_1_1CompositeGeometryBuffer_a829392d13a7248e212b098a55857794b.html#a829392d13a7248e212b098a55857794b",
"classMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_ab90b0847540a860cfbeb6fb9649ae0e1.html#ab90b0847540a860cfbeb6fb9649ae0e1",
"classMayaFlux_1_1Buffers_1_1FormaProcessor_af839baa802637f8414d8e214628b4e7e.html#af839baa802637f8414d8e214628b4e7e",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_a402bf233fc26944177992aab5f7406b1.html#a402bf233fc26944177992aab5f7406b1",
"classMayaFlux_1_1Buffers_1_1NetworkAudioBuffer.html",
"classMayaFlux_1_1Buffers_1_1NodeTextureProcessor_a5ec9536e38c507b39c0bd3b5b28a90c3.html#a5ec9536e38c507b39c0bd3b5b28a90c3",
"classMayaFlux_1_1Buffers_1_1RootAudioBuffer_a9738464fde6976bbd58e095d6bc9f210.html#a9738464fde6976bbd58e095d6bc9f210",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a08f86ff68a93950b07208ee936badf8e.html#a08f86ff68a93950b07208ee936badf8e",
"classMayaFlux_1_1Buffers_1_1StreamSliceProcessor_a4d90149629d432046d55a734cac89784.html#a4d90149629d432046d55a734cac89784",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_a52225b0b5bd1248a600ecab7ac39ff80.html#a52225b0b5bd1248a600ecab7ac39ff80",
"classMayaFlux_1_1Buffers_1_1VKBuffer_a8cdb58c91411faea0e43e30902930180.html#a8cdb58c91411faea0e43e30902930180",
"classMayaFlux_1_1Core_1_1AudioSubsystem_a99b1357434cbb2c55b5cf4c3f14259a9.html#a99b1357434cbb2c55b5cf4c3f14259a9",
"classMayaFlux_1_1Core_1_1DescriptorUpdateBatch_ac1a97e630626ac3c14ed6cfb638b01c7.html#ac1a97e630626ac3c14ed6cfb638b01c7",
"classMayaFlux_1_1Core_1_1HIDBackend_a923efc2f05b2c0ef0e8adf1892d31c1e.html#a923efc2f05b2c0ef0e8adf1892d31c1e",
"classMayaFlux_1_1Core_1_1InputManager_a52e1248fb673347274d98764e30fadb4.html#a52e1248fb673347274d98764e30fadb4",
"classMayaFlux_1_1Core_1_1NodeProcessingHandle_a9e484d01e9407b050b815bcf24cfc9f5.html#a9e484d01e9407b050b815bcf24cfc9f5",
"classMayaFlux_1_1Core_1_1TCPBackend_a5fcbb1af20b970090f5ac54e9b433629.html#a5fcbb1af20b970090f5ac54e9b433629",
"classMayaFlux_1_1Core_1_1VKComputePipeline_ab298ebfb607a4ea9beba4c5903bf110b.html#ab298ebfb607a4ea9beba4c5903bf110b",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a514d86dcb1a0a59b9d5adb1212d5a334.html#a514d86dcb1a0a59b9d5adb1212d5a334",
"classMayaFlux_1_1Core_1_1VKShaderModule_a090c9bd6fde6629f054ac03129df8267.html#a090c9bd6fde6629f054ac03129df8267",
"classMayaFlux_1_1Core_1_1WindowManager_a40e562572ff2ba72f7e5eeadc21aaa9b.html#a40e562572ff2ba72f7e5eeadc21aaa9b",
"classMayaFlux_1_1IO_1_1AudioEncodeContext_a88c4b55c713e47e49eab9f84eede726f.html#a88c4b55c713e47e49eab9f84eede726f",
"classMayaFlux_1_1IO_1_1FFmpegMuxContext_ad9c0d65d550d9ca8b5d46e69edc20534.html#ad9c0d65d550d9ca8b5d46e69edc20534",
"classMayaFlux_1_1IO_1_1ImageReader_a1f82fd05f44b3ed3d3d17dda265e4fb9.html#a1f82fd05f44b3ed3d3d17dda265e4fb9",
"classMayaFlux_1_1IO_1_1SoundFileReader_a55edd9ee43004846f85961b546c811c9.html#a55edd9ee43004846f85961b546c811c9",
"classMayaFlux_1_1IO_1_1VideoFileReader_a1bc9872591d502c748c5c2f0b42bc2e0.html#a1bc9872591d502c748c5c2f0b42bc2e0",
"classMayaFlux_1_1IO_1_1VideoStreamContext_a5a5ad532a1f914770743a203e766c9d7.html#a5a5ad532a1f914770743a203e766c9d7",
"classMayaFlux_1_1Kakshya_1_1CameraContainer_ac1dc36ef403cc034c2cab7329fbdb21a.html#ac1dc36ef403cc034c2cab7329fbdb21a",
"classMayaFlux_1_1Kakshya_1_1DynamicRegionProcessor_a90d85311a7740e69ea4662ae29370a75.html#a90d85311a7740e69ea4662ae29370a75",
"classMayaFlux_1_1Kakshya_1_1MeshInsertion_a4e4acb9fff17b9b75dc4d5b1adf858c8.html#a4e4acb9fff17b9b75dc4d5b1adf858c8",
"classMayaFlux_1_1Kakshya_1_1PlotContainer_ae3eb0422e37baf35f124e7d60942b354.html#ae3eb0422e37baf35f124e7d60942b354",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_abee9ba631807533f90b6a7612191213d.html#abee9ba631807533f90b6a7612191213d",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_ae32e7e941c90d8b04e618e55a82c00f0.html#ae32e7e941c90d8b04e618e55a82c00f0",
"classMayaFlux_1_1Kakshya_1_1TextureContainer_a83f5ce54593c36c5fda22a314b26d28a.html#a83f5ce54593c36c5fda22a314b26d28a",
"classMayaFlux_1_1Kakshya_1_1VideoStreamContainer_a781d2b3f28836b0fddf6cb48ec60e2a9.html#a781d2b3f28836b0fddf6cb48ec60e2a9",
"classMayaFlux_1_1Kakshya_1_1WindowContainer_a36dc887f4398346c40f92bd77b67025a.html#a36dc887f4398346c40f92bd77b67025a",
"classMayaFlux_1_1Kinesis_1_1Stochastic_1_1Stochastic_a7a7d2515539722a278e2779a762e7493.html#a7a7d2515539722a278e2779a762e7493",
"classMayaFlux_1_1Kriya_1_1BufferCapture_ab0cb9f04be4a8aabe8b58f062e0011bd.html#ab0cb9f04be4a8aabe8b58f062e0011bd",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a9879a6e8025b921fcafb50fb1daec0ed.html#a9879a6e8025b921fcafb50fb1daec0ed",
"classMayaFlux_1_1Kriya_1_1SamplingPipeline_aa3f28e1d318ee7bb7fb406cc8e30e867.html#aa3f28e1d318ee7bb7fb406cc8e30e867",
"classMayaFlux_1_1Memory_1_1RingBuffer_ab9b32b60c34fcca16a74cda794064804.html#ab9b32b60c34fcca16a74cda794064804",
"classMayaFlux_1_1Nexus_1_1Emitter_a0bdaa0d7cf820f80bed50f041209ca8c.html#a0bdaa0d7cf820f80bed50f041209ca8c",
"classMayaFlux_1_1Nexus_1_1Fabric_ae5c3d3dc8b8162568adee476cd3663c8.html#ae5c3d3dc8b8162568adee476cd3663c8",
"classMayaFlux_1_1Nexus_1_1Wiring_a0b9d0b1c5a9e2406f743a0c5b99f357e.html#a0b9d0b1c5a9e2406f743a0c5b99f357e",
"classMayaFlux_1_1Nodes_1_1ChainNode_a689bd4fd7b0255488b29249daa42bc42.html#a689bd4fd7b0255488b29249daa42bc42",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_adcfc7b6b2d8e6d1fc28b36beb9cf386a.html#adcfc7b6b2d8e6d1fc28b36beb9cf386a",
"classMayaFlux_1_1Nodes_1_1Generator_1_1LogicContext_a6a774f69d9fd87395ba79b1df7d45f32.html#a6a774f69d9fd87395ba79b1df7d45f32",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a81d25fbe35e6063de2ed5fd7535995c0.html#a81d25fbe35e6063de2ed5fd7535995c0",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Sine_a5cd786be5839d0917206f0d8655e4c05.html#a5cd786be5839d0917206f0d8655e4c05",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1GpuComputeContext_aa1e821a9f5a4aecb4c378cec8b83924a.html#aa1e821a9f5a4aecb4c378cec8b83924a",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointCollectionNode_a649829e953dcb8596ef9ac7398d95539.html#a649829e953dcb8596ef9ac7398d95539",
"classMayaFlux_1_1Nodes_1_1Input_1_1HIDNode_a26e6f0b137b449205797e3e8ea776d70.html#a26e6f0b137b449205797e3e8ea776d70",
"classMayaFlux_1_1Nodes_1_1Network_1_1FieldOperator_ab3ac391cdcbed4424bfa6a3730377168.html#ab3ac391cdcbed4424bfa6a3730377168",
"classMayaFlux_1_1Nodes_1_1Network_1_1MeshNetwork_a9f63923eb2fe52bf1d9dc5bdd7e8b7d4.html#a9f63923eb2fe52bf1d9dc5bdd7e8b7d4",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a16e6c32edc9eb00d6dafb0bd98799570.html#a16e6c32edc9eb00d6dafb0bd98799570",
"classMayaFlux_1_1Nodes_1_1Network_1_1PathOperator_a56493d0ddb01f84282276be3e1a6e024.html#a56493d0ddb01f84282276be3e1a6e024",
"classMayaFlux_1_1Nodes_1_1Network_1_1PointCloudNetwork_a83a07217ff806caf5ae99ab73ab59e52.html#a83a07217ff806caf5ae99ab73ab59e52",
"classMayaFlux_1_1Nodes_1_1Network_1_1WaveguideNetwork_a695ab1dd27c347836afc4cd46463320a.html#a695ab1dd27c347836afc4cd46463320a",
"classMayaFlux_1_1Nodes_1_1Node_a061b8c6d126cfb3d4d4fcc36e0858bf1.html#a061b8c6d126cfb3d4d4fcc36e0858bf1",
"classMayaFlux_1_1Platform_1_1SystemConfig_a89090367164daebee6e2ba7e72ac2321.html#a89090367164daebee6e2ba7e72ac2321",
"classMayaFlux_1_1Portal_1_1Forma_1_1Inspector_a9f2eb4294b19957f8b5487560816479c.html#a9f2eb4294b19957f8b5487560816479c",
"classMayaFlux_1_1Portal_1_1Forma_1_1Plot_1_1Series_ae7a24fc66955e8908f1e9176b6f80534.html#ae7a24fc66955e8908f1e9176b6f80534",
"classMayaFlux_1_1Portal_1_1Graphics_1_1SamplerForge_a98abd9597dc9dce341946786fd85f57b.html#a98abd9597dc9dce341946786fd85f57b",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderSpec_1_1Assemble_a44f5c516ff6ccfe853b704dffbdd1749.html#a44f5c516ff6ccfe853b704dffbdd1749",
"classMayaFlux_1_1Portal_1_1Text_1_1TypeFaceFoundry_a87fdad501996d00cbf447a1f6b25b384.html#a87fdad501996d00cbf447a1f6b25b384",
"classMayaFlux_1_1Vruta_1_1EventSource_ae667164f928e90bfb4d5152a2f5ca9a5.html#ae667164f928e90bfb4d5152a2f5ca9a5",
"classMayaFlux_1_1Vruta_1_1NetworkSource_a7376b04bff9ac1e4ac2766de04bd5d8e.html#a7376b04bff9ac1e4ac2766de04bd5d8e",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a99bd0b1b8f51d07e40aeaf1588342323.html#a99bd0b1b8f51d07e40aeaf1588342323",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_a7137d950b4f56144ca6dbb382851be50.html#a7137d950b4f56144ca6dbb382851be50",
"classMayaFlux_1_1Yantra_1_1FeatureExtractor_a7960c4d7413b7b785e3ea8e766eef8b0.html#a7960c4d7413b7b785e3ea8e766eef8b0",
"classMayaFlux_1_1Yantra_1_1GpuExtractor_a748c25318cf737e1de1da89871a903d2.html#a748c25318cf737e1de1da89871a903d2",
"classMayaFlux_1_1Yantra_1_1OperationHelper_aea16826ac411fbe8e5c913dbb07722ab.html#aea16826ac411fbe8e5c913dbb07722ab",
"classMayaFlux_1_1Yantra_1_1StatisticalAnalyzer_a4119cdaf579cc2c277c76047f6c8c794.html#a4119cdaf579cc2c277c76047f6c8c794",
"classMayaFlux_1_1Yantra_1_1UniversalExtractor_a1e05b2c99c977c84bab6d9c93730c3ca.html#a1e05b2c99c977c84bab6d9c93730c3ca",
"classMayaFlux_1_1Yantra_1_1VisionAnalyzer_a0857f4a6a86b03e3cc1e498714de8346.html#a0857f4a6a86b03e3cc1e498714de8346",
"functions_~.html",
"namespaceMayaFlux_1_1Core_a1cc4eb2f730088585e5b9622afd28a40.html#a1cc4eb2f730088585e5b9622afd28a40a270f3a25388fdd9f98c49f4fc0a12dcd",
"namespaceMayaFlux_1_1IO_a582f5249ec35cc284ce9e293ef33d9ce.html#a582f5249ec35cc284ce9e293ef33d9ceae7ec6f112fa4b5a58e82580ff159655c",
"namespaceMayaFlux_1_1Kakshya_a9009f3c353433a7e65c7d7ee13996f02.html#a9009f3c353433a7e65c7d7ee13996f02a921a0157a6e61eebbaa0f713fdfbb0f7",
"namespaceMayaFlux_1_1Kinesis_1_1Vision_a19cacf7c8b73b5fe8a8535342e4b0d86.html#a19cacf7c8b73b5fe8a8535342e4b0d86",
"namespaceMayaFlux_1_1Kinesis_ac32839d2593eb11735b40cd2829b72a3.html#ac32839d2593eb11735b40cd2829b72a3",
"namespaceMayaFlux_1_1Nodes_a59d09b556b30502af09a3682481b8560.html#a59d09b556b30502af09a3682481b8560",
"namespaceMayaFlux_1_1Portal_1_1Graphics_aedac3269aff6764f635d0a0d96fa7e92.html#aedac3269aff6764f635d0a0d96fa7e92a76cfc54de4e2b9cde17334c821217d11",
"namespaceMayaFlux_1_1Yantra_a16dce90274df3c813ae40ca985e2da7f.html#a16dce90274df3c813ae40ca985e2da7f",
"namespaceMayaFlux_1_1Yantra_af03c3714714287e5fb21aa133b375f3a.html#af03c3714714287e5fb21aa133b375f3a",
"namespaceMayaFlux_aa01b246fe947237c4ecc3b71d3984df9.html#aa01b246fe947237c4ecc3b71d3984df9",
"structMayaFlux_1_1Buffers_1_1AggregateBindingsProcessor_1_1AggregateBinding_aead00919760ac87788669d2c6da94891.html#aead00919760ac87788669d2c6da94891",
"structMayaFlux_1_1Buffers_1_1RootAudioUnit_a85e08cbcfef929fa4166ebc7f8198d48.html#a85e08cbcfef929fa4166ebc7f8198d48",
"structMayaFlux_1_1Core_1_1CaptureState_a356fa3b5d626e1941e512678ce70e2c4.html#a356fa3b5d626e1941e512678ce70e2c4",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_a7cbcd312755179ca5797c0c0534647ce.html#a7cbcd312755179ca5797c0c0534647cea5f90af42814c0a419d715d43ae54fd7a",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_af92983df63d558b64b363bb19c962594.html#af92983df63d558b64b363bb19c962594",
"structMayaFlux_1_1Core_1_1InputManager_1_1NodeRegistration_a8521ac83d900600fc3b791aa4a92fa57.html#a8521ac83d900600fc3b791aa4a92fa57",
"structMayaFlux_1_1Core_1_1ShaderReflection_1_1PushConstantRange_a684ea25e8ebe83cc87dcafb99839ece4.html#a684ea25e8ebe83cc87dcafb99839ece4",
"structMayaFlux_1_1Core_1_1VertexInputInfo_1_1Binding_a099b83392ecdfa8a8b52e20ae011dd86.html#a099b83392ecdfa8a8b52e20ae011dd86",
"structMayaFlux_1_1IO_1_1FileRegion_a3fb8b947a3f73d049a441f7e68825d3e.html#a3fb8b947a3f73d049a441f7e68825d3e",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure_adbc9165c3ecf537bad852e00a3c489ba.html#adbc9165c3ecf537bad852e00a3c489ba",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a30dd98fff113df9afea9814356938f0d.html#a30dd98fff113df9afea9814356938f0d",
"structMayaFlux_1_1Kakshya_1_1StreamSlice_a1b31b6648933665b1537d04bac1707b4.html#a1b31b6648933665b1537d04bac1707b4",
"structMayaFlux_1_1Kinesis_1_1FlyKeyMap_abedb1624bf87af435f09f0971e834e2f.html#abedb1624bf87af435f09f0971e834e2f",
"structMayaFlux_1_1Kinesis_1_1SpatialSnapshot_a0a80e0d8fb1b76ca99f4c3c7cf165ab1.html#a0a80e0d8fb1b76ca99f4c3c7cf165ab1",
"structMayaFlux_1_1Kinesis_1_1detail_1_1PointTraits_3_01glm_1_1vec3_01_4_a870d3c9bbf6d644d9e6f4d980dd1da39.html#a870d3c9bbf6d644d9e6f4d980dd1da39",
"structMayaFlux_1_1Nexus_1_1Fabric_1_1Registration_a4292d888e6f2b27c4a3edc37199d6a58.html#a4292d888e6f2b27c4a3edc37199d6a58",
"structMayaFlux_1_1Nexus_1_1State_1_1WiringRecord_af397f55b23a17b3b3fbb020a8342c1dd.html#af397f55b23a17b3b3fbb020a8342c1dd",
"structMayaFlux_1_1Nodes_1_1Network_1_1MeshSlot_ae294ab65701cd9b34f2b5f43da0eb4c0.html#ae294ab65701cd9b34f2b5f43da0eb4c0",
"structMayaFlux_1_1Portal_1_1Forma_1_1Element_a334fd05fe13e932fea1f77b1ddd047df.html#a334fd05fe13e932fea1f77b1ddd047df",
"structMayaFlux_1_1Portal_1_1Forma_1_1ValueGroup_a1fac51e34bd5021ee9d629ffdf22f472.html#a1fac51e34bd5021ee9d629ffdf22f472",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_1_1CommandBufferState_ae8eeede01a7b2ba62d490d29ba40a115.html#ae8eeede01a7b2ba62d490d29ba40a115",
"structMayaFlux_1_1Registry_1_1Service_1_1BufferService_ae0a0ab239dc8dbf4b45b5e8881eb869b.html#ae0a0ab239dc8dbf4b45b5e8881eb869b",
"structMayaFlux_1_1Yantra_1_1ChannelStatistics_a9f682dddaa86cb0ae5ad416a01464ca8.html#a9f682dddaa86cb0ae5ad416a01464ca8",
"structMayaFlux_1_1Yantra_1_1Granular_1_1GranularConfig_a5c7c61da4b07c0ed0b295d5a47d0c17d.html#a5c7c61da4b07c0ed0b295d5a47d0c17d",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01std_1_1vector_3_01Kakshya_1_1RegionSegment_01_4_01_4_a3518f1f0b5b3c3fa8750f2fd7ffb9630.html#a3518f1f0b5b3c3fa8750f2fd7ffb9630"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';