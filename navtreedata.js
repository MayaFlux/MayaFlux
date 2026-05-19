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
"BufferPipeline_8hpp_source.html",
"ComputeProcessor_8hpp.html",
"DataUtils_8cpp.html#a6afb7a0175ac8a1a38a4b4e74aa4fcb5",
"FFmpegDemuxContext_8hpp.html",
"GlobalGraphicsInfo_8hpp.html#a1cc4eb2f730088585e5b9622afd28a40a5c58478534ac36238f73c77d512498de",
"GraphicsUtils_8hpp.html#a3ac47112cb6b1975ec9bf6ed54707cd9aca0e438cddda63061a0149241dc23ad1",
"Input_8cpp.html#a325ef8e37e67d238bb2b8a6753610a53",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62ae1e1d3d40573127e9ee0480caf1283d6",
"MotionCurves_8cpp.html#ad75c60659787b7670802a72a799a31f2",
"NodeSpec_8hpp.html#a59e5aa087f3b567dd9915652c920e3d1a83b6e0a9092d85fd541dbbb34aee44dc",
"ProcessingTokens_8hpp.html#af6d1b053535f945d3e5fec41c8f1fafaaa1c360626270cbbdc9c9a21a16e8df48",
"RtMidiBackend_8cpp.html",
"StagingUtils_8cpp.html#a31f5983628d43dc3c83257330c321cc6",
"Tapestry_8cpp_source.html",
"UniversalAnalyzer_8hpp_source.html",
"ViewTransform_8hpp.html#aa2978769071adc9bab471092b31098d0",
"Yantra_8hpp.html#a83fdd952f258340a4ccab4da0f23cdc6",
"classLila_1_1Server_a56e777304d393ed5e64c70a9a238c640.html#a56e777304d393ed5e64c70a9a238c640",
"classMayaFlux_1_1Buffers_1_1AudioWriteProcessor_af4fa11178fcfa4df026d5377168b9a8b.html#af4fa11178fcfa4df026d5377168b9a8b",
"classMayaFlux_1_1Buffers_1_1BufferProcessingChain_a1b98bf6c28cf994a1cefad945b80817c.html#a1b98bf6c28cf994a1cefad945b80817c",
"classMayaFlux_1_1Buffers_1_1BufferUploadProcessor_af001eed23ab0c7a22124352e93ec6534.html#af001eed23ab0c7a22124352e93ec6534",
"classMayaFlux_1_1Buffers_1_1FeedbackProcessor_a2a158aaf240a50d60125aa6620d2e0e6.html#a2a158aaf240a50d60125aa6620d2e0e6",
"classMayaFlux_1_1Buffers_1_1GeometryWriteProcessor_a759d030f624ad02469fc9663e98f223d.html#a759d030f624ad02469fc9663e98f223d",
"classMayaFlux_1_1Buffers_1_1MeshNetworkBuffer_a91e13e2a0ef4bf044279b4595c716538.html#a91e13e2a0ef4bf044279b4595c716538",
"classMayaFlux_1_1Buffers_1_1NodeBindingsProcessor_ad1cdc408cecf642a54335eae32c14c03.html#ad1cdc408cecf642a54335eae32c14c03",
"classMayaFlux_1_1Buffers_1_1RenderProcessor_a1dee5374d652a97f471cb0ac0a1cddf0.html#a1dee5374d652a97f471cb0ac0a1cddf0",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a4519f49998bf5b8efa3257d9b1d37240.html#a4519f49998bf5b8efa3257d9b1d37240",
"classMayaFlux_1_1Buffers_1_1StreamSliceProcessor_aeb52c4588f37dd76af5a2e8cb3722717.html#aeb52c4588f37dd76af5a2e8cb3722717",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_a7673914495c7a5a49599cbcc48f727fe.html#a7673914495c7a5a49599cbcc48f727fe",
"classMayaFlux_1_1Buffers_1_1VKBuffer_a954e31fe2ae3bc1e240d7be142928be7.html#a954e31fe2ae3bc1e240d7be142928be7",
"classMayaFlux_1_1Core_1_1AudioSubsystem_acef12bf5810421fbccefdfedb05aff97.html#acef12bf5810421fbccefdfedb05aff97",
"classMayaFlux_1_1Core_1_1Engine_a20e8caea129c9ebda9f8bfe1b4a58ada.html#a20e8caea129c9ebda9f8bfe1b4a58ada",
"classMayaFlux_1_1Core_1_1GraphicsSubsystem_a36ed72177c582bda856511f7f3ffbd9b.html#a36ed72177c582bda856511f7f3ffbd9b",
"classMayaFlux_1_1Core_1_1IInputBackend_ab2dd3c885f96ec0ad5a5b764252bc35a.html#ab2dd3c885f96ec0ad5a5b764252bc35a",
"classMayaFlux_1_1Core_1_1InputSubsystem_af377fd09383ae048e0d90990a6de9a37.html#af377fd09383ae048e0d90990a6de9a37",
"classMayaFlux_1_1Core_1_1SubsystemManager_a853a093ef4e7c5b11eff549ca074d78a.html#a853a093ef4e7c5b11eff549ca074d78a",
"classMayaFlux_1_1Core_1_1UDPBackend_ae768818c0b99e3005af3a966b68596b6.html#ae768818c0b99e3005af3a966b68596b6",
"classMayaFlux_1_1Core_1_1VKDevice_a5abd7b4d06beff39a43925ece5e80d14.html#a5abd7b4d06beff39a43925ece5e80d14",
"classMayaFlux_1_1Core_1_1VKImage_aa2892c5fa7008bb1cab0eb8176e42c31.html#aa2892c5fa7008bb1cab0eb8176e42c31",
"classMayaFlux_1_1Core_1_1VKSwapchain_aeb335a05380d79074a40890d75e06115.html#aeb335a05380d79074a40890d75e06115",
"classMayaFlux_1_1Creator_a4c6304b9c65e93897f1022951537811e.html#a4c6304b9c65e93897f1022951537811e",
"classMayaFlux_1_1IO_1_1FFmpegDemuxContext_afd51276113e9020a482519185ea7d924.html#afd51276113e9020a482519185ea7d924",
"classMayaFlux_1_1IO_1_1ImageReader_aa46085c16a81be449491d3fb516607ae.html#aa46085c16a81be449491d3fb516607ae",
"classMayaFlux_1_1IO_1_1SoundFileReader_ac1ac8fc80581c39b32b8dbd1414128f8.html#ac1ac8fc80581c39b32b8dbd1414128f8",
"classMayaFlux_1_1IO_1_1VideoStreamContext_a489f77b8c0c775d1181201868168fe8f.html#a489f77b8c0c775d1181201868168fe8f",
"classMayaFlux_1_1Kakshya_1_1CameraContainer_a3610a872353dfbe6e8fea1d57c3214d5.html#a3610a872353dfbe6e8fea1d57c3214d5",
"classMayaFlux_1_1Kakshya_1_1DynamicRegionProcessor_a633c723721d5942aa48ed7357a2605d1.html#a633c723721d5942aa48ed7357a2605d1",
"classMayaFlux_1_1Kakshya_1_1MeshInsertion_ad48044437dbec2de2c6efbb313e7edb9.html#ad48044437dbec2de2c6efbb313e7edb9",
"classMayaFlux_1_1Kakshya_1_1PlotProcessor_a033ac0511484de59a7e0ed3a2d941543.html#a033ac0511484de59a7e0ed3a2d941543",
"classMayaFlux_1_1Kakshya_1_1SoundFileContainer_a80514881c59f60cddca5477c691486c9.html#a80514881c59f60cddca5477c691486c9",
"classMayaFlux_1_1Kakshya_1_1SpatialRegionProcessor_ac7cd330628a0e752c1de8ab09e34d4f8.html#ac7cd330628a0e752c1de8ab09e34d4f8",
"classMayaFlux_1_1Kakshya_1_1TextureContainer_abba98603ff69eff4a84bc83b73968321.html#abba98603ff69eff4a84bc83b73968321",
"classMayaFlux_1_1Kakshya_1_1VideoStreamContainer_ab8ec896cb3461bf80ff5c219732d371b.html#ab8ec896cb3461bf80ff5c219732d371b",
"classMayaFlux_1_1Kakshya_1_1WindowContainer_ad438edf99cbef75e53d6161ba55997fa.html#ad438edf99cbef75e53d6161ba55997fa",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a828ee91d6572a9ef58037f989e240dfe.html#a828ee91d6572a9ef58037f989e240dfe",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a8cf80f997179efca17af3f1087574729.html#a8cf80f997179efca17af3f1087574729",
"classMayaFlux_1_1Kriya_1_1SamplingPipeline_abd0dd2217a7a1d78eb6595dd0476406c.html#abd0dd2217a7a1d78eb6595dd0476406c",
"classMayaFlux_1_1Memory_1_1RingBuffer_af0058271894545c4b5aa609434f4b725.html#af0058271894545c4b5aa609434f4b725",
"classMayaFlux_1_1Nexus_1_1Emitter_afb14941c304e0311d41dbf9d0c48beba.html#afb14941c304e0311d41dbf9d0c48beba",
"classMayaFlux_1_1Nexus_1_1Wiring_a43787173a72e6ff3126d818430ec5587.html#a43787173a72e6ff3126d818430ec5587",
"classMayaFlux_1_1Nodes_1_1CompositeOpNode_a0063860959c8ee8f2383ae2b3e95937f.html#a0063860959c8ee8f2383ae2b3e95937f",
"classMayaFlux_1_1Nodes_1_1Filters_1_1IIR_addff49b61bd0798e67f305d59a8a5afa.html#addff49b61bd0798e67f305d59a8a5afa",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a0f95950c38cb7ba9ce04457b10d80d91.html#a0f95950c38cb7ba9ce04457b10d80d91",
"classMayaFlux_1_1Nodes_1_1Generator_1_1PolynomialContext.html",
"classMayaFlux_1_1Nodes_1_1GpuMatrixData_a341d7745a46eb00e38f16139460b382b.html#a341d7745a46eb00e38f16139460b382b",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PathGeneratorNode_a3e8c1fc1d2482fe26e231289e6516f2f.html#a3e8c1fc1d2482fe26e231289e6516f2f",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1TopologyGeneratorNode_a572a123b3a5157e8f4937bbc7c24dfd4.html#a572a123b3a5157e8f4937bbc7c24dfd4",
"classMayaFlux_1_1Nodes_1_1Input_1_1OSCNode_af1f637d7c432abd4b0a2783a4927a230.html#af1f637d7c432abd4b0a2783a4927a230",
"classMayaFlux_1_1Nodes_1_1Network_1_1MeshTransformOperator_a0e0383f7cda9ab0c5f0c94695c7c58da.html#a0e0383f7cda9ab0c5f0c94695c7c58da",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a66193cb19e1aa63e6cfb3a31b35d3449.html#a66193cb19e1aa63e6cfb3a31b35d3449",
"classMayaFlux_1_1Nodes_1_1Network_1_1PhysicsOperator_a20b446d35e44a8152c2bc095d95f0e66.html#a20b446d35e44a8152c2bc095d95f0e66",
"classMayaFlux_1_1Nodes_1_1Network_1_1ResonatorNetwork_a6b73907bb0bbf24c3a13d7c9bd2eee4f.html#a6b73907bb0bbf24c3a13d7c9bd2eee4f",
"classMayaFlux_1_1Nodes_1_1Network_1_1WaveguideNetwork_ace0db25b38d5c26a1fa4e2d4f0a1e6cd.html#ace0db25b38d5c26a1fa4e2d4f0a1e6cd",
"classMayaFlux_1_1Nodes_1_1Node_a9d82ce9915da354955f5f143241a9890.html#a9d82ce9915da354955f5f143241a9890",
"classMayaFlux_1_1Portal_1_1Forma_1_1Context.html",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a1df1f3c3ad7d059442f71219214119ef.html#a1df1f3c3ad7d059442f71219214119ef",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_aa9c6a8bc1c6cc0082f4519bd71da28c5.html#aa9c6a8bc1c6cc0082f4519bd71da28c5",
"classMayaFlux_1_1Portal_1_1Text_1_1GlyphAtlas_a7926e899e50ec8c77ac02b5cae2052cd.html#a7926e899e50ec8c77ac02b5cae2052cd",
"classMayaFlux_1_1Vruta_1_1EventSource_a668cdea29534b712276fd45c8b7d3c8d.html#a668cdea29534b712276fd45c8b7d3c8d",
"classMayaFlux_1_1Vruta_1_1Routine_a5e2ef539ea07ad5bb98359674371573b.html#a5e2ef539ea07ad5bb98359674371573b",
"classMayaFlux_1_1Vruta_1_1WindowEventSource_a066ed07d41ee880f10162fa53289ddfe.html#a066ed07d41ee880f10162fa53289ddfe",
"classMayaFlux_1_1Yantra_1_1ComputeOperation_a1d116c6d59286b0bcd39356c180c76d0.html#a1d116c6d59286b0bcd39356c180c76d0",
"classMayaFlux_1_1Yantra_1_1FluentExecutor_a4d4261a42e6a14824c02e8d3ea414a58.html#a4d4261a42e6a14824c02e8d3ea414a58",
"classMayaFlux_1_1Yantra_1_1GpuSorter_a5b07472fb50bc4184da09e0fae03aa58.html#a5b07472fb50bc4184da09e0fae03aa58",
"classMayaFlux_1_1Yantra_1_1OperationRegistry_ab9cbd4fea08320f30fc201342c1a49fd.html#ab9cbd4fea08320f30fc201342c1a49fd",
"classMayaFlux_1_1Yantra_1_1TemporalTransformer_ad66e5e9f84d2d7fef6657fd89f9849a1.html#ad66e5e9f84d2d7fef6657fd89f9849a1",
"classMayaFlux_1_1Yantra_1_1UniversalSorter_aa417b948d719abdc96ac77ccfcf6be7a.html#aa417b948d719abdc96ac77ccfcf6be7a",
"functions_func_q.html",
"namespaceMayaFlux_1_1Buffers_ab91dc7fea50f8badadc42ab1cea40a06.html#ab91dc7fea50f8badadc42ab1cea40a06",
"namespaceMayaFlux_1_1IO_a36a3852542b551e90c97c41c9ed56f62.html#a36a3852542b551e90c97c41c9ed56f62ac7758b9f88c5b97410d1c6a5f788d307",
"namespaceMayaFlux_1_1Kakshya_a6858a30e8f410b5f7808389074aa296f.html#a6858a30e8f410b5f7808389074aa296fa3ef5408e7dba79031dc0192bf76c8ceb",
"namespaceMayaFlux_1_1Kinesis_1_1Discrete_af8091f299e33adbdf60f9dde88702d4d.html#af8091f299e33adbdf60f9dde88702d4d",
"namespaceMayaFlux_1_1Nodes_1_1Generator_a746cbb1b07a1a9fd4eb078f06c0c295f.html#a746cbb1b07a1a9fd4eb078f06c0c295fa39403cd282d944abcd4f14996cb71bcb",
"namespaceMayaFlux_1_1Portal_1_1Graphics_a93450081141689c09c7ca3c35d883f7d.html#a93450081141689c09c7ca3c35d883f7da26a4b44a837bf97b972628509912b4a5",
"namespaceMayaFlux_1_1Yantra_a0fdcb54bbbc55da5e4974ccc18e56e4b.html#a0fdcb54bbbc55da5e4974ccc18e56e4baa07915f42c718e1fccf4694a6b1d2092",
"namespaceMayaFlux_1_1Yantra_af03c3714714287e5fb21aa133b375f3a.html#af03c3714714287e5fb21aa133b375f3aa63a868b6c021790e7125fe2b18e9b4bf",
"namespaceMayaFlux_aac38992207484764f0a4d9d0e3d82d6b.html#aac38992207484764f0a4d9d0e3d82d6ba7d49acf02f44f345574cdbde3593fdde",
"structMayaFlux_1_1Buffers_1_1BufferRoutingState_acc61d876d3db45cb55da82254c94146c.html#acc61d876d3db45cb55da82254c94146c",
"structMayaFlux_1_1Buffers_1_1RootGraphicsUnit_ae8a7eba6b16c22ee0a9a0cc9d0efc2c4.html#ae8a7eba6b16c22ee0a9a0cc9d0efc2c4",
"structMayaFlux_1_1Core_1_1EndpointInfo_ae7d846bfb979a9967a44da9f7e2fa55f.html#ae7d846bfb979a9967a44da9f7e2fa55f",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_a2e8585c33510ef6813b996ad437a8fc7.html#a2e8585c33510ef6813b996ad437a8fc7",
"structMayaFlux_1_1Core_1_1GraphicsSurfaceInfo_ad4c0f27c4b5f2bbeae59eaa7d1973c69.html#ad4c0f27c4b5f2bbeae59eaa7d1973c69",
"structMayaFlux_1_1Core_1_1InputValue_addf67f4f0cee53f5d0226e3444f1f230.html#addf67f4f0cee53f5d0226e3444f1f230",
"structMayaFlux_1_1Core_1_1TCPBackendInfo_a16a2a9cd5c8d1723346b07af0ad93c45.html#a16a2a9cd5c8d1723346b07af0ad93c45",
"structMayaFlux_1_1Core_1_1WindowEvent_a2c5437b3ad5569fb3b20e9fca2a524a0.html#a2c5437b3ad5569fb3b20e9fca2a524a0",
"structMayaFlux_1_1IO_1_1is__vector_3_01std_1_1vector_3_01T_01_4_01_4.html",
"structMayaFlux_1_1Kakshya_1_1DataDimension_aafb5f78e7e3682f8785ef62f77e5d77a.html#aafb5f78e7e3682f8785ef62f77e5d77a",
"structMayaFlux_1_1Kakshya_1_1RegionSegment_a60c0f61d39bb0d03e291ae264a1f4eaf.html#a60c0f61d39bb0d03e291ae264a1f4eaf",
"structMayaFlux_1_1Kinesis_1_1NavigationConfig_a6f3b047eef2c301d4c8d829ca98a42f2.html#a6f3b047eef2c301d4c8d829ca98a42f2",
"structMayaFlux_1_1Kriya_1_1SampleDelay_a5fa203c5e065a4d8fca2116e51b8d9ab.html#a5fa203c5e065a4d8fca2116e51b8d9ab",
"structMayaFlux_1_1Nodes_1_1GpuSync_1_1GeometryWriterNode_1_1GeometryState_a503042fd2f44d22874032ed368bc7794.html#a503042fd2f44d22874032ed368bc7794",
"structMayaFlux_1_1Nodes_1_1Network_1_1ResonatorNetwork_1_1ResonatorNode_a8862fc909de26797f4a16c898f2b5dd2.html#a8862fc909de26797f4a16c898f2b5dd2",
"structMayaFlux_1_1Portal_1_1Forma_1_1ValueRow_adba230f4f34b6efd0ed230d93fa46a50.html#adba230f4f34b6efd0ed230d93fa46a50",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderReflectionInfo_a72c24af0bdb130827301d43626b4963c.html#a72c24af0bdb130827301d43626b4963c",
"structMayaFlux_1_1Vruta_1_1audio__promise_ab90fc379831442e9d16503d549001a57.html#ab90fc379831442e9d16503d549001a57",
"structMayaFlux_1_1Yantra_1_1ExecutionContext_a5fab9f6f4f3cef18bb06cd95d2e72e94.html#a5fab9f6f4f3cef18bb06cd95d2e72e94",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01T_01_4_ac3b4c65778ae3967bf2bf55932b992f8.html#ac3b4c65778ae3967bf2bf55932b992f8"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';