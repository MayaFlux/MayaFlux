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
"ShaderUtils_8hpp.html#a54a53990589fdcb2c2538055ec519a9f",
"TemporalHelper_8hpp.html#acaaa1439ee62e8bd9181a0fe48456646",
"VKDevice_8cpp_source.html",
"Yantra_8hpp.html#adafbd9d1c8bd9ce9ce587fd1a4787cb5",
"classLila_1_1Server_a59b7f591adc209cf231f3336d9ae45a3.html#a59b7f591adc209cf231f3336d9ae45a3",
"classMayaFlux_1_1Buffers_1_1BufferAccessControl_ae128c7103cff89ef5f7a7786d73f367b.html#ae128c7103cff89ef5f7a7786d73f367b",
"classMayaFlux_1_1Buffers_1_1BufferProcessingControl_a00b86f8ddb04bc67a72ad46bea8ff906.html#a00b86f8ddb04bc67a72ad46bea8ff906",
"classMayaFlux_1_1Buffers_1_1ContainerBuffer_ab326599d890f0e09eec0debaff7b4723.html#ab326599d890f0e09eec0debaff7b4723",
"classMayaFlux_1_1Buffers_1_1FilterProcessor_a3413c5d53d13c44f61a89eb47ee744b9.html#a3413c5d53d13c44f61a89eb47ee744b9",
"classMayaFlux_1_1Buffers_1_1NetworkGeometryBuffer.html",
"classMayaFlux_1_1Buffers_1_1PresentProcessor_a230deb130a9d22e2e5bb17f79e3e7830.html#a230deb130a9d22e2e5bb17f79e3e7830",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a2029ada0eeeeb7af2dcee272857b685f.html#a2029ada0eeeeb7af2dcee272857b685f",
"classMayaFlux_1_1Buffers_1_1TextureProcessor_afb8d66e97b9ee7ccf639ce60794fe789.html#afb8d66e97b9ee7ccf639ce60794fe789",
"classMayaFlux_1_1Buffers_1_1VKBuffer_ac94505bfb06d4dc30d523083107b04a8.html#ac94505bfb06d4dc30d523083107b04a8",
"classMayaFlux_1_1Core_1_1BackendWindowHandler_aa812bf045d7548824a757d712b1b3e75.html#aa812bf045d7548824a757d712b1b3e75",
"classMayaFlux_1_1Core_1_1GlfwWindow_a83175e3bbc8257ddbae1186262594f89.html#a83175e3bbc8257ddbae1186262594f89",
"classMayaFlux_1_1Core_1_1RtAudioDevice_ace6015b5e12c39c5311017c6e7e6d276.html#ace6015b5e12c39c5311017c6e7e6d276",
"classMayaFlux_1_1Core_1_1VKComputePipeline_a93460232cb244acac2afdfb9e5b57a26.html#a93460232cb244acac2afdfb9e5b57a26",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a4dafc5412fbbed7c892d8acd99edcfe2.html#a4dafc5412fbbed7c892d8acd99edcfe2",
"classMayaFlux_1_1Core_1_1VKShaderModule_a1193da7c80e0dd97942d79c3671a127d.html#a1193da7c80e0dd97942d79c3671a127d",
"classMayaFlux_1_1Core_1_1WindowManager_a6adf8120f8407fab46f3faca18e10eab.html#a6adf8120f8407fab46f3faca18e10eab",
"classMayaFlux_1_1IO_1_1ImageReader_a23bace26e9e409a21e3fbd2bf35f6f00.html#a23bace26e9e409a21e3fbd2bf35f6f00",
"classMayaFlux_1_1Journal_1_1Archivist_1_1Impl_aa078132f8f3e385176e90e472394f9ac.html#aa078132f8f3e385176e90e472394f9ac",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_afd3267ff190d889b84537ca75c7fff32.html#afd3267ff190d889b84537ca75c7fff32",
"classMayaFlux_1_1Kakshya_1_1RegionOrganizationProcessor_a689f4b3fcc4e996be99404281d52f2be.html#a689f4b3fcc4e996be99404281d52f2be",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a7fe4fdd8bad531d735d0531a56aaed41.html#a7fe4fdd8bad531d735d0531a56aaed41",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a2e976aec46039f71632fe8c5e4366bba.html#a2e976aec46039f71632fe8c5e4366bbaa93c8d295bb538cdb3302604512828d68",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a28a8163b4bba1143e774e6827c6afed5.html#a28a8163b4bba1143e774e6827c6afed5",
"classMayaFlux_1_1Kriya_1_1NodeTimer_af3eef56a8c66a64e07f64e4306d37554.html#af3eef56a8c66a64e07f64e4306d37554",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_abafc4764937a7175c7c5ac50e265d6ee.html#abafc4764937a7175c7c5ac50e265d6ee",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a66c91cc1dfd94118322a8c641da5d160.html#a66c91cc1dfd94118322a8c641da5d160",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_a562d7fe0c2828b1c6158f306de6628e4.html#a562d7fe0c2828b1c6158f306de6628e4",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1ComputeOutNode_a9d7f8e5de2fc1b9b58b67f91ac24460e.html#a9d7f8e5de2fc1b9b58b67f91ac24460e",
"classMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_a8173f007c0e4c9d9684f255657a97e0a.html#a8173f007c0e4c9d9684f255657a97e0a",
"classMayaFlux_1_1Nodes_1_1Network_1_1ParticleNetwork_a940e69de3eea2171b84183ab5487cee1.html#a940e69de3eea2171b84183ab5487cee1",
"classMayaFlux_1_1Nodes_1_1Node_a7a37eff0de80c73f8978e4f84e1dbd58.html#a7a37eff0de80c73f8978e4f84e1dbd58",
"classMayaFlux_1_1Portal_1_1Graphics_1_1SamplerForge_acfad50a7438053e9d70abce48312b0e7.html#acfad50a7438053e9d70abce48312b0e7",
"classMayaFlux_1_1Portal_1_1Graphics_1_1TextureLoom_aaf5e48e2bb8325c1db70971a89cabec7.html#aaf5e48e2bb8325c1db70971a89cabec7",
"classMayaFlux_1_1Vruta_1_1Event_acab20087788a2c9e35c05302d8b08eb6.html#acab20087788a2c9e35c05302d8b08eb6",
"classMayaFlux_1_1Vruta_1_1SoundRoutine_a8ac72398f64b0f7f0d9d8df6d16eb3ef.html#a8ac72398f64b0f7f0d9d8df6d16eb3ef",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_a44cb157cea1acbd1c9cbb4f369ad03be.html#a44cb157cea1acbd1c9cbb4f369ad03be",
"classMayaFlux_1_1Yantra_1_1FeatureExtractor_a84beb259bb9d64ff1b966c79f68a9267.html#a84beb259bb9d64ff1b966c79f68a9267",
"classMayaFlux_1_1Yantra_1_1OperationPool_afc6843b3e75ff51c620842e46fcfeb69.html#afc6843b3e75ff51c620842e46fcfeb69",
"classMayaFlux_1_1Yantra_1_1UniversalAnalyzer_a4a53048a2fa21b022ac3f4ca4cebed86.html#a4a53048a2fa21b022ac3f4ca4cebed86",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_a994553abb6d63f80f1815629e68a50e9.html#a994553abb6d63f80f1815629e68a50e9",
"lila__server_8cpp_ac7608332d002ef2745359f4cada4afc8.html#ac7608332d002ef2745359f4cada4afc8",
"md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412",
"namespaceMayaFlux_1_1Journal_1_1AnsiColors_ac6eaaaafdc2b685f622bd5dd666719c3.html#ac6eaaaafdc2b685f622bd5dd666719c3",
"namespaceMayaFlux_1_1Kriya.html",
"namespaceMayaFlux_1_1Utils_aacdbbac22afb71086e6c1ac1ad1a4d41.html#aacdbbac22afb71086e6c1ac1ad1a4d41",
"namespaceMayaFlux_1_1Yantra_a7cf5bf7190486c22ed7929c9cff96a06.html#a7cf5bf7190486c22ed7929c9cff96a06",
"namespaceMayaFlux_a1098a01a89726849902f5863981f4db4.html#a1098a01a89726849902f5863981f4db4",
"structLila_1_1ClangInterpreter_1_1EvalResult_a5257c34fe8a6abd69892c3b361fe4421.html#a5257c34fe8a6abd69892c3b361fe4421",
"structMayaFlux_1_1Buffers_1_1RootBuffer_1_1PendingBufferOp.html",
"structMayaFlux_1_1Core_1_1DeviceInfo_a809a7398a20f107696dfa071c0948cb8.html#a809a7398a20f107696dfa071c0948cb8",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_ac45c3ef104bfa3a345c44d55651ce8e4.html#ac45c3ef104bfa3a345c44d55651ce8e4a049518eb4dc1859c7cebbe15876cfd63",
"structMayaFlux_1_1Core_1_1PushConstantInfo_a12247b52770c5e640ba725ce6795f901.html#a12247b52770c5e640ba725ce6795f901",
"structMayaFlux_1_1Core_1_1WindowEvent_1_1KeyData.html",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure.html",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a914b425f3eeeb8074f6a0991a04c4227.html#a914b425f3eeeb8074f6a0991a04c4227",
"structMayaFlux_1_1Kriya_1_1FrameDelay_a8188e7d9ca235a980c1527b9a7702fe0.html#a8188e7d9ca235a980c1527b9a7702fe0",
"structMayaFlux_1_1Portal_1_1Graphics_1_1RenderPassAttachment_a6e24253ebc453d54070b8478dd7b876f.html#a6e24253ebc453d54070b8478dd7b876f",
"structMayaFlux_1_1Vruta_1_1graphics__promise_a52c288a83a92999abe2403171b32ede5.html#a52c288a83a92999abe2403171b32ede5",
"structMayaFlux_1_1Yantra_1_1PooledOperationInfo_ad95cdb9c7ecb50da75c893437c25a5d1.html#ad95cdb9c7ecb50da75c893437c25a5d1"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';