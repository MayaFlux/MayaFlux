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
        [ "Build", "md_README.html#autotoc_md61", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md63", [
        [ "Basic Application Structure", "md_README.html#autotoc_md64", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md66", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md68", [
        [ "Tutorials", "md_README.html#autotoc_md69", null ],
        [ "API Documentation", "md_README.html#autotoc_md70", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md72", null ],
      [ "Philosophy", "md_README.html#autotoc_md74", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md76", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md78", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md79", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md80", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md81", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md82", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md84", null ],
      [ "Contributing", "md_README.html#autotoc_md86", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md88", null ],
      [ "Contact", "md_README.html#autotoc_md90", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md95", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md97", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md99", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md102", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md104", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md106", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md108", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md115", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md116", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md117", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md118", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md119", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md120", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md121", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md122", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md123", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md124", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md125", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md126", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md127", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md129", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md130", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md131", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md132", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md133", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md134", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md135", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md136", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md137", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md138", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md140", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md141", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md142", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md143", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md147", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md149", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md151", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md156", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md161", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md167", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md168", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md169", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md170", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md171", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md172", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md193", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md194", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md195", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md196", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md197", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md198", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md199", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md200", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md201", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md202", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md203", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md204", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md205", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md206", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md207", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md208", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md209", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md210", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md211", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md212", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md213", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md214", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md215", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md216", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md217", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md253", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md255", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md256", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md258", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md260", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md262", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md264", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md265", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md266", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md267", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md268", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md269", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md271", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md273", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md275", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md277", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md278", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md279", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md280", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md281", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md282", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md283", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md284", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md285", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md286", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md287", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md288", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md289", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md291", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md293", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md295", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md297", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md299", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md301", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md303", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md305", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md307", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md309", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md311", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md312", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md313", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md314", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md315", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md316", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md317", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md320", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md321", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md323", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md325", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md327", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md329", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md331", null ],
        [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md333", [
          [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", null ]
        ] ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md342", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md344", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md346", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md386", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
        ] ],
        [ "StreamWriteProcessor (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", null ],
          [ "Expansion 1: What StreamWriteProcessor Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md419", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md426", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md428", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md430", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md433", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md435", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md437", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md439", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md441", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md444", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md446", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md448", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md450", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md452", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
      [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ],
      [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
      [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
      [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
      [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "Expansion 4: ContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md493", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md495", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md497", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md499", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md501", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md503", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md504", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md505", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md507", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md509", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md511", null ],
        [ "Expansion 3: StreamWriteProcessor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md513", null ],
        [ "Expansion 4: FileBridgeBuffer—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md515", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md517", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md519", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md521", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md523", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md525", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md526", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md527", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md529", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md531", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md533", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md535", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md537", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md539", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md541", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md543", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md545", null ]
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
"Chronie_8cpp.html#abe02d6d4a3a24e5787913202d5834384",
"Creator_8cpp.html#a9cc3e2fb1bbb15299ccc77dd059165c4",
"FeatureExtractor_8hpp.html#a26091e7c0eb9db9c3719b4574a945a8b",
"GraphicsUtils_8hpp.html#a5a7c1fd1307aa888ba7888ad8298d9ef",
"MatrixHelper_8hpp.html#a232cb99a468d04d193b1776fe115768d",
"ProcessingTokens_8hpp.html#af6d1b053535f945d3e5fec41c8f1fafaab9b4bdd221be4765efe9bb39689d78b8",
"SortingHelper_8hpp.html#a06b399b5c5804274f48a0f2309c1f79c",
"TextureNode_8hpp.html",
"VKShaderModule_8cpp_source.html",
"classLila_1_1ClangInterpreter_a30b0911f8c51f3d0fe2b653897a32ad2.html#a30b0911f8c51f3d0fe2b653897a32ad2",
"classLila_1_1Server_ab53cab573d0ad8d550a57faf2baf88cc.html#ab53cab573d0ad8d550a57faf2baf88cc",
"classMayaFlux_1_1Buffers_1_1BufferDownloadProcessor_aca8a6508ca020e794744ce76d41a810c.html#aca8a6508ca020e794744ce76d41a810c",
"classMayaFlux_1_1Buffers_1_1BufferProcessingControl_aca98f612509068999521651adcad13ef.html#aca98f612509068999521651adcad13ef",
"classMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_a5164b091db5f66824edb8714b54c2867.html#a5164b091db5f66824edb8714b54c2867",
"classMayaFlux_1_1Buffers_1_1GeometryBuffer_a958882d2b9507c346b857bb07eb974e3.html#a958882d2b9507c346b857bb07eb974e3",
"classMayaFlux_1_1Buffers_1_1NodeBindingsProcessor_af778bcd2c3d39dee8af08b210d1d81dd.html#af778bcd2c3d39dee8af08b210d1d81dd",
"classMayaFlux_1_1Buffers_1_1RootBuffer_a7550585e4dc8667433f838ad48e0182a.html#a7550585e4dc8667433f838ad48e0182a",
"classMayaFlux_1_1Buffers_1_1TextureBindBuffer_a32294c317b98b002f63848ab79ea606e.html#a32294c317b98b002f63848ab79ea606e",
"classMayaFlux_1_1Buffers_1_1VKBuffer.html",
"classMayaFlux_1_1Core_1_1AudioSubsystem_a745a5d31ef0b095e1351a794806ed289.html#a745a5d31ef0b095e1351a794806ed289",
"classMayaFlux_1_1Core_1_1Engine_a412cb254c5f79a702caa3d5f88371c7e.html#a412cb254c5f79a702caa3d5f88371c7e",
"classMayaFlux_1_1Core_1_1GraphicsSubsystem_adb1f7534da87613f4cd34c221dfc568b.html#adb1f7534da87613f4cd34c221dfc568b",
"classMayaFlux_1_1Core_1_1SubsystemManager_a9d69dbe5ecd65c44ab624dff24fb8aa0.html#a9d69dbe5ecd65c44ab624dff24fb8aa0",
"classMayaFlux_1_1Core_1_1VKDescriptorManager_aa536be95394347cba1481fea7ee9d524.html#aa536be95394347cba1481fea7ee9d524",
"classMayaFlux_1_1Core_1_1VKImage_a603d6ef31c19453be73d8a72929184ef.html#a603d6ef31c19453be73d8a72929184efab8a1431064f351dbef92871d02bab8cf",
"classMayaFlux_1_1Core_1_1VKSwapchain_a57dba5cf2ffab90c12ce8cb753ce05ab.html#a57dba5cf2ffab90c12ce8cb753ce05ab",
"classMayaFlux_1_1Creator_a248b156b468ba29a31b3fe4da2ef9ddc.html#a248b156b468ba29a31b3fe4da2ef9ddc",
"classMayaFlux_1_1IO_1_1SoundFileReader_a8bfd1aa043b76cab1172b2d00705066f.html#a8bfd1aa043b76cab1172b2d00705066f",
"classMayaFlux_1_1Journal_1_1RingBuffer_ad7e96ba193b358ad4b72e3ea5301adfc.html#ad7e96ba193b358ad4b72e3ea5301adfc",
"classMayaFlux_1_1Kakshya_1_1NDDataContainer_a1057e363c4c7bfeae33a325b2ec18da4.html#a1057e363c4c7bfeae33a325b2ec18da4",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_afb1e9fa2087ce89c809bba91d0ac9abd.html#afb1e9fa2087ce89c809bba91d0ac9abd",
"classMayaFlux_1_1Kakshya_1_1StreamContainer.html",
"classMayaFlux_1_1Kriya_1_1BufferOperation_a5bf68d41220c3bf6b540073e821fd733.html#a5bf68d41220c3bf6b540073e821fd733",
"classMayaFlux_1_1Kriya_1_1CaptureBuilder_a7d9dcc40a50136bcfbf865202cfc7bda.html#a7d9dcc40a50136bcfbf865202cfc7bda",
"classMayaFlux_1_1Nodes_1_1ChainNode_a218d8c8509ee92e129b3f0655765522e.html#a218d8c8509ee92e129b3f0655765522e",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Impulse_a4d32cf8402099b3a2fa3581de86d87b7.html#a4d32cf8402099b3a2fa3581de86d87b7",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a05848fc75ee39784e5fba308111b8200.html#a05848fc75ee39784e5fba308111b8200",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Sine_a9946405253609b4531c5ee9177370173.html#a9946405253609b4531c5ee9177370173",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointCollectionNode_aa79c2ae388168f24a8567d1e7c4bbda1.html#aa79c2ae388168f24a8567d1e7c4bbda1",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_ad56f5f12b2711cd6d27fd71bd015179b.html#ad56f5f12b2711cd6d27fd71bd015179b",
"classMayaFlux_1_1Nodes_1_1ParticleNetwork_a36a9159edc16917cf51550dcaec81043.html#a36a9159edc16917cf51550dcaec81043",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a8b62fc52029322ba0eb773bca6340e11.html#a8b62fc52029322ba0eb773bca6340e11",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_ae48a3feff5a9a139688512316c1c6421.html#ae48a3feff5a9a139688512316c1c6421",
"classMayaFlux_1_1Vruta_1_1EventManager_aeda668e2b94ff8f7959aa68b2d2732db.html#aeda668e2b94ff8f7959aa68b2d2732db",
"classMayaFlux_1_1Vruta_1_1Routine_ac4b869e50cce6dbe3b970d247eb1e11c.html#ac4b869e50cce6dbe3b970d247eb1e11c",
"classMayaFlux_1_1Yantra_1_1ComputationGrammar_a8927fad320f2744a510ab4ace9e3743f.html#a8927fad320f2744a510ab4ace9e3743f",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer_a43d6403ed62473f4f439fe94174b6864.html#a43d6403ed62473f4f439fe94174b6864",
"classMayaFlux_1_1Yantra_1_1OperationHelper_ab87e318fc808c4e06d895135af6097e2.html#ab87e318fc808c4e06d895135af6097e2",
"classMayaFlux_1_1Yantra_1_1StatisticalAnalyzer_ad6f638b9c42a1ada446cedd0a2cbbd12.html#ad6f638b9c42a1ada446cedd0a2cbbd12",
"classMayaFlux_1_1Yantra_1_1UniversalSorter_a909b60d5c509fb7363a08c70130d4dc0.html#a909b60d5c509fb7363a08c70130d4dc0",
"functions_v.html",
"md_docs_2StarterTasks.html#autotoc_md314",
"namespaceMayaFlux_1_1Core_a2e82bbb5d2933af42ad2237ece2b38a3.html#a2e82bbb5d2933af42ad2237ece2b38a3",
"namespaceMayaFlux_1_1Kakshya_a9e6ea2534816c07babe66fab4f084199.html#a9e6ea2534816c07babe66fab4f084199",
"namespaceMayaFlux_1_1Portal_1_1Graphics_aaef77e9962a207a691e1f9d328588a8a.html#aaef77e9962a207a691e1f9d328588a8aa92ba720a64bd436721e0d2aa50e170a9",
"namespaceMayaFlux_1_1Yantra_a4e4d7c9ee689d59863f323f9262247ce.html#a4e4d7c9ee689d59863f323f9262247cea94e94133f4bdc1794c6b647b8ea134d0",
"namespaceMayaFlux_1_1Yantra_aeae845735454e552bd99f99e725e9931.html#aeae845735454e552bd99f99e725e9931",
"namespaceMayaFlux_affda79373ae79a02574a2768948c300e.html#affda79373ae79a02574a2768948c300e",
"structMayaFlux_1_1Buffers_1_1ProcessorTokenInfo_a8dc02902513c4c5e929241d400a9e449.html#a8dc02902513c4c5e929241d400a9e449",
"structMayaFlux_1_1Core_1_1ComputePipelineConfig_a1570ad21c394648c4230662d2c5f4530.html#a1570ad21c394648c4230662d2c5f4530",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_a1223d989e3c553979996266bf78157c0.html#a1223d989e3c553979996266bf78157c0",
"structMayaFlux_1_1Core_1_1GraphicsSurfaceInfo_a716b4de59e7824fbaf02228a8cbf0c7d.html#a716b4de59e7824fbaf02228a8cbf0c7da71c8ab8ff994f8954ddcead46d877319",
"structMayaFlux_1_1Core_1_1VideoMode_ada9affc88fcf3ddfef56bac33592ee70.html#ada9affc88fcf3ddfef56bac33592ee70",
"structMayaFlux_1_1Journal_1_1JournalEntry_a2bcd6c17f669c2bd67fedcbaa05466ca.html#a2bcd6c17f669c2bd67fedcbaa05466ca",
"structMayaFlux_1_1Kakshya_1_1DataDimension_adb2bac98f840e5eadcbd69054b3ae05e.html#adb2bac98f840e5eadcbd69054b3ae05e",
"structMayaFlux_1_1Kakshya_1_1VertexLayout_a544b48334c1b582e8bf50da8215d2095.html#a544b48334c1b582e8bf50da8215d2095",
"structMayaFlux_1_1Portal_1_1Graphics_1_1PushConstantRangeInfo_a611984393b5572ebd3df393c7c966370.html#a611984393b5572ebd3df393c7c966370",
"structMayaFlux_1_1Registry_1_1Service_1_1DisplayService_a7e57c432ae142d889e393f958fbc6357.html#a7e57c432ae142d889e393f958fbc6357",
"structMayaFlux_1_1Yantra_1_1IO_acf749a4324dbf4873cef5644b91796c1.html#acf749a4324dbf4873cef5644b91796c1"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';