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
      [ "The Paradigm", "md_README.html#autotoc_md47", [
        [ "New Possibilities", "md_README.html#autotoc_md48", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md50", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md52", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md53", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md54", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md55", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md57", [
        [ "Management Mode", "md_README.html#autotoc_md58", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md59", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md60", [
        [ "Requirements", "md_README.html#autotoc_md61", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md62", null ],
        [ "Build", "md_README.html#autotoc_md63", null ]
      ] ],
      [ "Releases & Builds", "md_README.html#autotoc_md65", [
        [ "Stable Releases", "md_README.html#autotoc_md66", null ],
        [ "Development Builds", "md_README.html#autotoc_md67", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md69", [
        [ "Basic Application Structure", "md_README.html#autotoc_md70", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md72", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md74", [
        [ "Tutorials", "md_README.html#autotoc_md75", null ],
        [ "API Documentation", "md_README.html#autotoc_md76", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md78", null ],
      [ "Philosophy", "md_README.html#autotoc_md80", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md82", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md84", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md85", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md86", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md87", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md88", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md90", null ],
      [ "Contributing", "md_README.html#autotoc_md92", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md94", null ],
      [ "Contact", "md_README.html#autotoc_md96", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md101", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md103", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md105", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md108", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md110", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md112", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md114", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md121", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md122", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md123", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md124", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md125", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md126", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md127", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md128", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md129", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md130", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md131", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md132", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md133", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md134", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md135", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md136", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md137", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md138", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md140", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md141", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md142", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md143", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md144", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md145", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md146", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md148", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md149", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md153", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md155", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md157", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md162", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md167", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md193", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md194", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md195", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md197", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md199", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md200", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md201", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md202", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md203", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md204", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md205", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md206", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md207", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md208", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md209", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md210", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md211", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md212", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md213", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md214", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md215", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md216", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md217", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md218", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md219", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md220", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md221", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md222", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md223", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md267", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md269", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md270", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md272", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md274", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md276", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md278", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md279", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md280", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md281", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md282", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md283", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md285", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md287", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md289", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md291", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md292", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md293", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md294", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md295", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md296", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md297", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md298", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md299", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md300", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md301", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md302", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md303", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md305", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md307", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md309", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md311", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md313", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md315", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md317", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md319", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md321", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md323", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md325", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md326", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md327", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md328", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md329", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md330", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md331", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md335", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md337", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md339", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md341", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md343", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md344", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md345", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md346", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md348", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md350", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md352", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md354", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md375", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md379", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md386", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", null ]
        ] ],
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md409", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md426", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md428", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md431", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md432", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md433", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md434", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md435", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md436", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md439", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md440", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md441", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md442", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md443", null ],
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md444", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md445", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md446", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md447", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md448", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md450", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md454", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md492", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md494", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md496", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md498", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md500", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md502", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md504", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md506", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md508", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md511", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md513", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md515", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md517", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md519", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md521", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md523", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md526", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md529", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md531", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md532", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md534", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md536", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md539", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md540", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md542", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md544", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md546", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md548", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md550", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md551", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md553", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md555", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md557", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md559", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md561", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md563", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md565", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md567", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md569", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md571", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md573", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md575", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md577", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md579", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md580", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md582", null ]
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
"AggregateBindingsProcessor_8cpp.html",
"Chronie_8cpp.html#a6613e51ffdbc148b6667699e873a87a6",
"Core_8cpp.html#afe8d554b31c372f2e0df38b03b51569a",
"ExtractionHelper_8cpp.html#a5ae82b069290bb66e406e61bb86bdd48",
"Graph_8hpp.html#aad56cf1420bfb5ad395d77cfed879312",
"JournalEntry_8hpp.html#a4d1f572381818d4f67c4a5294d68ef2daacf504f3a4b8247f31b947647c140e4e",
"Logic_8hpp.html#aeb69bd502d2282932915b16e9c5790cfa68296ea3aabdfaee4e684f70b3904bf2",
"Parallel_8hpp.html#af10b8d4246c19ed17f8b9903ff57df14",
"RtAudioBackend_8hpp.html#a133ce155821da146bbc8f75e898e8b71",
"StructureIntrospection_8hpp.html",
"Utils_8hpp.html#a8a8d84b43dd1afb431a4414a393765c7ac56ffc065551798a6cc649edad18019b",
"Yantra_8hpp.html#a41c30fc23eaa471e7958c6953f55ded7",
"classLila_1_1Lila_af35871b7845a2c88bdcdd1249bb72d90.html#af35871b7845a2c88bdcdd1249bb72d90",
"classMayaFlux_1_1Buffers_1_1AudioBuffer_ae08cacf5784b9e80414070807692e9da.html#ae08cacf5784b9e80414070807692e9da",
"classMayaFlux_1_1Buffers_1_1BufferProcessingChain_a2f00960367f66dd400aa103d947fb89c.html#a2f00960367f66dd400aa103d947fb89c",
"classMayaFlux_1_1Buffers_1_1Buffer_a554b7208fe7b1e988b80f8f8ca92f2fb.html#a554b7208fe7b1e988b80f8f8ca92f2fb",
"classMayaFlux_1_1Buffers_1_1GeometryBuffer_a958882d2b9507c346b857bb07eb974e3.html#a958882d2b9507c346b857bb07eb974e3",
"classMayaFlux_1_1Buffers_1_1NodeBindingsProcessor_abfa6c720af76cca9809c43f112b820ab.html#abfa6c720af76cca9809c43f112b820aba182fa1c42a2468f8488e6dcf75a81b81",
"classMayaFlux_1_1Buffers_1_1RenderProcessor_a48a13b34e7cf44301f4df7e8b124eb78.html#a48a13b34e7cf44301f4df7e8b124eb78",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a9f7e60949fa728a6ddf736d32aa3908f.html#a9f7e60949fa728a6ddf736d32aa3908f",
"classMayaFlux_1_1Buffers_1_1TextureBuffer_afbcd8d601e79abe8ee197b3fd2ab5c0c.html#afbcd8d601e79abe8ee197b3fd2ab5c0c",
"classMayaFlux_1_1Buffers_1_1VKBuffer_aa35a413bdfd0243eb48257069be7298e.html#aa35a413bdfd0243eb48257069be7298e",
"classMayaFlux_1_1Core_1_1BackendResourceManager_ae6ec7e3044b266267de2e7fcb3c0f2e4.html#ae6ec7e3044b266267de2e7fcb3c0f2e4",
"classMayaFlux_1_1Core_1_1GlfwWindow_a07e6312b33feec08b880a876cabd160b.html#a07e6312b33feec08b880a876cabd160b",
"classMayaFlux_1_1Core_1_1HIDBackend_aa8bf4dd6b1508f3157ac522dcab69dd8.html#aa8bf4dd6b1508f3157ac522dcab69dd8",
"classMayaFlux_1_1Core_1_1InputSubsystem_a29786c020cfea01d78b35e6ef0c01e32.html#a29786c020cfea01d78b35e6ef0c01e32",
"classMayaFlux_1_1Core_1_1SubsystemManager_ac4e9890c907efa3a5e1f77187424f29a.html#ac4e9890c907efa3a5e1f77187424f29a",
"classMayaFlux_1_1Core_1_1VKDescriptorManager_ab4195ea24953753de59b3be3b3463cd7.html#ab4195ea24953753de59b3be3b3463cd7",
"classMayaFlux_1_1Core_1_1VKImage_a603d6ef31c19453be73d8a72929184ef.html#a603d6ef31c19453be73d8a72929184efacc301ce75e16247f3b96a3907519096c",
"classMayaFlux_1_1Core_1_1VKSwapchain_a4cbb6f79982d6870316d521aa757d0c3.html#a4cbb6f79982d6870316d521aa757d0c3",
"classMayaFlux_1_1CreationHandle_a45da0a6e9d203f793042e71a198fe7e5.html#a45da0a6e9d203f793042e71a198fe7e5",
"classMayaFlux_1_1IO_1_1SoundFileReader_a5c2c3602412fdcd3e2d25e5a6be2cbd8.html#a5c2c3602412fdcd3e2d25e5a6be2cbd8",
"classMayaFlux_1_1Journal_1_1FileSink_abe478abbcd776700d687709fc9c19330.html#abe478abbcd776700d687709fc9c19330",
"classMayaFlux_1_1Kakshya_1_1DynamicSoundStream_ab68f447003a3786f4d2add7eb52cd9c3.html#ab68f447003a3786f4d2add7eb52cd9c3",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_a9d6a96b8855c6bf617063a76a4b8b3db.html#a9d6a96b8855c6bf617063a76a4b8b3db",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_adb4e5483ef726cc0d5a809784cac1d83.html#adb4e5483ef726cc0d5a809784cac1d83",
"classMayaFlux_1_1Kriya_1_1BufferOperation_a133901488087bcc70288dda0f1613226.html#a133901488087bcc70288dda0f1613226",
"classMayaFlux_1_1Kriya_1_1BufferPipeline_afb70039e4177853435d7ac42b28c7f7b.html#afb70039e4177853435d7ac42b28c7f7b",
"classMayaFlux_1_1Nodes_1_1BinaryOpNode_a5b414570d2c7d97ac8b3ccc5dae7c3cd.html#a5b414570d2c7d97ac8b3ccc5dae7c3cd",
"classMayaFlux_1_1Nodes_1_1Generator_1_1GeneratorContext_a9348ff07f344c169e526015e51797c08.html#a9348ff07f344c169e526015e51797c08",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a984849d53ffbc5b5ba3b862ca46f1ade.html#a984849d53ffbc5b5ba3b862ca46f1ade",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_a90706cb654cdf29abe842d2703241f23.html#a90706cb654cdf29abe842d2703241f23",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1ComputeOutNode_a5d8ad6909bbaf573b168ea937b227aa0.html#a5d8ad6909bbaf573b168ea937b227aa0",
"classMayaFlux_1_1Nodes_1_1GpuVectorData.html",
"classMayaFlux_1_1Nodes_1_1Network_1_1NodeNetwork_a55a436b490c8a7cde7e40b54ade903cc.html#a55a436b490c8a7cde7e40b54ade903cc",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_a0f644abe23eb4531176982e8144fc3e5.html#a0f644abe23eb4531176982e8144fc3e5",
"classMayaFlux_1_1Nodes_1_1RootNode_ac034bc67c52ebeca67545a3837745933.html#ac034bc67c52ebeca67545a3837745933",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a426980f3654cc944518b5746d88fef9d.html#a426980f3654cc944518b5746d88fef9d",
"classMayaFlux_1_1Registry_1_1BackendRegistry_a864cf8ecca6a3313428533c157ba478f.html#a864cf8ecca6a3313428533c157ba478f",
"classMayaFlux_1_1Vruta_1_1FrameClock_a9799ba4d807597f7603450273d8e0a16.html#a9799ba4d807597f7603450273d8e0a16",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a03178adf3c6e2f64af520f0be66b75cc.html#a03178adf3c6e2f64af520f0be66b75cc",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_aae7111e8ba2a296efb036acfb83351d2.html#aae7111e8ba2a296efb036acfb83351d2",
"classMayaFlux_1_1Yantra_1_1FluentExecutor_a0796ef94da593f1600dff0d1420faf2f.html#a0796ef94da593f1600dff0d1420faf2f",
"classMayaFlux_1_1Yantra_1_1SpectralTransformer_a42506575dea94bdb51d75731fe1fbdcb.html#a42506575dea94bdb51d75731fe1fbdcb",
"classMayaFlux_1_1Yantra_1_1UniversalAnalyzer_ab9a3fc19924f161a4371c3aec5c2f3fb.html#ab9a3fc19924f161a4371c3aec5c2f3fb",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_ae86a6743c591b4585419dbbd8698c4b4.html#ae86a6743c591b4585419dbbd8698c4b4",
"md_README.html#autotoc_md61",
"md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md448",
"namespaceMayaFlux_1_1IO_a26e87bc07718489a8bb883da3bd4216e.html#a26e87bc07718489a8bb883da3bd4216ea91ff24acfc01cc2c3a4238a272a37d07",
"namespaceMayaFlux_1_1Journal_ad918cd0db16e3f2184007817220b837f.html#ad918cd0db16e3f2184007817220b837fa2e40ad879e955201df4dedbf8d479a12",
"namespaceMayaFlux_1_1Nodes_1_1Generator_a9675cd11407831a81787b92a2107a08c.html#a9675cd11407831a81787b92a2107a08ca6685384d880fca387e09391a6e53e3a8",
"namespaceMayaFlux_1_1Vruta_a77f95e569fd1fad7cb269c075de408c8.html#a77f95e569fd1fad7cb269c075de408c8acd6176dbac3d74dfa5c5fc63aa57316e",
"namespaceMayaFlux_1_1Yantra_a97626154364c72235ed716bed2c5a50d.html#a97626154364c72235ed716bed2c5a50d",
"namespaceMayaFlux_a3b8f6b534c503df4c4a2b106a64a65fe.html#a3b8f6b534c503df4c4a2b106a64a65fe",
"structLila_1_1ClangInterpreter_1_1Impl_ae02cb12deb2fbf34bd2e49f50ec336df.html#ae02cb12deb2fbf34bd2e49f50ec336df",
"structMayaFlux_1_1Buffers_1_1RootGraphicsBuffer_1_1RenderableBufferInfo_a6d44ea28bdc0412aea55784f93774903.html#a6d44ea28bdc0412aea55784f93774903",
"structMayaFlux_1_1Core_1_1FragmentOutputInfo.html",
"structMayaFlux_1_1Core_1_1GraphicsBackendInfo_a903bf8d122747ddb29e8f123ad05e922.html#a903bf8d122747ddb29e8f123ad05e922",
"structMayaFlux_1_1Core_1_1HIDDeviceFilter.html",
"structMayaFlux_1_1Core_1_1OSCConfig_aa22c1b588d5c815a307e51a03a7ff786.html#aa22c1b588d5c815a307e51a03a7ff786",
"structMayaFlux_1_1Core_1_1WindowCreateInfo_a17a57485f7b51b56a88594490b20cc68.html#a17a57485f7b51b56a88594490b20cc68",
"structMayaFlux_1_1Journal_1_1JournalEntry_ae778590040d063a16587679ded8d78c8.html#ae778590040d063a16587679ded8d78c8",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a06671a2baa3a79e07ab4de29c1148048.html#a06671a2baa3a79e07ab4de29c1148048",
"structMayaFlux_1_1Kriya_1_1BufferDelay_a4265ae1f637c7597f7928fa1778177ab.html#a4265ae1f637c7597f7928fa1778177ab",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ComputePress_1_1PipelineState_a79fa22d9e45e283e69ebd13fd0ac944e.html#a79fa22d9e45e283e69ebd13fd0ac944e",
"structMayaFlux_1_1Registry_1_1Service_1_1DisplayService_a377a0faf74f63210cd9ca8d7b25d9a68.html#a377a0faf74f63210cd9ca8d7b25d9a68",
"structMayaFlux_1_1Yantra_1_1IO_a5943148d57c545719335829fa5cfce22.html#a5943148d57c545719335829fa5cfce22",
"structstd_1_1hash_3_01std_1_1pair_3_01MayaFlux_1_1Buffers_1_1ProcessingToken_00_01MayaFlux_1_1Bufe99a771863110ed1593d2441c0e7984_a3bd4c4312db3b79638000819f22c3356.html#a3bd4c4312db3b79638000819f22c3356"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';