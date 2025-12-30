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
      [ "The Paradigm", "md_README.html#autotoc_md46", [
        [ "What Becomes Possible", "md_README.html#autotoc_md47", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md49", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md51", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md52", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md53", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md54", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md56", [
        [ "Management Mode", "md_README.html#autotoc_md57", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md58", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md59", [
        [ "Requirements", "md_README.html#autotoc_md60", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md61", null ],
        [ "Build", "md_README.html#autotoc_md62", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md64", [
        [ "Basic Application Structure", "md_README.html#autotoc_md65", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md67", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md69", [
        [ "Tutorials", "md_README.html#autotoc_md70", null ],
        [ "API Documentation", "md_README.html#autotoc_md71", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md73", null ],
      [ "Philosophy", "md_README.html#autotoc_md75", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md77", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md79", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md80", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md81", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md82", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md83", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md85", null ],
      [ "Contributing", "md_README.html#autotoc_md87", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md89", null ],
      [ "Contact", "md_README.html#autotoc_md91", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md96", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md98", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md100", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md103", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md105", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md107", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md109", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md116", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md117", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md118", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md119", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md120", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md121", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md122", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md123", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md124", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md125", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md126", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md127", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md129", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md130", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md131", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md132", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md133", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md134", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md135", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md136", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md137", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md138", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md140", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md141", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md142", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md143", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md144", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md148", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md150", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md152", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md157", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md162", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md168", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md169", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md170", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md171", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md172", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md194", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md195", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md196", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md197", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md198", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md199", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md200", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md201", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md202", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md203", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md204", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md205", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md206", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md207", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md208", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md209", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md210", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md211", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md212", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md213", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md214", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md215", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md216", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md217", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md218", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md262", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md264", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md265", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md267", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md269", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md271", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md273", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md274", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md275", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md276", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md277", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md278", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md280", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md282", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md284", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md286", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md287", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md288", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md289", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md290", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md291", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md292", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md293", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md294", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md295", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md296", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md297", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md298", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md300", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md302", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md304", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md306", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md308", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md310", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md312", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md314", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md316", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md318", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md320", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md321", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md322", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md323", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md324", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md325", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md326", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md329", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md330", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md331", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md332", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md333", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md339", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md341", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md342", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md343", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md345", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md348", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md350", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md352", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md354", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md358", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md375", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md379", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md383", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ]
        ] ],
        [ "StreamWriteProcessor (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", null ],
          [ "Expansion 1: What StreamWriteProcessor Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md409", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md419", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md426", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md427", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md428", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md429", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md430", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md431", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md434", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md435", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md436", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md437", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md438", null ],
        [ "Expansion 4: ContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md439", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md440", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md441", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md442", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md443", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md444", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md445", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md447", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md450", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md451", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md453", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md454", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", null ],
        [ "Expansion 3: StreamWriteProcessor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
        [ "Expansion 4: FileBridgeBuffer—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ]
      ] ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
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
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"AggregateBindingsProcessor_8cpp.html",
"Chronie_8cpp.html#a0b2f122006fb411804adab9baa87647d",
"Core_8hpp.html#a89f2d26d88199ad81c229c6eef0706bd",
"ExtractionHelper_8hpp.html#a33eeb9e28202e46d72f8ff6938a7906e",
"GraphicsBackend_8hpp_source.html",
"MathematicalTransformer_8hpp.html#a48e533bd2ca1105afab7227143eb9f42",
"ProcessingArchitecture_8hpp.html",
"ShaderUtils_8hpp.html#a5b84f9d8a72e3d14294025695080d76d",
"TemporalHelper_8hpp.html#aeae845735454e552bd99f99e725e9931",
"VKDevice_8hpp_source.html",
"Yantra_8hpp.html#adc93a991311699aee935e8a53a9a9ea5",
"classLila_1_1Server_a6fd5a6f20e33b14fad2bd85183d9fd44.html#a6fd5a6f20e33b14fad2bd85183d9fd44",
"classMayaFlux_1_1Buffers_1_1BufferAccessControl_ae92fa8262d95697a1198c36d705b765d.html#ae92fa8262d95697a1198c36d705b765d",
"classMayaFlux_1_1Buffers_1_1BufferProcessingControl_a071db42a8b90ab8067b6cf1dca5f5ce7.html#a071db42a8b90ab8067b6cf1dca5f5ce7",
"classMayaFlux_1_1Buffers_1_1ContainerBuffer_ab9af4f0dd2c8ba5235e07bdef863dd39.html#ab9af4f0dd2c8ba5235e07bdef863dd39",
"classMayaFlux_1_1Buffers_1_1FilterProcessor_aac1ff9bbdef33b661e52ed5f41728aa2.html#aac1ff9bbdef33b661e52ed5f41728aa2",
"classMayaFlux_1_1Buffers_1_1NetworkGeometryBuffer_a09a747b9b0a8d796ca0bb2ae91181221.html#a09a747b9b0a8d796ca0bb2ae91181221",
"classMayaFlux_1_1Buffers_1_1PresentProcessor_a28799cba5ae2c8f4b75ececd4edac62d.html#a28799cba5ae2c8f4b75ececd4edac62d",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a2ba7134648ddbeca54b26e451490de21.html#a2ba7134648ddbeca54b26e451490de21",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager.html",
"classMayaFlux_1_1Buffers_1_1VKBuffer_ad0b0d2e462ce4b68da8fb7cf99540529.html#ad0b0d2e462ce4b68da8fb7cf99540529",
"classMayaFlux_1_1Core_1_1BackendWindowHandler_ac35e7bcac2c855db53d91c72619bd6a7.html#ac35e7bcac2c855db53d91c72619bd6a7",
"classMayaFlux_1_1Core_1_1GlfwWindow_a7dcbaff7075f80a58f89d40c6e37d1fe.html#a7dcbaff7075f80a58f89d40c6e37d1fe",
"classMayaFlux_1_1Core_1_1RtAudioDevice_a22028b56a2c969592b03eebd04a8de6f.html#a22028b56a2c969592b03eebd04a8de6f",
"classMayaFlux_1_1Core_1_1VKComputePipeline_a6c691221c1829abb41a1e51198825827.html#a6c691221c1829abb41a1e51198825827",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a221e362b563394cd2c7eabb878546295.html#a221e362b563394cd2c7eabb878546295",
"classMayaFlux_1_1Core_1_1VKRenderPass_acb5e01f9ce9b21270f5fe4e0fa5bb8c6.html#acb5e01f9ce9b21270f5fe4e0fa5bb8c6",
"classMayaFlux_1_1Core_1_1WindowManager_a3afaca4f9c56554d144872bc6df39778.html#a3afaca4f9c56554d144872bc6df39778",
"classMayaFlux_1_1IO_1_1FileWriter_a325b030ee52d1675ff52ebbaa7c4e917.html#a325b030ee52d1675ff52ebbaa7c4e917",
"classMayaFlux_1_1Journal_1_1Archivist_1_1Impl_a2d617216681007df30e43bf5bfaba480.html#a2d617216681007df30e43bf5bfaba480",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_a48ae11eb9b0fa08ad98ead9dce702ddd.html#a48ae11eb9b0fa08ad98ead9dce702ddd",
"classMayaFlux_1_1Kakshya_1_1RegionCacheManager_aff0ba0f9afe6c039d772674cb01bf0c5.html#aff0ba0f9afe6c039d772674cb01bf0c5",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a5f12a4bd69b500eda0888d11900f2755.html#a5f12a4bd69b500eda0888d11900f2755",
"classMayaFlux_1_1Kriya_1_1BufferCapture.html",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a14c11b8660a7cf766e7afe2fd643460f.html#a14c11b8660a7cf766e7afe2fd643460f",
"classMayaFlux_1_1Kriya_1_1NodeTimer_a5054cfbe5eb79235df6b349cf9cf4af9.html#a5054cfbe5eb79235df6b349cf9cf4af9",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_a8afd06321975537640442865617f26cc.html#a8afd06321975537640442865617f26cc",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a37e348a2523b3556dbb32b14c09e8f72.html#a37e348a2523b3556dbb32b14c09e8f72",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_a1092ee41e329cabef412fc2728f1820f.html#a1092ee41e329cabef412fc2728f1820f",
"classMayaFlux_1_1Nodes_1_1GpuStructuredData_a41aa4cde8af5168316bc8cee93825e34.html#a41aa4cde8af5168316bc8cee93825e34",
"classMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_a345c7d05d53c4962737bf14426c4ce51.html#a345c7d05d53c4962737bf14426c4ce51",
"classMayaFlux_1_1Nodes_1_1Network_1_1ParticleNetwork_a6542f869975171c5bfae100d200d38e0.html#a6542f869975171c5bfae100d200d38e0",
"classMayaFlux_1_1Nodes_1_1Node_a3f33819c592fd1647ce1aa7201dc74dc.html#a3f33819c592fd1647ce1aa7201dc74dc",
"classMayaFlux_1_1Portal_1_1Graphics_1_1SamplerForge_a65d8a4f5d8e73676ac909a077cb6a582.html#a65d8a4f5d8e73676ac909a077cb6a582",
"classMayaFlux_1_1Portal_1_1Graphics_1_1TextureLoom_a3a42e83f8a57c6ea8cf2e15cbaaf3892.html#a3a42e83f8a57c6ea8cf2e15cbaaf3892",
"classMayaFlux_1_1Vruta_1_1Event_a68afeb1723b4b2eb39ad23dd1089d869.html#a68afeb1723b4b2eb39ad23dd1089d869",
"classMayaFlux_1_1Vruta_1_1SoundRoutine_a2632252e0b740f454ecd97a00007ddfc.html#a2632252e0b740f454ecd97a00007ddfc",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_a0d5e3303217ffd150704a11296772a65.html#a0d5e3303217ffd150704a11296772a65",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer_af328f60aff072539d034a91c54bf8bc8.html#af328f60aff072539d034a91c54bf8bc8",
"classMayaFlux_1_1Yantra_1_1OperationPool_aad4dce74997760d7e221b05b88fd1d94.html#aad4dce74997760d7e221b05b88fd1d94",
"classMayaFlux_1_1Yantra_1_1TemporalTransformer_af986fee42616231d9c23fd597bbf3979.html#af986fee42616231d9c23fd597bbf3979",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_a4b29bc78fc586c1a515295eea9602833.html#a4b29bc78fc586c1a515295eea9602833",
"index.html#autotoc_md240",
"md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382",
"namespaceMayaFlux_1_1IO_a582f5249ec35cc284ce9e293ef33d9ce.html#a582f5249ec35cc284ce9e293ef33d9cea7b880b89b33facaeeea9f4a93be68be0",
"namespaceMayaFlux_1_1Kakshya_aec5fc7fe96708768600b4ce8feaba1b8.html#aec5fc7fe96708768600b4ce8feaba1b8a139882c654db8a57f7c3092de1dd0b02",
"namespaceMayaFlux_1_1Utils_a2e008290b4b8f52b87b5571a42bb88b5.html#a2e008290b4b8f52b87b5571a42bb88b5",
"namespaceMayaFlux_1_1Yantra_a6e087cd6a207f24c128efa5a8b96270e.html#a6e087cd6a207f24c128efa5a8b96270ea8f80acdfd62d1e3dc0214d4f29039d7e",
"namespaceMayaFlux_1_1internal_ac0f0b4b1cae6af4441db0ea022e15583.html#ac0f0b4b1cae6af4441db0ea022e15583",
"namespacemembers_n.html",
"structMayaFlux_1_1Buffers_1_1RenderProcessor_1_1VertexInfo_a434ea80f940bcf8a24cba682f9ea9561.html#a434ea80f940bcf8a24cba682f9ea9561",
"structMayaFlux_1_1Core_1_1DescriptorBinding_a7fe284e5a79764dae5b4ceefa8f9213c.html#a7fe284e5a79764dae5b4ceefa8f9213c",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_a3690c743fb85ad0de3f33ab16db445ce.html#a3690c743fb85ad0de3f33ab16db445cea68491a6222f5a943f983e0ce87ca2a58",
"structMayaFlux_1_1Core_1_1InputConfig_ac9f972c7be3a055b7843e1342b3ed415.html#ac9f972c7be3a055b7843e1342b3ed415",
"structMayaFlux_1_1Core_1_1WindowCreateInfo_a9e5a79e7d7b9cb4683c6a2fd0cffae20.html#a9e5a79e7d7b9cb4683c6a2fd0cffae20",
"structMayaFlux_1_1Journal_1_1RealtimeEntry_a87c0eacdc861f915a3ea679ddca30f65.html#a87c0eacdc861f915a3ea679ddca30f65",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a54f4df85bbfd431ffdcc0c28029d541e.html#a54f4df85bbfd431ffdcc0c28029d541e",
"structMayaFlux_1_1Kriya_1_1EventChain_1_1TimedEvent.html",
"structMayaFlux_1_1Portal_1_1Graphics_1_1RenderPipelineConfig_a2f9117ab43c9370ff347aaf3ec51aff7.html#a2f9117ab43c9370ff347aaf3ec51aff7",
"structMayaFlux_1_1Vruta_1_1routine__promise_a1863cf785eadf9523b702f55e15c2158.html#a1863cf785eadf9523b702f55e15c2158",
"structMayaFlux_1_1Yantra_1_1StatisticalAnalysis.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';