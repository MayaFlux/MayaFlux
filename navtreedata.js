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
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
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
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md443", null ],
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
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
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
"Chronie_8cpp.html#a6613e51ffdbc148b6667699e873a87a6",
"Core_8cpp_source.html",
"ExtractionHelper_8cpp.html#a5ae82b069290bb66e406e61bb86bdd48",
"Graph_8hpp.html#ab62f030499d58eb3e1d5c43ca3bc230e",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62a08a38277b0309070706f6652eeae9a53",
"NDData_8hpp.html",
"Random_8cpp.html#a6d76c1657f091b72fe62aebe02ca575d",
"SortingHelper_8hpp.html#a745cab1ce5b2561ceff4e575927b038b",
"Timers_8cpp_ad6ac5bed3309925192fdb78ef9112425.html#ad6ac5bed3309925192fdb78ef9112425",
"VKShaderModule_8hpp.html#aa98c135cff2f0b2a5dfc0fc1e624681caaf9c7e51dce60522c00c0d4de81c59d8",
"classLila_1_1ClangInterpreter_a578389353fc3ed70dc467cd8024a5bde.html#a578389353fc3ed70dc467cd8024a5bde",
"classLila_1_1Server_aaee9cdd22cb0dd2000bfc1caf565c10f.html#aaee9cdd22cb0dd2000bfc1caf565c10f",
"classMayaFlux_1_1Buffers_1_1BufferDownloadProcessor_a7aad78ca7f1551f2410d9e4ebefb92c9.html#a7aad78ca7f1551f2410d9e4ebefb92c9",
"classMayaFlux_1_1Buffers_1_1BufferProcessingControl_a2bcd15e4f80d7423f57540057d0139f7.html#a2bcd15e4f80d7423f57540057d0139f7",
"classMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_a92545e0a03b7177493bf465caa550862.html#a92545e0a03b7177493bf465caa550862",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_a3728b3f430e1174bb249238eaf9f75bc.html#a3728b3f430e1174bb249238eaf9f75bca560a2dd6f6744646473b3b19e1fe96d7",
"classMayaFlux_1_1Buffers_1_1NodeTextureBuffer_aaad490cb1ac9dbd516f18a6ed5baa792.html#aaad490cb1ac9dbd516f18a6ed5baa792",
"classMayaFlux_1_1Buffers_1_1RootBuffer_a2b9197d91c068da2f1e55d7d64653562.html#a2b9197d91c068da2f1e55d7d64653562",
"classMayaFlux_1_1Buffers_1_1SoundFileBridge_ad0bedd820e321b8c8f046d5989ed0285.html#ad0bedd820e321b8c8f046d5989ed0285",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_ae6aa0b974f630058d58159a1b03560a2.html#ae6aa0b974f630058d58159a1b03560a2",
"classMayaFlux_1_1Core_1_1AudioStream_a075427086d28768680c2b9e6079d75b1.html#a075427086d28768680c2b9e6079d75b1",
"classMayaFlux_1_1Core_1_1BufferProcessingHandle_adfefad512d9b46de7d1ae113ff0b03a7.html#adfefad512d9b46de7d1ae113ff0b03a7",
"classMayaFlux_1_1Core_1_1GlfwWindow_af23e104e122650a5f215cfe716bc9672.html#af23e104e122650a5f215cfe716bc9672",
"classMayaFlux_1_1Core_1_1RtAudioSingleton_aacfb9f0e810fe2c73c01c30845fd6151.html#aacfb9f0e810fe2c73c01c30845fd6151",
"classMayaFlux_1_1Core_1_1VKComputePipeline_aea62756347fbde8e072b382fd5591c25.html#aea62756347fbde8e072b382fd5591c25",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a825fc45b4bd3403822ea95478132011b.html#a825fc45b4bd3403822ea95478132011b",
"classMayaFlux_1_1Core_1_1VKShaderModule_a470641752e868e1fa799222f8cfeda08.html#a470641752e868e1fa799222f8cfeda08",
"classMayaFlux_1_1Core_1_1WindowManager_a9b4fbb193419543cd5aeeee7a4cc349a.html#a9b4fbb193419543cd5aeeee7a4cc349a",
"classMayaFlux_1_1IO_1_1ImageReader.html",
"classMayaFlux_1_1Journal_1_1Archivist_1_1Impl_a8c61507390a6b5298e2b3c9c194a6502.html#a8c61507390a6b5298e2b3c9c194a6502",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_aef790a2bbfc0106263f72e4586ae85ba.html#aef790a2bbfc0106263f72e4586ae85ba",
"classMayaFlux_1_1Kakshya_1_1RegionOrganizationProcessor_a53ac8aa1aa08e4214a8f1783ef1e68d6.html#a53ac8aa1aa08e4214a8f1783ef1e68d6",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a76d848ea237359fc260a40a50a02d38a.html#a76d848ea237359fc260a40a50a02d38a",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a1a77d806a781a69b7f69d65804690092.html#a1a77d806a781a69b7f69d65804690092",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a20b39eda4e9d7481f01629f8875df49b.html#a20b39eda4e9d7481f01629f8875df49b",
"classMayaFlux_1_1Kriya_1_1NodeTimer_ac6c83d5e44961360315b7efae1ae35c1.html#ac6c83d5e44961360315b7efae1ae35c1",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_a9c4aa075101d6506781471513b6c0ce6.html#a9c4aa075101d6506781471513b6c0ce6",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a2ad9dc96e65ae4525fe659bf448f05c2.html#a2ad9dc96e65ae4525fe659bf448f05c2",
"classMayaFlux_1_1Nodes_1_1Generator_1_1PolynomialContext_a4e91c127dd8f537ebe4542ece02e5b91.html#a4e91c127dd8f537ebe4542ece02e5b91",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Stochastics_1_1Random_acab6076c17b9012acae7a28f0e0411b2.html#acab6076c17b9012acae7a28f0e0411b2",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointNode_ae31361e8430da0e07e295c33704133f9.html#ae31361e8430da0e07e295c33704133f9",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a97e8cd5641d65db8b91b7c13de3666b1.html#a97e8cd5641d65db8b91b7c13de3666b1",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_a5b7627b25ad4de317b0c6ca93d1001c9.html#a5b7627b25ad4de317b0c6ca93d1001c9",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ComputePress_a6948fbfdc469b8371205414a311c02a7.html#a6948fbfdc469b8371205414a311c02a7",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a6df777498d5e24f1ac9017fa37423480.html#a6df777498d5e24f1ac9017fa37423480",
"classMayaFlux_1_1Vruta_1_1ComplexRoutine_a79c3484eed3452d156ebc50a6864d974.html#a79c3484eed3452d156ebc50a6864d974",
"classMayaFlux_1_1Vruta_1_1GraphicsRoutine_a0d2ae26f316816e50fdd9640098f599b.html#a0d2ae26f316816e50fdd9640098f599b",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a69aa9210925f6d7a5d075e5887f4faaa.html#a69aa9210925f6d7a5d075e5887f4faaa",
"classMayaFlux_1_1Yantra_1_1ComputeOperation_a235132f2f294a5b30c4ed85ce8c802de.html#a235132f2f294a5b30c4ed85ce8c802de",
"classMayaFlux_1_1Yantra_1_1FluentExecutor_a91b36ee1825d13afe6af26a2cfb3e1de.html#a91b36ee1825d13afe6af26a2cfb3e1de",
"classMayaFlux_1_1Yantra_1_1StandardSorter_a23b5d6fc1ca885f8971ee0cf3b30b8fb.html#a23b5d6fc1ca885f8971ee0cf3b30b8fb",
"classMayaFlux_1_1Yantra_1_1UniversalExtractor_a49c4f4d458f76ad338d3fdf07aadf670.html#a49c4f4d458f76ad338d3fdf07aadf670",
"deprecated.html",
"md_docs_2Advanced__Context__Control.html#autotoc_md133",
"md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md493",
"namespaceMayaFlux_1_1IO_a36a3852542b551e90c97c41c9ed56f62.html#a36a3852542b551e90c97c41c9ed56f62a692ed49ff2182a144044181274242db5",
"namespaceMayaFlux_1_1Kakshya_a4b936549737a580757a43c3a34b11787.html#a4b936549737a580757a43c3a34b11787",
"namespaceMayaFlux_1_1Parallel_a5da0996dba37190dffba9510d880a0c9.html#a5da0996dba37190dffba9510d880a0c9",
"namespaceMayaFlux_1_1Yantra_a1b9e1e32a6d52cccf873bee90c60aa5c.html#a1b9e1e32a6d52cccf873bee90c60aa5ca6b2117c9c0f9081cadb3cf1f5875e49d",
"namespaceMayaFlux_1_1Yantra_ac646e918f1e6a6e257ec45eacdff6be0.html#ac646e918f1e6a6e257ec45eacdff6be0",
"namespaceMayaFlux_a94ffbf5017cec2f2de704361b8b5ad4b.html#a94ffbf5017cec2f2de704361b8b5ad4b",
"structMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_1_1DescriptorBinding_acc505905e0504d3c5713ddc727fa80ca.html#acc505905e0504d3c5713ddc727fa80ca",
"structMayaFlux_1_1Buffers_1_1ShaderDispatchConfig_aed3ef668d4328ffe9a7d9d4b75e957c4.html#aed3ef668d4328ffe9a7d9d4b75e957c4",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_1_1ChannelConfig.html",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_a995a1a81124b8c709216af57641d2edf.html#a995a1a81124b8c709216af57641d2edf",
"structMayaFlux_1_1Core_1_1SubpassDescription_a2b6eed281713e0fe25706f823be43c7d.html#a2b6eed281713e0fe25706f823be43c7d",
"structMayaFlux_1_1Core_1_1WindowState_a650409b7a4db32d40b5afceb78481ca5.html#a650409b7a4db32d40b5afceb78481ca5",
"structMayaFlux_1_1Kakshya_1_1DataConverter_3_01From_00_01To_00_01std_1_1enable__if__t_3_01GlmTyp4ac36b01ee43908ee32d3d24a3bd64ad.html",
"structMayaFlux_1_1Kakshya_1_1RegionSegment_a3ff9ac9f55d21c38f0499f75197998ad.html#a3ff9ac9f55d21c38f0499f75197998ad",
"structMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_1_1ModalNode_a73d85c951516c061de38bcb5e45a3b7e.html#a73d85c951516c061de38bcb5e45a3b7e",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderReflectionInfo_aa8a17b23844ae507a74e03faf92afad3.html#aa8a17b23844ae507a74e03faf92afad3",
"structMayaFlux_1_1Yantra_1_1ComputationGrammar_1_1Rule_a9e84d8d747ef1c5ec91abfcd80eb97de.html#a9e84d8d747ef1c5ec91abfcd80eb97de",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01std_1_1shared__ptr_3_01Kakshya_1_1SignalSourceContainer_01_4_01_4_ab8c1833ef416e38582a7280c3e1d7b8e.html#ab8c1833ef416e38582a7280c3e1d7b8e"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';