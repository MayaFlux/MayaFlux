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
      [ "The Paradigm", "md_README.html#autotoc_md50", [
        [ "New Possibilities", "md_README.html#autotoc_md51", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md53", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md55", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md56", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md57", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md58", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md60", [
        [ "Management Mode", "md_README.html#autotoc_md61", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md62", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md63", [
        [ "Requirements", "md_README.html#autotoc_md64", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md65", null ],
        [ "Build", "md_README.html#autotoc_md66", null ]
      ] ],
      [ "Releases & Builds", "md_README.html#autotoc_md68", [
        [ "Stable Releases", "md_README.html#autotoc_md69", null ],
        [ "Development Builds", "md_README.html#autotoc_md70", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md72", [
        [ "Basic Application Structure", "md_README.html#autotoc_md73", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md75", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md77", [
        [ "Tutorials", "md_README.html#autotoc_md78", null ],
        [ "API Documentation", "md_README.html#autotoc_md79", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md81", null ],
      [ "Philosophy", "md_README.html#autotoc_md83", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md85", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md87", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md88", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md89", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md90", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md91", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md93", null ],
      [ "Contributing", "md_README.html#autotoc_md95", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md97", null ],
      [ "Contact", "md_README.html#autotoc_md99", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md104", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md106", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md108", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md111", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md113", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md115", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md117", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md124", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md125", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md126", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md127", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md129", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md130", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md131", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md132", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md133", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md134", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md135", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md136", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md137", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md138", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md140", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md141", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md142", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md143", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md144", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md145", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md146", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md148", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md149", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md150", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md151", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md152", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md156", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md158", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md160", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md165", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md170", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md193", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md194", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md195", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md197", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md198", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md199", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md200", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md202", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md203", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md204", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md205", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md206", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md207", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md208", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md209", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md210", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md211", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md212", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md213", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md214", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md215", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md216", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md217", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md218", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md219", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md220", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md221", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md222", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md223", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md224", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md225", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md226", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md270", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md272", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md273", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md275", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md277", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md279", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md281", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md282", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md283", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md284", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md285", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md286", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md288", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md290", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md292", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md294", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md295", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md296", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md297", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md298", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md299", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md300", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md301", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md302", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md303", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md304", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md305", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md306", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md308", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md310", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md312", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md314", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md316", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md318", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md320", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md322", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md324", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md326", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md328", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md329", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md330", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md331", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md332", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md333", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md334", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md337", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md339", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md341", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md342", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md344", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md346", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md348", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md350", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md358", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md375", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md383", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ]
        ] ],
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md409", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md419", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md426", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md429", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md434", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md435", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md436", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md437", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md438", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md439", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md442", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md443", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md444", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md445", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md446", null ],
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md447", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md448", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md450", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md451", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md453", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md490", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md492", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md495", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md497", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md499", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md501", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md503", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md505", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md507", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md509", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md511", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md514", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md516", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md518", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md520", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md522", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md524", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md526", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md529", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md532", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md534", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md535", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md537", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md539", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md542", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md543", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md545", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md547", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md549", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md551", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md553", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md554", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md556", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md558", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md560", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md562", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md564", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md566", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md568", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md570", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md572", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md574", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md576", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md578", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md580", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md582", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md583", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md585", null ]
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
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"API_2Random_8cpp.html",
"Capture_8hpp.html#a8fbeb523647cb5063bcf61b85ef689e0",
"CoordUtils_8hpp.html#a36e1afd2addb0e5dcfa74475079d23a9",
"EnergyAnalyzer_8hpp_source.html",
"GrammarHelper_8hpp_source.html",
"InputNode_8hpp.html#a5476952510ae5458ee4906bb9674c593a1de1c752385bf565da9d1ca8e85b58f0",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62abec20cee5d032151c66d7303207f2f00",
"NDData_8hpp.html#aec5fc7fe96708768600b4ce8feaba1b8a139882c654db8a57f7c3092de1dd0b02",
"RegionProcessors_8cpp.html#a4f4d80add8013fb73fabc262edc2e76c",
"SortingHelper_8hpp.html#ac878b27376ae997a32dc3ce40f47f87a",
"TokenUnitManager_8cpp.html",
"Yantra_8cpp.html#a11837edee4362ad1cf81299f7a839903",
"classLila_1_1Commentator_a496dffe59add00a426b994e300f42305.html#a496dffe59add00a426b994e300f42305",
"classLila_aa218326c1af159c102fd5131b4d14abc.html#aa218326c1af159c102fd5131b4d14abca84ae7f7740714c338ba1c85408165b13",
"classMayaFlux_1_1Buffers_1_1BufferManager_a3b78d0d5c885e6056e59bfd10a984005.html#a3b78d0d5c885e6056e59bfd10a984005",
"classMayaFlux_1_1Buffers_1_1BufferProcessor_a8d78744c6eeea932aceea31ea7bf2559.html#a8d78744c6eeea932aceea31ea7bf2559",
"classMayaFlux_1_1Buffers_1_1FeedbackBuffer_afbc4235a503fc2e2224eb0c73aa87e94.html#afbc4235a503fc2e2224eb0c73aa87e94",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_ac97775675f073d414149359523eb9251.html#ac97775675f073d414149359523eb9251",
"classMayaFlux_1_1Buffers_1_1PolynomialProcessor_a5d2bcf2e269566038dda3c1c4ca3c407.html#a5d2bcf2e269566038dda3c1c4ca3c407",
"classMayaFlux_1_1Buffers_1_1RootGraphicsBuffer_a4f306655233db6cdd35252d292bf3612.html#a4f306655233db6cdd35252d292bf3612",
"classMayaFlux_1_1Buffers_1_1SoundStreamReader_afecfefb4fcfba9cc4ca5ada0bca2175e.html#afecfefb4fcfba9cc4ca5ada0bca2175e",
"classMayaFlux_1_1Buffers_1_1VKBuffer_a0774cbe58037b8f7d18e00b6e962901b.html#a0774cbe58037b8f7d18e00b6e962901b",
"classMayaFlux_1_1Core_1_1AudioSubsystem_a7da101d6515b09830cb1d0ac7488bde5.html#a7da101d6515b09830cb1d0ac7488bde5",
"classMayaFlux_1_1Core_1_1Engine_a4650c886dbf271daca7138c65a83dfea.html#a4650c886dbf271daca7138c65a83dfea",
"classMayaFlux_1_1Core_1_1GraphicsSubsystem_a72e293bc08a7ad852976e5301d241c4e.html#a72e293bc08a7ad852976e5301d241c4e",
"classMayaFlux_1_1Core_1_1ISubsystem_a589aa5db79b7340abc015868fefe2e11.html#a589aa5db79b7340abc015868fefe2e11",
"classMayaFlux_1_1Core_1_1MIDIBackend_ae65ccd2b1f8b23bf1ab381d0f915f250.html#ae65ccd2b1f8b23bf1ab381d0f915f250",
"classMayaFlux_1_1Core_1_1TaskSchedulerHandle_a9f45f113913ace26b438be91b367f66e.html#a9f45f113913ace26b438be91b367f66e",
"classMayaFlux_1_1Core_1_1VKDevice_ae7b2045b61ea20c92692ce2b27775a24.html#ae7b2045b61ea20c92692ce2b27775a24",
"classMayaFlux_1_1Core_1_1VKImage_ac7b6bbe9c47c4602b3f0f97c44b3eca4.html#ac7b6bbe9c47c4602b3f0f97c44b3eca4",
"classMayaFlux_1_1Core_1_1VulkanBackend_a49e8d16926263842b7af135559a07604.html#a49e8d16926263842b7af135559a07604",
"classMayaFlux_1_1Creator_aa7e28aa8d9ac8e457ca89e2d82a98dc1.html#aa7e28aa8d9ac8e457ca89e2d82a98dc1",
"classMayaFlux_1_1IO_1_1SoundFileReader_ada623835764ecaf37aa2c437d2b2f3c7.html#ada623835764ecaf37aa2c437d2b2f3c7",
"classMayaFlux_1_1Kakshya_1_1ContiguousAccessProcessor_aca05ee63009dd88d08bdb79c0fa821c0.html#aca05ee63009dd88d08bdb79c0fa821c0",
"classMayaFlux_1_1Kakshya_1_1EigenInsertion_afae4fdb884bcfc31ef058a55e18ab229.html#afae4fdb884bcfc31ef058a55e18ab229",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_acea3f1f9946a902554afa599cac9f9ca.html#acea3f1f9946a902554afa599cac9f9ca",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_af08433b25b9cf03f4fc50d26aecb45e5.html#af08433b25b9cf03f4fc50d26aecb45e5",
"classMayaFlux_1_1Kriya_1_1BufferCapture_a826da8a14d2df88090b5c2dd94ce423e.html#a826da8a14d2df88090b5c2dd94ce423e",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_a8cf80f997179efca17af3f1087574729.html#a8cf80f997179efca17af3f1087574729",
"classMayaFlux_1_1Kriya_1_1Timer_a6ade48631a3e9acaf70c569bf88599a4.html#a6ade48631a3e9acaf70c569bf88599a4",
"classMayaFlux_1_1Nodes_1_1Filters_1_1FilterContext.html",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Impulse_ae7791eb89892ef854a120c4b2c65a997.html#ae7791eb89892ef854a120c4b2c65a997",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor_a40e745678101c07b9b4987d908fbb288.html#a40e745678101c07b9b4987d908fbb288",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Random_afb32a6df285c47187063ac2b6a1ecc8b.html#afb32a6df285c47187063ac2b6a1ecc8b",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointCollectionNode_a45ab3825f6c80eb1cb5a16c775f31672.html#a45ab3825f6c80eb1cb5a16c775f31672",
"classMayaFlux_1_1Nodes_1_1Input_1_1MIDINode_a53e51aaf8495bc2e1e77e733d3d49f23.html#a53e51aaf8495bc2e1e77e733d3d49f23",
"classMayaFlux_1_1Nodes_1_1Network_1_1ParticleNetwork_a1a86e83715728f151002a83f4ca8a53c.html#a1a86e83715728f151002a83f4ca8a53c",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_ae9e7dbb2ac51c04faea14dbbb14d7544.html#ae9e7dbb2ac51c04faea14dbbb14d7544",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a5caaefa7ac32568cf867e030ad65a48e.html#a5caaefa7ac32568cf867e030ad65a48e",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_abfa6ba441dace18abd157505339ff3a8.html#abfa6ba441dace18abd157505339ff3a8",
"classMayaFlux_1_1Vruta_1_1EventManager_a57dfab40ee4b14db2bef10e6c4c5e9d2.html#a57dfab40ee4b14db2bef10e6c4c5e9d2",
"classMayaFlux_1_1Vruta_1_1IClock_adea4b742b63f4f0c4fb0ef6df5bff53b.html#adea4b742b63f4f0c4fb0ef6df5bff53b",
"classMayaFlux_1_1Yantra_1_1ComputationGrammar_1_1RuleBuilder.html",
"classMayaFlux_1_1Yantra_1_1ConvolutionTransformer.html",
"classMayaFlux_1_1Yantra_1_1OpUnit.html",
"classMayaFlux_1_1Yantra_1_1StatisticalAnalyzer_a52b9b65701c1f40bc817c0ba44790c0e.html#a52b9b65701c1f40bc817c0ba44790c0e",
"classMayaFlux_1_1Yantra_1_1UniversalSorter_a0fe08f7b04f8e0d423bd6210a5177a84.html#a0fe08f7b04f8e0d423bd6210a5177a84",
"dir_efaf62d53fb55b40a234ff4e6ec638ef.html",
"md_docs_2Domain__and__Control.html#autotoc_md203",
"namespaceLila_1_1Colors_a6448439a870f18c7d9c28b11b78e0948.html#a6448439a870f18c7d9c28b11b78e0948",
"namespaceMayaFlux_1_1IO_a36a3852542b551e90c97c41c9ed56f62.html#a36a3852542b551e90c97c41c9ed56f62ab213ce22ca6ad4eda8db82966b9b6e5a",
"namespaceMayaFlux_1_1Kakshya_a6e689a97fa5c40e151a3d5f30b8ed33b.html#a6e689a97fa5c40e151a3d5f30b8ed33b",
"namespaceMayaFlux_1_1Nodes_1_1Input_a5476952510ae5458ee4906bb9674c593.html#a5476952510ae5458ee4906bb9674c593a1de1c752385bf565da9d1ca8e85b58f0",
"namespaceMayaFlux_1_1Yantra_a0d3f7904d87cfd07617fa47c1f6281ad.html#a0d3f7904d87cfd07617fa47c1f6281adacc5eac6d4ce84ae96a802a5ecdd51977",
"namespaceMayaFlux_1_1Yantra_aad2895941e12cdd28c214ba4fe0a6de3.html#aad2895941e12cdd28c214ba4fe0a6de3aa7f142a76d280360fb9663dcdfa7ad65",
"namespaceMayaFlux_a6f4fd0800d812b015081570529cc5a30.html#a6f4fd0800d812b015081570529cc5a30",
"structLila_1_1StreamEvent_a1254a7a47e4300861b9089d7169f4c07.html#a1254a7a47e4300861b9089d7169f4c07",
"structMayaFlux_1_1Buffers_1_1ShaderBinding_abf06d61600dc5e7cd2c772ea2585a114.html#abf06d61600dc5e7cd2c772ea2585a114",
"structMayaFlux_1_1Core_1_1GlobalGraphicsConfig_a1e15dca6f860f2ea9b5eb612bfc9b8a9.html#a1e15dca6f860f2ea9b5eb612bfc9b8a9aa203d5f2c811f85341bed32623f5bdf6",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_a1eef8df7749be2d408e93bb8355b2de5.html#a1eef8df7749be2d408e93bb8355b2de5",
"structMayaFlux_1_1Core_1_1InputBinding_a005fa6bb0d79e0b4136f705e86a6b8d1.html#a005fa6bb0d79e0b4136f705e86a6b8d1",
"structMayaFlux_1_1Core_1_1PushConstantRange_a433b8cb4e1fcb98e0a8e308a61126e60.html#a433b8cb4e1fcb98e0a8e308a61126e60",
"structMayaFlux_1_1Core_1_1WindowCreateInfo_aa1c0c3f06b26c3ca37bb464f5e3f88d4.html#aa1c0c3f06b26c3ca37bb464f5e3f88d4",
"structMayaFlux_1_1Journal_1_1RealtimeEntry_a87c0eacdc861f915a3ea679ddca30f65.html#a87c0eacdc861f915a3ea679ddca30f65",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a54f4df85bbfd431ffdcc0c28029d541e.html#a54f4df85bbfd431ffdcc0c28029d541e",
"structMayaFlux_1_1Kinesis_1_1Stochastic_1_1GeneratorState_ae48f7f34c8e1cd9b50097f041ae611ff.html#ae48f7f34c8e1cd9b50097f041ae611ff",
"structMayaFlux_1_1Nodes_1_1Input_1_1InputConfig.html",
"structMayaFlux_1_1Portal_1_1Graphics_1_1SamplerConfig_a3762ed1679d882b157e4cc6a3e20bc11.html#a3762ed1679d882b157e4cc6a3e20bc11",
"structMayaFlux_1_1Vruta_1_1routine__promise_aa0d0ede0be988b9fb00071d1d3a9c4d1.html#aa0d0ede0be988b9fb00071d1d3a9c4d1",
"structMayaFlux_1_1Yantra_1_1TransformationKey_a188a2d1e5e6fe4f50c194d4f79e42773.html#a188a2d1e5e6fe4f50c194d4f79e42773"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';