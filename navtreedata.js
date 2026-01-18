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
        [ "New Possibilities", "md_README.html#autotoc_md47", null ]
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
      [ "Releases & Builds", "md_README.html#autotoc_md64", [
        [ "Stable Releases", "md_README.html#autotoc_md65", null ],
        [ "Development Builds", "md_README.html#autotoc_md66", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md68", [
        [ "Basic Application Structure", "md_README.html#autotoc_md69", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md71", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md73", [
        [ "Tutorials", "md_README.html#autotoc_md74", null ],
        [ "API Documentation", "md_README.html#autotoc_md75", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md77", null ],
      [ "Philosophy", "md_README.html#autotoc_md79", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md81", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md83", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md84", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md85", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md86", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md87", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md89", null ],
      [ "Contributing", "md_README.html#autotoc_md91", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md93", null ],
      [ "Contact", "md_README.html#autotoc_md95", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md100", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md102", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md104", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md107", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md109", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md111", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md113", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md120", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md121", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md122", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md123", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md124", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md125", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md126", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md127", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md129", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md130", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md131", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md132", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md133", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md134", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md135", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md136", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md137", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md138", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md140", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md141", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md142", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md143", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md144", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md145", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md146", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md148", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md152", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md154", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md156", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md161", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md166", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md172", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md193", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md194", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md195", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md198", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md199", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md200", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md201", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md202", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md203", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md204", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md205", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md206", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md207", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md208", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md209", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md210", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md211", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md212", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md213", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md214", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md215", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md216", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md217", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md218", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md219", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md220", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md221", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md222", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md266", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md268", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md269", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md271", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md273", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md275", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md277", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md278", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md279", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md280", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md281", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md282", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md284", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md286", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md288", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md290", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md291", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md292", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md293", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md294", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md295", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md296", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md297", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md298", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md299", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md300", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md301", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md302", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md304", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md306", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md308", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md310", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md312", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md314", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md316", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md318", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md320", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md322", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md324", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md325", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md326", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md327", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md328", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md329", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md330", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md333", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md335", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md337", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md342", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md343", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md344", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md345", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md346", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md352", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md354", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md358", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md379", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md383", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md386", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ]
        ] ],
        [ "StreamWriteProcessor (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", null ],
          [ "Expansion 1: What StreamWriteProcessor Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md419", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md430", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md431", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md432", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md433", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md434", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md435", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md438", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md439", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md440", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md441", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md442", null ],
        [ "Expansion 4: ContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md443", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md444", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md445", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md446", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md447", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md448", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md451", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md453", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md454", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ],
        [ "Expansion 3: StreamWriteProcessor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 4: FileBridgeBuffer—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md491", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md493", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md495", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md497", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md499", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md501", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md503", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md505", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md507", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md510", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md512", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md514", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md516", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md518", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md520", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md522", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md525", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md528", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md530", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md531", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md533", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md535", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md538", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md539", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md541", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md543", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md545", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md547", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md549", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md550", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md552", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md554", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md556", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md558", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md560", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md562", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md564", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md566", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md568", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md570", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md572", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md574", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md576", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md578", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md579", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md581", null ]
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
"Core_8hpp.html#a621f792c2b1f9e9d3f0d48c4cc7c2a4d",
"ExtractionHelper_8hpp.html#a13d0bda7f4723ea4150cffdf4f6395db",
"Graph_8hpp.html#afc136ade4d9c21adf7ee7477021dc972",
"MathematicalHelper_8hpp.html#ad24e6f405102277cf7fef7d823405ff6",
"Polynomial_8hpp_source.html",
"ShaderProcessor_8hpp.html",
"TemporalHelper_8hpp.html#a4a550da6039421755b9d9f71d0b08c99",
"VKContext_8hpp_source.html",
"Yantra_8hpp.html#ac4f678c4ec071b483a40fa991de2aae7",
"classLila_1_1Server_a08907163347d13e61086823eed2afdbe.html#a08907163347d13e61086823eed2afdbe",
"classMayaFlux_1_1Buffers_1_1BufferAccessControl_a9268e2e39a7d87b9be5156013aa50c06.html#a9268e2e39a7d87b9be5156013aa50c06",
"classMayaFlux_1_1Buffers_1_1BufferProcessingChain_adfe0c9fb06fd442b9302227a1b6aee04.html#adfe0c9fb06fd442b9302227a1b6aee04",
"classMayaFlux_1_1Buffers_1_1ComputeProcessor_aef0abe04f4545e1701e0e1c1273e61b2.html#aef0abe04f4545e1701e0e1c1273e61b2",
"classMayaFlux_1_1Buffers_1_1FileToStreamChain_a2d84a7b726db89e6efe3bd034938837f.html#a2d84a7b726db89e6efe3bd034938837f",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_af2d352ebb4234293f705505d622e297a.html#af2d352ebb4234293f705505d622e297a",
"classMayaFlux_1_1Buffers_1_1PolynomialProcessor_ad4f482a7f4e30f34b83c4ff1a362ca44.html#ad4f482a7f4e30f34b83c4ff1a362ca44",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor.html",
"classMayaFlux_1_1Buffers_1_1TextureProcessor_a7c8aaf1c16e1ed32a4900fb0e1d10d8f.html#a7c8aaf1c16e1ed32a4900fb0e1d10d8f",
"classMayaFlux_1_1Buffers_1_1VKBuffer_ab2774aad8e6ca3ee02ff9b31c2f8f6f7.html#ab2774aad8e6ca3ee02ff9b31c2f8f6f7",
"classMayaFlux_1_1Core_1_1BackendWindowHandler_a6471e3bb0fe7f5c466dc5f19317e617b.html#a6471e3bb0fe7f5c466dc5f19317e617b",
"classMayaFlux_1_1Core_1_1GlfwWindow_a29aa8a33db2378e0b37dcf270993b465.html#a29aa8a33db2378e0b37dcf270993b465",
"classMayaFlux_1_1Core_1_1NodeProcessingHandle_a9e484d01e9407b050b815bcf24cfc9f5.html#a9e484d01e9407b050b815bcf24cfc9f5",
"classMayaFlux_1_1Core_1_1VKCommandManager_ab25f2104b7f013fca0c7876ade0632b5.html#ab25f2104b7f013fca0c7876ade0632b5",
"classMayaFlux_1_1Core_1_1VKFramebuffer_a796e7daf158b82753ecab1edee18f8fc.html#a796e7daf158b82753ecab1edee18f8fc",
"classMayaFlux_1_1Core_1_1VKInstance_ab6d0d8e03bb2101a3e5203df7722def6.html#ab6d0d8e03bb2101a3e5203df7722def6",
"classMayaFlux_1_1Core_1_1VulkanBackend_ad77369bf508d5ea177c77a260b65b88d.html#ad77369bf508d5ea177c77a260b65b88d",
"classMayaFlux_1_1IO_1_1FileReader_a350b954b9faedc9790d4fbb5437baf1b.html#a350b954b9faedc9790d4fbb5437baf1b",
"classMayaFlux_1_1IO_1_1TextFileWriter_a4492eb0d2e462bffbadc5b06cec14e70.html#a4492eb0d2e462bffbadc5b06cec14e70",
"classMayaFlux_1_1Kakshya_1_1DataAccess_a346ecbe2653d09b44d3f255895a97dc8.html#a346ecbe2653d09b44d3f255895a97dc8",
"classMayaFlux_1_1Kakshya_1_1RegionCacheManager_a1af6a325f66ecd5fbb14d56ececdbf66.html#a1af6a325f66ecd5fbb14d56ececdbf66",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a25d78123b4268880b11261a0e0c38486.html#a25d78123b4268880b11261a0e0c38486",
"classMayaFlux_1_1Kakshya_1_1StructuredView_a16adafea2a7dd2bd43979df73d684789.html#a16adafea2a7dd2bd43979df73d684789",
"classMayaFlux_1_1Kriya_1_1BufferOperation_adbad4a55612f42b94f6ed892c61fd82b.html#adbad4a55612f42b94f6ed892c61fd82ba251d2fee497bff48dcf891d205ca7687",
"classMayaFlux_1_1Kriya_1_1EventChain.html",
"classMayaFlux_1_1Nodes_1_1Filters_1_1FilterContext_a47000ed38c09e22b48593dc6c31858ec.html#a47000ed38c09e22b48593dc6c31858ec",
"classMayaFlux_1_1Nodes_1_1Generator_1_1LogicContextGpu.html",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a834d85310c043984f899d9df9c3b596e.html#a834d85310c043984f899d9df9c3b596e",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Stochastics_1_1Random_a738597fad3af35f5638b445c35f02920.html#a738597fad3af35f5638b445c35f02920",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointNode_a01cfba43c61c7c92d1cb7074646bdfdf.html#a01cfba43c61c7c92d1cb7074646bdfdf",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a6866e1d1db1fdc0b568425026d9cbad4.html#a6866e1d1db1fdc0b568425026d9cbad4",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_a2e171aae726941cbe715d7d962f99b29.html#a2e171aae726941cbe715d7d962f99b29",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ComputePress.html",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a52651a5a78f0c50974d443de43c776ac.html#a52651a5a78f0c50974d443de43c776ac",
"classMayaFlux_1_1Vruta_1_1ComplexRoutine_a0611ee4adbe3d9312fdc10fb5a2c2569.html#a0611ee4adbe3d9312fdc10fb5a2c2569",
"classMayaFlux_1_1Vruta_1_1GraphicsRoutine_a081ac11bb5b1a90a8f0c62288ace4999.html#a081ac11bb5b1a90a8f0c62288ace4999",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a5f392865228c75f60264d38225a0eb35.html#a5f392865228c75f60264d38225a0eb35",
"classMayaFlux_1_1Yantra_1_1ComputeOperation_a1bf4adec60a307b8caefa39288624c94.html#a1bf4adec60a307b8caefa39288624c94",
"classMayaFlux_1_1Yantra_1_1FluentExecutor_a85d6aebe86dbe353f62471cfdab3c1a2.html#a85d6aebe86dbe353f62471cfdab3c1a2",
"classMayaFlux_1_1Yantra_1_1StandardSorter_a1187d263424d60d6834853aa7230e081.html#a1187d263424d60d6834853aa7230e081",
"classMayaFlux_1_1Yantra_1_1UniversalExtractor_a37ec9e24638713bb07502e60a415c9f1.html#a37ec9e24638713bb07502e60a415c9f1",
"conceptMayaFlux_1_1Yantra_1_1SingleVariant.html",
"md_docs_2Advanced__Context__Control.html#autotoc_md129",
"md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487",
"namespaceMayaFlux_1_1Journal_a4d1f572381818d4f67c4a5294d68ef2d.html#a4d1f572381818d4f67c4a5294d68ef2da03320356e0e42cde7ed8c21f4c2e3f54",
"namespaceMayaFlux_1_1Kriya_a8bb5778e118091b6eaf51c3b933d0f47.html#a8bb5778e118091b6eaf51c3b933d0f47",
"namespaceMayaFlux_1_1Utils_ab711cbb0bcdaf9905fd2352a41fa339e.html#ab711cbb0bcdaf9905fd2352a41fa339eae6b0e3f319580a049715e478c9d9f2a3",
"namespaceMayaFlux_1_1Yantra_a8b905b88af7094a01f5321ae2d5ac6df.html#a8b905b88af7094a01f5321ae2d5ac6df",
"namespaceMayaFlux_a19a1d605a77dab6ea22a44bc1f74c853.html#a19a1d605a77dab6ea22a44bc1f74c853",
"structLila_1_1ClangInterpreter_1_1Impl.html",
"structMayaFlux_1_1Buffers_1_1RootBuffer_1_1PendingBufferOp_aeaf9a84982fa6a434e89295df6fc9ccf.html#aeaf9a84982fa6a434e89295df6fc9ccf",
"structMayaFlux_1_1Core_1_1DeviceInfo_ad7d4405171a87ce12348413915333507.html#ad7d4405171a87ce12348413915333507",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_ad218407f7918f14af4f36ec496ff076c.html#ad218407f7918f14af4f36ec496ff076c",
"structMayaFlux_1_1Core_1_1PushConstantRange_a9effd251ee4147a4b0e2d7235805981f.html#a9effd251ee4147a4b0e2d7235805981f",
"structMayaFlux_1_1Core_1_1WindowEvent_1_1MouseButtonData_afdf44efb827358b4b98ff155016166ec.html#afdf44efb827358b4b98ff155016166ec",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure_a27b9d48a60ea2512b02a8196c6581eec.html#a27b9d48a60ea2512b02a8196c6581eec",
"structMayaFlux_1_1Kakshya_1_1RegionCache_a04d96ba1cb57d7c7b09e5e8432c411b5.html#a04d96ba1cb57d7c7b09e5e8432c411b5",
"structMayaFlux_1_1Kriya_1_1GetPromiseBase_ae3b6071159760d559653bb1f1b4b30eb.html#ae3b6071159760d559653bb1f1b4b30eb",
"structMayaFlux_1_1Portal_1_1Graphics_1_1SamplerConfig.html",
"structMayaFlux_1_1Yantra_1_1ChannelEnergy_a22d1d106693607b2e800d32256f0fd21.html#a22d1d106693607b2e800d32256f0fd21",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01Eigen_1_1MatrixXd_01_4.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';