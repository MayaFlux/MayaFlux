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
    [ "MayaFlux - Getting Started Guide", "index.html", "index" ],
    [ "MayaFlux", "md_README.html", [
      [ "The Architecture", "md_README.html#autotoc_md70", null ],
      [ "Processing Model", "md_README.html#autotoc_md72", null ],
      [ "In Practice", "md_README.html#autotoc_md74", null ],
      [ "In Practice: Rhythm as Topology", "md_README.html#autotoc_md75", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md77", [
        [ "Nodes", "md_README.html#autotoc_md78", null ],
        [ "Buffers", "md_README.html#autotoc_md79", null ],
        [ "IO and Containers", "md_README.html#autotoc_md80", null ],
        [ "Graphics", "md_README.html#autotoc_md81", null ],
        [ "Portal::Text", "md_README.html#autotoc_md82", null ],
        [ "Coroutines", "md_README.html#autotoc_md83", null ],
        [ "Networking", "md_README.html#autotoc_md84", null ],
        [ "Nexus", "md_README.html#autotoc_md85", null ],
        [ "Yantra (Offline Compute)", "md_README.html#autotoc_md86", null ],
        [ "Lila (Live Coding)", "md_README.html#autotoc_md87", null ],
        [ "Input", "md_README.html#autotoc_md88", null ],
        [ "Kinesis", "md_README.html#autotoc_md89", null ]
      ] ],
      [ "Quick Start (Projects) — Weave", "md_README.html#autotoc_md91", [
        [ "Management Mode", "md_README.html#autotoc_md92", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md93", null ]
      ] ],
      [ "Quick Start (Developer)", "md_README.html#autotoc_md95", [
        [ "Requirements", "md_README.html#autotoc_md96", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md97", null ],
        [ "Build", "md_README.html#autotoc_md98", null ]
      ] ],
      [ "Releases and Builds", "md_README.html#autotoc_md100", [
        [ "Stable Releases", "md_README.html#autotoc_md101", null ],
        [ "Development Builds", "md_README.html#autotoc_md102", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md104", [
        [ "Basic Application Structure", "md_README.html#autotoc_md105", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md107", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md109", [
        [ "Tutorials", "md_README.html#autotoc_md110", null ],
        [ "API Documentation", "md_README.html#autotoc_md111", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md113", null ],
      [ "Philosophy", "md_README.html#autotoc_md115", null ],
      [ "For Researchers and Developers", "md_README.html#autotoc_md117", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md119", [
        [ "Phase 1 (Complete)", "md_README.html#autotoc_md120", null ],
        [ "Phase 2 (Current, Q1 2026)", "md_README.html#autotoc_md121", null ],
        [ "Phase 3 (Complete)", "md_README.html#autotoc_md122", null ],
        [ "Phase 4 (0.4)", "md_README.html#autotoc_md123", null ],
        [ "Phase 5 (0.5)", "md_README.html#autotoc_md124", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md126", null ],
      [ "Contributing", "md_README.html#autotoc_md128", null ],
      [ "Authorship and Ethics", "md_README.html#autotoc_md130", null ],
      [ "Contact", "md_README.html#autotoc_md132", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md137", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md139", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md141", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md144", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md146", null ],
      [ "🧠 AI-Assisted Contributions", "md_CONTRIBUTING.html#autotoc_md148", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md150", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md152", null ]
    ] ],
    [ "MayaFlux: The Computational Substrate", "md_docs_2Digital__Architecture.html", [
      [ "Everything Is Numbers", "md_docs_2Digital__Architecture.html#autotoc_md200", null ],
      [ "The Real-Time Core: Nodes, Buffers, Vruta, Kriya", "md_docs_2Digital__Architecture.html#autotoc_md202", [
        [ "Nodes", "md_docs_2Digital__Architecture.html#autotoc_md203", null ],
        [ "Buffers", "md_docs_2Digital__Architecture.html#autotoc_md204", null ],
        [ "Processors", "md_docs_2Digital__Architecture.html#autotoc_md205", null ],
        [ "Vruta", "md_docs_2Digital__Architecture.html#autotoc_md206", null ],
        [ "Kriya", "md_docs_2Digital__Architecture.html#autotoc_md207", null ]
      ] ],
      [ "Kakshya: The Data Layer", "md_docs_2Digital__Architecture.html#autotoc_md209", null ],
      [ "Yantra: The Offline Computation Universe", "md_docs_2Digital__Architecture.html#autotoc_md211", null ],
      [ "The Visual Pipeline", "md_docs_2Digital__Architecture.html#autotoc_md213", [
        [ "Portal", "md_docs_2Digital__Architecture.html#autotoc_md214", null ],
        [ "Registry", "md_docs_2Digital__Architecture.html#autotoc_md215", null ],
        [ "Core and Subsystems", "md_docs_2Digital__Architecture.html#autotoc_md216", null ]
      ] ],
      [ "Nexus: Spatial Entity Simulation", "md_docs_2Digital__Architecture.html#autotoc_md218", null ],
      [ "Kinesis: The Mathematical Substrate", "md_docs_2Digital__Architecture.html#autotoc_md220", null ],
      [ "IO: Reading the World", "md_docs_2Digital__Architecture.html#autotoc_md222", null ],
      [ "Transitive: Framework-Independent Utilities", "md_docs_2Digital__Architecture.html#autotoc_md224", null ],
      [ "Lila: The Live Layer", "md_docs_2Digital__Architecture.html#autotoc_md226", null ],
      [ "Tokens and Domain", "md_docs_2Digital__Architecture.html#autotoc_md228", null ],
      [ "How the System Composes", "md_docs_2Digital__Architecture.html#autotoc_md230", null ]
    ] ],
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
"BufferTokenDistributor_8cpp_source.html",
"Config_8hpp.html#ad8c6968db02415b5ba48863063076989",
"DataUtils_8hpp.html#af9f779ced39b9697f27c44e6f3bfa3e9",
"FieldOperator_8hpp.html#a72e839ba9a1d28a78731ca7637ee3fa8a04bd834032febb3fda8c6936ee140949",
"GlyphAtlas_8hpp_source.html",
"GraphicsUtils_8hpp.html#a9fa38a89c88d0526a7773ffcd5ade928ad28539b874a2bc4c5d40f49b375af474",
"JournalEntry_8hpp.html#a4d1f572381818d4f67c4a5294d68ef2da316b358495dc9d6efa66b7c7118f83ca",
"LiveArena_8cpp_source.html",
"NDData_8hpp.html#a6858a30e8f410b5f7808389074aa296fab5caca4ee419b9d90bf910b6334ed6f0",
"NodeUtils_8hpp.html#abfb2eeef9f8a8fcd2b9118d2e8bbeddb",
"RegionUtils_8cpp.html#ac6eb03f7c3dd211b03db4bce4cabfa67",
"Sort_8cpp.html",
"StateEncoder_8hpp_source.html",
"TextureLoom_8hpp.html#a335ffb0c43b53ec263ab6559ac29476b",
"VKEnumUtils_8hpp.html#a536f388ac516606e0df343ebf0047db6",
"Yantra_8cpp.html#a3b8f6b534c503df4c4a2b106a64a65fe",
"classLila_1_1Commentator_ab779bc9939bf3e7474aad9d5b87c17d2.html#ab779bc9939bf3e7474aad9d5b87c17d2",
"classMayaFlux_1_1Buffers_1_1AudioBuffer_a1cacb0395f79b9d80f2fd2de2aec81d0.html#a1cacb0395f79b9d80f2fd2de2aec81d0",
"classMayaFlux_1_1Buffers_1_1BufferManager_a2ee00b4bbc19109306e37b3690c85c1b.html#a2ee00b4bbc19109306e37b3690c85c1b",
"classMayaFlux_1_1Buffers_1_1BufferProcessor.html",
"classMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_a1600fb2ddd56bb5d07612cc743b2b141.html#a1600fb2ddd56bb5d07612cc743b2b141",
"classMayaFlux_1_1Buffers_1_1FormaProcessor.html",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_ab714600ca552f2e1d99f661be7cf38a7.html#ab714600ca552f2e1d99f661be7cf38a7",
"classMayaFlux_1_1Buffers_1_1NetworkAudioProcessor_a39315b6e0dd05a7296c79158cd8cbbd7.html#a39315b6e0dd05a7296c79158cd8cbbd7",
"classMayaFlux_1_1Buffers_1_1PolynomialProcessor_a1bf52c05daa489ac916bea50adffc379.html#a1bf52c05daa489ac916bea50adffc379",
"classMayaFlux_1_1Buffers_1_1RootBuffer_a656d1e97a244e15badb58075d80fc919.html#a656d1e97a244e15badb58075d80fc919",
"classMayaFlux_1_1Buffers_1_1SoundStreamReader_a0683c364c55702a4fb9a1f48ef639937.html#a0683c364c55702a4fb9a1f48ef639937",
"classMayaFlux_1_1Buffers_1_1TextureBuffer_ad4dfd30af6ecb53741917052bf839ccb.html#ad4dfd30af6ecb53741917052bf839ccb",
"classMayaFlux_1_1Buffers_1_1VKBuffer_a247de60c8748d57118d712f12f6b86cc.html#a247de60c8748d57118d712f12f6b86cc",
"classMayaFlux_1_1Core_1_1AudioDevice_a0304da38d44c00707c57d4af2e241a4b.html#a0304da38d44c00707c57d4af2e241a4b",
"classMayaFlux_1_1Core_1_1BufferProcessingHandle_a1524ec9305e8f65c9f9b5982daaaf830.html#a1524ec9305e8f65c9f9b5982daaaf830",
"classMayaFlux_1_1Core_1_1GlfwWindow_a4d1c8e66f3297ac6f5b582112ec3fabd.html#a4d1c8e66f3297ac6f5b582112ec3fabd",
"classMayaFlux_1_1Core_1_1IAudioBackend.html",
"classMayaFlux_1_1Core_1_1InputSubsystem_a0ff2bfec03ea6df781132047d818ecf9.html#a0ff2bfec03ea6df781132047d818ecf9",
"classMayaFlux_1_1Core_1_1NetworkSubsystem_ad24e3a894e47bdeaeb597f41ec54bd23.html#ad24e3a894e47bdeaeb597f41ec54bd23",
"classMayaFlux_1_1Core_1_1TCPBackend_a5a7c58ced329ff4baa4c19873f189e65.html#a5a7c58ced329ff4baa4c19873f189e65",
"classMayaFlux_1_1Core_1_1VKComputePipeline_addb8f28f7088a538046c0350dca957a8.html#addb8f28f7088a538046c0350dca957a8",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a61aab59444f4b41db8e1447d57c066c3.html#a61aab59444f4b41db8e1447d57c066c3",
"classMayaFlux_1_1Core_1_1VKShaderModule_a2ddceda97cc3410a6ff63b5d5356264b.html#a2ddceda97cc3410a6ff63b5d5356264b",
"classMayaFlux_1_1Core_1_1WindowManager_a686f2d909f8efc8fcee6cf6f3619bbde.html#a686f2d909f8efc8fcee6cf6f3619bbde",
"classMayaFlux_1_1IO_1_1CameraReader_a25ec3322a9e680a703b83504d18b3b2f.html#a25ec3322a9e680a703b83504d18b3b2f",
"classMayaFlux_1_1IO_1_1IOManager_a4400ee4a8190b5d6f87d5265e0ff65cf.html#a4400ee4a8190b5d6f87d5265e0ff65cf",
"classMayaFlux_1_1IO_1_1ModelReader_ab67380f254552fe59218900f9f565d13.html#ab67380f254552fe59218900f9f565d13",
"classMayaFlux_1_1IO_1_1VideoFileReader_a62a80833658fcfb3bbfca5c5e6fe6d0f.html#a62a80833658fcfb3bbfca5c5e6fe6d0f",
"classMayaFlux_1_1Journal_1_1Archivist_a93ea2b6e9e626426403495ed7967ea20.html#a93ea2b6e9e626426403495ed7967ea20",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_a6d07166944f6b34beaa9d13e203a6660.html#a6d07166944f6b34beaa9d13e203a6660",
"classMayaFlux_1_1Kakshya_1_1FrameAccessProcessor_a83314ea7b9a628b38ebb8fd4644b878e.html#a83314ea7b9a628b38ebb8fd4644b878e",
"classMayaFlux_1_1Kakshya_1_1RegionProcessorBase_aa2c85ad36fcb99f15eb82647474857fa.html#aa2c85ad36fcb99f15eb82647474857fa",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_ab749d02374d2c8f66394fc57586b306f.html#ab749d02374d2c8f66394fc57586b306f",
"classMayaFlux_1_1Kakshya_1_1TextureContainer_a4d38e20b150032ef015bb2543e36ca03.html#a4d38e20b150032ef015bb2543e36ca03",
"classMayaFlux_1_1Kakshya_1_1VideoStreamContainer_a571e47ad3e6ea4f13f5ed071bc0a437c.html#a571e47ad3e6ea4f13f5ed071bc0a437c",
"classMayaFlux_1_1Kakshya_1_1WindowContainer_a61ffbda351f7b1370eea19feadf8caaa.html#a61ffbda351f7b1370eea19feadf8caaa",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a12be9fb6e0e03fc7ee5ca589fba99a51.html#a12be9fb6e0e03fc7ee5ca589fba99a51",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a1d31280361c0a444800a91d501486a96.html#a1d31280361c0a444800a91d501486a96",
"classMayaFlux_1_1Kriya_1_1NetworkAwaiter_a7705176f5fb44dd91f228da443d1f48c.html#a7705176f5fb44dd91f228da443d1f48c",
"classMayaFlux_1_1Memory_1_1RingBuffer_a6193cc43e1066b55f01756bb00cec07f.html#a6193cc43e1066b55f01756bb00cec07f",
"classMayaFlux_1_1Nexus_1_1Emitter_ac1aff71ea426c0f0ac41ce769ff20857.html#ac1aff71ea426c0f0ac41ce769ff20857",
"classMayaFlux_1_1Nexus_1_1Wiring_a00438b0e94db32093622c3e2f6f9f579.html#a00438b0e94db32093622c3e2f6f9f579",
"classMayaFlux_1_1Nodes_1_1ChainNode_aca2624684c8d722f5acf0bf44e853c8b.html#aca2624684c8d722f5acf0bf44e853c8b",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_af73e68a1e2bef4ab6430cdaff21b14f8.html#af73e68a1e2bef4ab6430cdaff21b14f8",
"classMayaFlux_1_1Nodes_1_1Generator_1_1LogicContext_a9818a3fcfa1407c25c8edc0d645ee103.html#a9818a3fcfa1407c25c8edc0d645ee103",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a86c6884b2837fc170125759c68f733fe.html#a86c6884b2837fc170125759c68f733fe",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Sine_a57ab010217fda6cc0085477a9629be94.html#a57ab010217fda6cc0085477a9629be94",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1MeshWriterNode_a9366906caa590bf324c92be99b1f0cbe.html#a9366906caa590bf324c92be99b1f0cbe",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1TextureNode_ae687c0846f3918796c8e67244b833ec8.html#ae687c0846f3918796c8e67244b833ec8",
"classMayaFlux_1_1Nodes_1_1Input_1_1MIDINode_a7d7d17e6d86f5ca318b5558f04ded8a9.html#a7d7d17e6d86f5ca318b5558f04ded8a9",
"classMayaFlux_1_1Nodes_1_1Network_1_1MeshNetwork_aca6b2a01a5e6233750b953728ee50684.html#aca6b2a01a5e6233750b953728ee50684",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a2f4b5e97cd86132543960434e403313b.html#a2f4b5e97cd86132543960434e403313b",
"classMayaFlux_1_1Nodes_1_1Network_1_1PathOperator_ab6802732193853af083ed30baca17b86.html#ab6802732193853af083ed30baca17b86",
"classMayaFlux_1_1Nodes_1_1Network_1_1ResonatorNetwork_a07959e7594f736a2bc6da735f88267d5.html#a07959e7594f736a2bc6da735f88267d5",
"classMayaFlux_1_1Nodes_1_1Network_1_1WaveguideNetwork_ab54de2dc884379dab027f82b8d0fbd95.html#ab54de2dc884379dab027f82b8d0fbd95",
"classMayaFlux_1_1Nodes_1_1Node_a609c100f1425dc2587c17e8527ce4ac1.html#a609c100f1425dc2587c17e8527ce4ac1",
"classMayaFlux_1_1Portal_1_1Forma_1_1Bridge_aadb281b88d67a45aae2377bc344ee500.html#aadb281b88d67a45aae2377bc344ee500",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a8cf0ad50e661b9ff2f6b51febb5a7cf7.html#a8cf0ad50e661b9ff2f6b51febb5a7cf7",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_ac2a823c7787484ea47fe26650c632202.html#ac2a823c7787484ea47fe26650c632202",
"classMayaFlux_1_1Portal_1_1Text_1_1TypeFaceFoundry_a33297f10cf0efb8394bd219ba56f0f12.html#a33297f10cf0efb8394bd219ba56f0f12",
"classMayaFlux_1_1Vruta_1_1Event_a93a2770c4987cd71c6691223f66bc42e.html#a93a2770c4987cd71c6691223f66bc42e",
"classMayaFlux_1_1Vruta_1_1Routine_ae337f35ef930eea0053e94fc76867328.html#ae337f35ef930eea0053e94fc76867328",
"classMayaFlux_1_1Yantra_1_1ComputationPipeline.html",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer.html",
"classMayaFlux_1_1Yantra_1_1GpuDispatchCore_a8f1830a9946ac5ab0568192027de1d1b.html#a8f1830a9946ac5ab0568192027de1d1b",
"classMayaFlux_1_1Yantra_1_1MathematicalTransformer_a6e33d648c252e4cff7c42a3d50779efb.html#a6e33d648c252e4cff7c42a3d50779efb",
"classMayaFlux_1_1Yantra_1_1StandardSorter_a387fd994e714a76dddfd84a8f8deae19.html#a387fd994e714a76dddfd84a8f8deae19",
"classMayaFlux_1_1Yantra_1_1UniversalAnalyzer_ad747f8e8b4d0893c2b72190715a254ca.html#ad747f8e8b4d0893c2b72190715a254ca",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_ace1dfab4ce0f281e9da5dec81d724b18.html#ace1dfab4ce0f281e9da5dec81d724b18",
"lila__server_8cpp_a3b527c56ed133ee6815bfbc625e757af.html#a3b527c56ed133ee6815bfbc625e757af",
"namespaceMayaFlux_1_1Core_a4d48a7abfbddf63004ecb33fed2a8220.html#a4d48a7abfbddf63004ecb33fed2a8220ab136ef5f6a01d816991fe3cf7a6ac763",
"namespaceMayaFlux_1_1Journal_1_1AnsiColors_afd227bc613a8e72931e592c8bfed876b.html#afd227bc613a8e72931e592c8bfed876b",
"namespaceMayaFlux_1_1Kakshya_ad322e2da4b0bd3c2aacd5935dfa0cd39.html#ad322e2da4b0bd3c2aacd5935dfa0cd39",
"namespaceMayaFlux_1_1Kinesis_a7a56c8a0b053317eb839d9d20eba2ada.html#a7a56c8a0b053317eb839d9d20eba2adaa3c6039d0bdd8b0adad731b7dca62637f",
"namespaceMayaFlux_1_1Nodes_a1d792a9c7c4adccf80fff01afb356cb7.html#a1d792a9c7c4adccf80fff01afb356cb7",
"namespaceMayaFlux_1_1Vruta_a6c049928f3eac462518262bb646f6920.html#a6c049928f3eac462518262bb646f6920",
"namespaceMayaFlux_1_1Yantra_aa2ae6e7df3e7cf6052ca578c2e0e43e3.html#aa2ae6e7df3e7cf6052ca578c2e0e43e3a493aeb39c64681a35674f8f148bc7ab5",
"namespaceMayaFlux_a6026d46d032b38446b8c86b358b637cb.html#a6026d46d032b38446b8c86b358b637cb",
"structDescriptorBindingConfig.html",
"structMayaFlux_1_1Buffers_1_1NetworkGpuData_a9560c198effa692c8c72585458caa090.html#a9560c198effa692c8c72585458caa090",
"structMayaFlux_1_1Core_1_1AttachmentDescription_a21f467a65690e1e0ad634fd313d580e1.html#a21f467a65690e1e0ad634fd313d580e1",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_a1edf7b0cdab0d0e1233dcb3f49a9220d.html#a1edf7b0cdab0d0e1233dcb3f49a9220d",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_aa58df8b79e12dee41a20a28c1b018488.html#aa58df8b79e12dee41a20a28c1b018488",
"structMayaFlux_1_1Core_1_1InputDeviceInfo_a76a77100567121c3cdc0e078942aabb4.html#a76a77100567121c3cdc0e078942aabb4",
"structMayaFlux_1_1Core_1_1SerialBackendInfo_a556608630acff4153cfba987c211842f.html#a556608630acff4153cfba987c211842f",
"structMayaFlux_1_1Core_1_1VertexBinding_a2cd0f8f663d51bbc9ebd1760fd226539.html#a2cd0f8f663d51bbc9ebd1760fd226539",
"structMayaFlux_1_1IO_1_1CameraConfig_af82e8abbb21a5f764a0605cd113e51f1.html#af82e8abbb21a5f764a0605cd113e51f1",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure_ac98d4004b47256d8372e14303610713a.html#ac98d4004b47256d8372e14303610713a",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_ac6787205a70d759a66edd2735f2f7f7b.html#ac6787205a70d759a66edd2735f2f7f7b",
"structMayaFlux_1_1Kakshya_1_1VertexAccess.html",
"structMayaFlux_1_1Kinesis_1_1Tendency_a80f204b7875ed4581447bd86d9c947db.html#a80f204b7875ed4581447bd86d9c947db",
"structMayaFlux_1_1Nexus_1_1InfluenceUBO.html",
"structMayaFlux_1_1Nodes_1_1Network_1_1MeshSlot_a28504fc805b90420804bd79a86648c00.html#a28504fc805b90420804bd79a86648c00",
"structMayaFlux_1_1Portal_1_1Forma_1_1Mapped_aa67cad8f2fdc8d0dd5302a729b719e88.html#aa67cad8f2fdc8d0dd5302a729b719e88",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderReflectionInfo_a5d42e623165bec4c6e6c3617dd4c19fb.html#a5d42e623165bec4c6e6c3617dd4c19fb",
"structMayaFlux_1_1Vruta_1_1complex__promise_a5ebe3a9581e39cbdfed7cfb88b6ee1b8.html#a5ebe3a9581e39cbdfed7cfb88b6ee1b8",
"structMayaFlux_1_1Yantra_1_1ExecutionContext_ac0ce43fd6f5cb159807d455150ef0174.html#ac0ce43fd6f5cb159807d455150ef0174",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01std_1_1vector_3_01Kakshya_1_1DataVariant_01_4_01_4.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';