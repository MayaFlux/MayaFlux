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
      [ "The Paradigm", "md_README.html#autotoc_md35", [
        [ "What Becomes Possible", "md_README.html#autotoc_md36", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md38", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md40", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md41", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md42", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md43", null ]
      ] ],
      [ "Quick Start", "md_README.html#autotoc_md45", [
        [ "Requirements", "md_README.html#autotoc_md46", null ],
        [ "Build", "md_README.html#autotoc_md47", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md49", [
        [ "Basic Application Structure", "md_README.html#autotoc_md50", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md52", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md54", [
        [ "Tutorials", "md_README.html#autotoc_md55", null ],
        [ "API Documentation", "md_README.html#autotoc_md56", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md58", null ],
      [ "Philosophy", "md_README.html#autotoc_md60", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md62", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md64", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md65", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md66", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md67", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md68", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md70", null ],
      [ "Contributing", "md_README.html#autotoc_md72", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md74", null ],
      [ "Contact", "md_README.html#autotoc_md76", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md81", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md83", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md85", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md88", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md90", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md92", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md94", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md101", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md102", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md103", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md104", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md105", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md106", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md107", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md108", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md109", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md110", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md111", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md112", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md113", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md114", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md115", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md116", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md117", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md118", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md119", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md120", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md121", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md122", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md123", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md124", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md125", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md126", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md127", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md129", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md133", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md135", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md137", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md142", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md147", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md153", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md154", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md155", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md156", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md157", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md158", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md159", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md160", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md161", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md162", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md163", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md164", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md165", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md166", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md167", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md168", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md169", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md170", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md171", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md172", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md179", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md180", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md181", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md182", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md183", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md184", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md185", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md186", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md187", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md188", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md189", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md190", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md191", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md192", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md193", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md194", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md195", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md196", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md197", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md198", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md199", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md200", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md201", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md202", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md203", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md234", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md236", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md237", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md239", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md241", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md243", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md245", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md246", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md247", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md248", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md249", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md250", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md252", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md254", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md256", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md258", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md259", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md260", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md261", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md262", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md263", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md264", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md265", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md266", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md267", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md268", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md269", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md270", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md272", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md274", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md276", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md278", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md280", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md282", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md284", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md286", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md288", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md290", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md292", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md293", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md294", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md295", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md296", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md297", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md298", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md301", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md302", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md304", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md306", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md308", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md310", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md312", null ],
        [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md314", [
          [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md315", null ]
        ] ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md317", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md319", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md321", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md323", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md325", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md327", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md328", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md330", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md332", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md341", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md343", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md345", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md350", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ]
        ] ],
        [ "StreamWriteProcessor (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", null ],
          [ "Expansion 1: What StreamWriteProcessor Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md409", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md429", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md433", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md436", null ],
      [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md438", null ],
      [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md440", null ],
      [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md442", null ],
      [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md444", null ],
      [ "Expansion 5: What <tt>.read()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md446", null ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md450", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md454", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Expansion 4: ContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md490", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md492", null ],
        [ "Expansion 3: StreamWriteProcessor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md494", null ],
        [ "Expansion 4: FileBridgeBuffer—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md496", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md498", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md500", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md502", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md504", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md506", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md507", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md508", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md510", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md512", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md514", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md516", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md518", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md520", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md522", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md524", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md526", null ]
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
"Chronie_8hpp.html#a44556db6c9a1fd8493244b1e6b30e443",
"CycleCoordinator_8cpp.html",
"FileReader_8hpp.html#a582f5249ec35cc284ce9e293ef33d9cea2e5626de6a522fd3e19b15f26dd26293",
"JournalEntry_8hpp.html#a4d1f572381818d4f67c4a5294d68ef2dadb974238714ca8de634a7ce1d083a14f",
"NodeUtils_8hpp.html#a4cc726e3c21a41cdf05c43b6ab318057",
"RenderFlow_8hpp.html#a5218aafde9593ab708f8d34ff5e0ea0a",
"StandardSorter_8hpp.html",
"UniversalSorter_8hpp.html#aa2ae6e7df3e7cf6052ca578c2e0e43e3a94e94133f4bdc1794c6b647b8ea134d0",
"Yantra_8cpp.html#a6b5aed304676ea30ffe84686c4f2bfeb",
"classLila_1_1EventBus_a2b022004d6b3eada15738804209844fc.html#a2b022004d6b3eada15738804209844fc",
"classMayaFlux_1_1Buffers_1_1AudioBuffer_a1cacb0395f79b9d80f2fd2de2aec81d0.html#a1cacb0395f79b9d80f2fd2de2aec81d0",
"classMayaFlux_1_1Buffers_1_1BufferManager_ad1c6582366d5e260db2bc8e24c2f00a8.html#ad1c6582366d5e260db2bc8e24c2f00a8",
"classMayaFlux_1_1Buffers_1_1Buffer_a3253b249acb744cddba21025f884f8c4.html#a3253b249acb744cddba21025f884f8c4",
"classMayaFlux_1_1Buffers_1_1FileToStreamChain_a2d84a7b726db89e6efe3bd034938837f.html#a2d84a7b726db89e6efe3bd034938837f",
"classMayaFlux_1_1Buffers_1_1NodeBuffer_ae7384d59429917161a69deaeb7dac90e.html#ae7384d59429917161a69deaeb7dac90e",
"classMayaFlux_1_1Buffers_1_1RootGraphicsBuffer_a2901d2913f1c6dc769924b99624a03ea.html#a2901d2913f1c6dc769924b99624a03ea",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_a7673914495c7a5a49599cbcc48f727fe.html#a7673914495c7a5a49599cbcc48f727fe",
"classMayaFlux_1_1Core_1_1AudioBackendFactory_ad3e9c4ec1bd04a900ca5fbc46dd6adcb.html#ad3e9c4ec1bd04a900ca5fbc46dd6adcb",
"classMayaFlux_1_1Core_1_1BufferProcessingHandle_a7e88ab03bd0a5574dffead5c4f4d6e59.html#a7e88ab03bd0a5574dffead5c4f4d6e59",
"classMayaFlux_1_1Core_1_1GraphicsSubsystem_a25611dc038e2fa775987e640a7826262.html#a25611dc038e2fa775987e640a7826262",
"classMayaFlux_1_1Core_1_1RtAudioStream_a84da2c36afe576b7ac6443e56aa4cec9.html#a84da2c36afe576b7ac6443e56aa4cec9",
"classMayaFlux_1_1Core_1_1VKContext_aa076977c33941d8fc34f2bfe2fd295a7.html#aa076977c33941d8fc34f2bfe2fd295a7",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_ab6ff30279d339f7d992f88d45bf12cba.html#ab6ff30279d339f7d992f88d45bf12cba",
"classMayaFlux_1_1Core_1_1VKShaderModule_a9a3336159cb14b8f4789b7a7ebdf9de8.html#a9a3336159cb14b8f4789b7a7ebdf9de8",
"classMayaFlux_1_1Core_1_1Window_a5720fd9b32bddd3a26b80bfd97c244c1.html#a5720fd9b32bddd3a26b80bfd97c244c1",
"classMayaFlux_1_1IO_1_1SoundFileReader_a4f7a6ceb98290998b95c4f95b79aa51c.html#a4f7a6ceb98290998b95c4f95b79aa51c",
"classMayaFlux_1_1Journal_1_1FileSink_a02975fcbe03196aa3ff4a6e0473474ea.html#a02975fcbe03196aa3ff4a6e0473474ea",
"classMayaFlux_1_1Kakshya_1_1DynamicSoundStream_aa4e4316022347f61b6ff80925a636ac4.html#aa4e4316022347f61b6ff80925a636ac4",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_a7b404f8fb56db904797e86bdcc38921e.html#a7b404f8fb56db904797e86bdcc38921e",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_ad61a554e45549b00668c783e8ea70ad8.html#ad61a554e45549b00668c783e8ea70ad8",
"classMayaFlux_1_1Kriya_1_1BufferOperation_a0d7ae149fb319df81e6a5fa731e2f692.html#a0d7ae149fb319df81e6a5fa731e2f692",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_aedc72ee53b8fdda6a03e0cbb9076affb.html#aedc72ee53b8fdda6a03e0cbb9076affb",
"classMayaFlux_1_1Nodes_1_1BinaryOpNode_a757bd5eed3572abf3c4335a767b0fc72.html#a757bd5eed3572abf3c4335a767b0fc72",
"classMayaFlux_1_1Nodes_1_1Generator_1_1GeneratorContext_ae863328d82f6953712ac2f172cce6e6a.html#ae863328d82f6953712ac2f172cce6e6a",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_aa7de388069468353a61a5196d9b89992.html#aa7de388069468353a61a5196d9b89992",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_af91a098e3618e36777e77df36407b3d4.html#af91a098e3618e36777e77df36407b3d4",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_ab7485f6b5bd28a3d64ffaa5034d3ed9a.html#ab7485f6b5bd28a3d64ffaa5034d3ed9a",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a6e776dc125d8568846864650abef3825.html#a6e776dc125d8568846864650abef3825",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_ad4d07cfd5c332c269e6b3d4618ab4f8f.html#ad4d07cfd5c332c269e6b3d4618ab4f8f",
"classMayaFlux_1_1Vruta_1_1EventManager_aeda668e2b94ff8f7959aa68b2d2732db.html#aeda668e2b94ff8f7959aa68b2d2732db",
"classMayaFlux_1_1Vruta_1_1Routine_ac4b869e50cce6dbe3b970d247eb1e11c.html#ac4b869e50cce6dbe3b970d247eb1e11c",
"classMayaFlux_1_1Yantra_1_1ComputationGrammar_a8927fad320f2744a510ab4ace9e3743f.html#a8927fad320f2744a510ab4ace9e3743f",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer_a43d6403ed62473f4f439fe94174b6864.html#a43d6403ed62473f4f439fe94174b6864",
"classMayaFlux_1_1Yantra_1_1OperationHelper_ab87e318fc808c4e06d895135af6097e2.html#ab87e318fc808c4e06d895135af6097e2",
"classMayaFlux_1_1Yantra_1_1StatisticalAnalyzer_ad6f638b9c42a1ada446cedd0a2cbbd12.html#ad6f638b9c42a1ada446cedd0a2cbbd12",
"classMayaFlux_1_1Yantra_1_1UniversalSorter_a909b60d5c509fb7363a08c70130d4dc0.html#a909b60d5c509fb7363a08c70130d4dc0",
"functions_vars_c.html",
"md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md312",
"namespaceMayaFlux_1_1Core_aa98c135cff2f0b2a5dfc0fc1e624681c.html#aa98c135cff2f0b2a5dfc0fc1e624681cab22957ad8078e8d73de61aef53d13a74",
"namespaceMayaFlux_1_1Kakshya_ab1d4d86d7f3183462598c145ce313388.html#ab1d4d86d7f3183462598c145ce313388",
"namespaceMayaFlux_1_1Portal_1_1Graphics_ad9c08cabfd2bc89d18b82034affb1726.html#ad9c08cabfd2bc89d18b82034affb1726a969f331a87d8c958473c32b4d0e61a44",
"namespaceMayaFlux_1_1Yantra_a5b8650bb485d510d0cd23d69a4f7437a.html#a5b8650bb485d510d0cd23d69a4f7437a",
"namespaceMayaFlux_1_1Yantra_af6644b9fe9fde9b35eef151e1d4d4977.html#af6644b9fe9fde9b35eef151e1d4d4977",
"namespacemembers_func_w.html",
"structMayaFlux_1_1Buffers_1_1ShaderDispatchConfig_a1e8ff5bbd92ef954cb62756cf8798174.html#a1e8ff5bbd92ef954cb62756cf8798174",
"structMayaFlux_1_1Core_1_1GlobalGraphicsConfig_a1e15dca6f860f2ea9b5eb612bfc9b8a9.html#a1e15dca6f860f2ea9b5eb612bfc9b8a9af78504d96ba7177dc0c6784905ac8743",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_a57b5633fc9e91c7a17c0c1879ed460a1.html#a57b5633fc9e91c7a17c0c1879ed460a1",
"structMayaFlux_1_1Core_1_1ShaderReflection_1_1SpecializationConstant_acb891d1f2633fe0f995e6b4d235b99d4.html#acb891d1f2633fe0f995e6b4d235b99d4",
"structMayaFlux_1_1Core_1_1WindowRenderContext_aae38d57e8d9a1aa0c08c587ea08d1409.html#aae38d57e8d9a1aa0c08c587ea08d1409",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure_acf719f2e5b2aec01f6216485a6292075.html#acf719f2e5b2aec01f6216485a6292075",
"structMayaFlux_1_1Kakshya_1_1RegionGroup_a8762865fdc9f10f87a68c9255cd1a3e6.html#a8762865fdc9f10f87a68c9255cd1a3e6",
"structMayaFlux_1_1Nodes_1_1Generator_1_1Logic_1_1LogicCallback_abd25abdba8c64ebe38c264ebdc824ec3.html#abd25abdba8c64ebe38c264ebdc824ec3",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderReflectionInfo_a5b27b04fd8179cc5541d0014ea4cbf2c.html#a5b27b04fd8179cc5541d0014ea4cbf2c",
"structMayaFlux_1_1Yantra_1_1DataStructureInfo_a3da38e27693ebe61e9f7d4814b25bb5c.html#a3da38e27693ebe61e9f7d4814b25bb5c",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01std_1_1vector_3_01Kakshya_1_1DataVariant_01_4_01_4_a56493b027ed79de2d79e0c2fb34f69f0.html#a56493b027ed79de2d79e0c2fb34f69f0"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';