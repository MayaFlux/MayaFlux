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
      [ "The Paradigm", "md_README.html#autotoc_md60", [
        [ "New Possibilities", "md_README.html#autotoc_md61", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md63", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md65", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md66", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md67", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md68", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md70", [
        [ "Management Mode", "md_README.html#autotoc_md71", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md72", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md73", [
        [ "Requirements", "md_README.html#autotoc_md74", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md75", null ],
        [ "Build", "md_README.html#autotoc_md76", null ]
      ] ],
      [ "Releases & Builds", "md_README.html#autotoc_md78", [
        [ "Stable Releases", "md_README.html#autotoc_md79", null ],
        [ "Development Builds", "md_README.html#autotoc_md80", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md82", [
        [ "Basic Application Structure", "md_README.html#autotoc_md83", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md85", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md87", [
        [ "Tutorials", "md_README.html#autotoc_md88", null ],
        [ "API Documentation", "md_README.html#autotoc_md89", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md91", null ],
      [ "Philosophy", "md_README.html#autotoc_md93", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md95", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md97", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md98", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md99", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md100", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md101", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md103", null ],
      [ "Contributing", "md_README.html#autotoc_md105", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md107", null ],
      [ "Contact", "md_README.html#autotoc_md109", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md114", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md116", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md118", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md121", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md123", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md125", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md127", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md134", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md135", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md136", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md137", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md138", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md139", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md140", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md141", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md142", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md143", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md144", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md145", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md146", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md148", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md149", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md150", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md151", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md152", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md153", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md154", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md155", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md156", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md157", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md158", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md159", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md160", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md161", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md162", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md166", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md168", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md170", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md175", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md180", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md188", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md189", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md190", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md193", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md194", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md195", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md197", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md198", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md199", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md200", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md201", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md202", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md203", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md204", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md205", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md206", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md207", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md208", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md209", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md210", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md212", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md213", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md214", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md215", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md216", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md217", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md218", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md219", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md220", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md221", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md222", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md223", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md224", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md225", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md226", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md227", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md228", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md229", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md230", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md231", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md232", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md233", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md234", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md235", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md236", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md283", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md285", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md286", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md288", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md290", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md292", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md294", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md295", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md296", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md297", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md298", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md299", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md301", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md303", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md305", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md307", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md308", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md309", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md310", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md311", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md312", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md313", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md314", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md315", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md316", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md317", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md318", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md319", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md321", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md323", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md325", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md327", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md329", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md331", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md333", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md335", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md337", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md339", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md341", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md342", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md343", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md344", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md345", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md346", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md347", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md350", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md352", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md354", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md375", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md379", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md386", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ]
        ] ],
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md429", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md430", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md432", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md433", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md434", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md436", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md437", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md438", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md439", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md440", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md444", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md447", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md448", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md449", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md450", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md451", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ],
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md490", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md492", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md494", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md495", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md496", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md497", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md498", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md499", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md500", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md501", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md503", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md504", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md505", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md508", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md510", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md512", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md514", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md516", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md518", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md520", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md522", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md524", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md527", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md529", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md531", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md533", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md535", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md537", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md539", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md542", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md545", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md547", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md548", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md550", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md552", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md555", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md556", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md558", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md560", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md562", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md564", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md566", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md567", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md569", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md571", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md573", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md575", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md577", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md579", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md581", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md583", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md585", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md587", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md589", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md591", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md593", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md595", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md596", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md598", null ]
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
"Capture_8cpp_source.html",
"CoordUtils_8hpp.html#a12eab32554f456833cc21895bca0a48f",
"EnergyAnalyzer_8hpp.html#a8c8031b77d776f1dbc6071f6489f3292",
"GrammarHelper_8hpp.html#a3b5509355cd7b80a6a049a73d006eb8caedb093b511ddda91e008aacf5db8fd48",
"InputEvents_8cpp.html",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62a88183b946cc5f0e8c96b2e66e1c74a7e",
"NDData_8hpp.html#a6858a30e8f410b5f7808389074aa296fa2649b7aa4172a0b66a849ed9dbab3f96",
"ProcessingTokens_8hpp.html#a14cf747bf2cc6ba81673249bfeb2350ca5681c4d154e8dd5ffb443bc698e15fd1",
"Server_8cpp.html#a65bc7d9e41b755b82bb9dbd8b9d86e17",
"StructureIntrospection_8hpp.html#ac479594582dbe473e5d89c5c771ea3fa",
"VKComputePipeline_8cpp_source.html",
"Yantra_8hpp.html#a985b4fbe457b036da9fbc809aed9aa3e",
"classLila_1_1ServerThread_aa640bbcc90d455adfba8629e342cd692.html#aa640bbcc90d455adfba8629e342cd692",
"classMayaFlux_1_1Buffers_1_1BufferAccessControl_a4f1abfc04b94ed0eae10733c9b04dafc.html#a4f1abfc04b94ed0eae10733c9b04dafc",
"classMayaFlux_1_1Buffers_1_1BufferProcessingChain_a63d031101038d545cf020e5660bf49f3.html#a63d031101038d545cf020e5660bf49f3",
"classMayaFlux_1_1Buffers_1_1Buffer_ad3f57b75fa2ae99b2880a98880d86686.html#ad3f57b75fa2ae99b2880a98880d86686",
"classMayaFlux_1_1Buffers_1_1GeometryBuffer_ae49b9cf8334e4f475993e3eefffbaa37.html#ae49b9cf8334e4f475993e3eefffbaa37",
"classMayaFlux_1_1Buffers_1_1NodeBindingsProcessor_aef4817f4487020cf6e6383e02b3b2a65.html#aef4817f4487020cf6e6383e02b3b2a65",
"classMayaFlux_1_1Buffers_1_1RenderProcessor_a890135d823fb945281a666474fa18c6b.html#a890135d823fb945281a666474fa18c6b",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_aadb3a42f427db73ee7f40139f8241c3a.html#aadb3a42f427db73ee7f40139f8241c3a",
"classMayaFlux_1_1Buffers_1_1TextureProcessor_a22c664d1f23e08fdf018d3a58f600e26.html#a22c664d1f23e08fdf018d3a58f600e26",
"classMayaFlux_1_1Buffers_1_1VKBuffer_aac00f89d8d383cb740e91468a611fdae.html#aac00f89d8d383cb740e91468a611fdae",
"classMayaFlux_1_1Core_1_1BackendResourceManager_ae3f1ac9a86e74efe0b8afa6a4f57168f.html#ae3f1ac9a86e74efe0b8afa6a4f57168f",
"classMayaFlux_1_1Core_1_1GLFWSingleton_adff2bd610ee97e2bf540f0763868834a.html#adff2bd610ee97e2bf540f0763868834a",
"classMayaFlux_1_1Core_1_1HIDBackend_a95b5d11b5df58658c662773a56b7c82c.html#a95b5d11b5df58658c662773a56b7c82c",
"classMayaFlux_1_1Core_1_1InputSubsystem_a0ff2bfec03ea6df781132047d818ecf9.html#a0ff2bfec03ea6df781132047d818ecf9",
"classMayaFlux_1_1Core_1_1RtAudioStream_a14a6667dae36aafd93aa1a1e8b0e20e1.html#a14a6667dae36aafd93aa1a1e8b0e20e1",
"classMayaFlux_1_1Core_1_1VKComputePipeline_ae2d607001fcd897f704bb04c48431b21.html#ae2d607001fcd897f704bb04c48431b21",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a741cf0976f7c8c42a2634d0ac162d6bc.html#a741cf0976f7c8c42a2634d0ac162d6bc",
"classMayaFlux_1_1Core_1_1VKShaderModule_a2e97ca8110a506203fb1319d9bab4009.html#a2e97ca8110a506203fb1319d9bab4009",
"classMayaFlux_1_1Core_1_1WindowManager_a6969dff5a9f733a710ed7e402b35edae.html#a6969dff5a9f733a710ed7e402b35edae",
"classMayaFlux_1_1IO_1_1FileWriter_a19bda5a804efa9584f0b8b743c5bbbc9.html#a19bda5a804efa9584f0b8b743c5bbbc9",
"classMayaFlux_1_1Journal_1_1Archivist_1_1Impl_a1e87e4eb957ff3f231623f51728d039b.html#a1e87e4eb957ff3f231623f51728d039b",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_aef790a2bbfc0106263f72e4586ae85ba.html#aef790a2bbfc0106263f72e4586ae85ba",
"classMayaFlux_1_1Kakshya_1_1RegionCacheManager_a3ae2e17f25ecaaf70b200de3b43826a9.html#a3ae2e17f25ecaaf70b200de3b43826a9",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a2df106c31610185cee99251f3bad8aa7.html#a2df106c31610185cee99251f3bad8aa7",
"classMayaFlux_1_1Kakshya_1_1StructuredView_a46963a6a2dfd3ba383ce0b7af5ecd5fa.html#a46963a6a2dfd3ba383ce0b7af5ecd5fa",
"classMayaFlux_1_1Kriya_1_1BufferOperation_a976f72ffe884eac01ba5b57cb0e2ed17.html#a976f72ffe884eac01ba5b57cb0e2ed17",
"classMayaFlux_1_1Kriya_1_1CycleCoordinator_ad3ce334044d7958f8bab7e6b5b0beb00.html#ad3ce334044d7958f8bab7e6b5b0beb00",
"classMayaFlux_1_1Memory_1_1RingBuffer_ab9398ea2adf1572ccd93b66c668934a1.html#ab9398ea2adf1572ccd93b66c668934a1",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_adcfc7b6b2d8e6d1fc28b36beb9cf386a.html#adcfc7b6b2d8e6d1fc28b36beb9cf386a",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a5d9eb9e8a257bccb93937782b284884e.html#a5d9eb9e8a257bccb93937782b284884e",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_a3075f4fe1afc9f9dc6249ed143e1e87e.html#a3075f4fe1afc9f9dc6249ed143e1e87e",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1ComputeOutContext_a3dde1cf9c581cdebfd65adea818a1f08.html#a3dde1cf9c581cdebfd65adea818a1f08",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PathGeneratorNode_af00fd812df95de41fe84b7c178dfa1b7.html#af00fd812df95de41fe84b7c178dfa1b7",
"classMayaFlux_1_1Nodes_1_1GpuVectorData_af28daf8ad2a02ab5e4944e1edae0f08f.html#af28daf8ad2a02ab5e4944e1edae0f08f",
"classMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_a7e5837a91fc7a36c49e12e98f7e144f6.html#a7e5837a91fc7a36c49e12e98f7e144f6",
"classMayaFlux_1_1Nodes_1_1Network_1_1ParticleNetwork_a142528d227b140ad0d53a5a9211a53e1.html#a142528d227b140ad0d53a5a9211a53e1abc899ea5d6a94a8e4e60c16244919ba0",
"classMayaFlux_1_1Nodes_1_1Network_1_1PhysicsOperator_ac9c21ef7e09836a4f47055140068f847.html#ac9c21ef7e09836a4f47055140068f847",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_a95e1e6b87e88cec39baaaaa0715d6803.html#a95e1e6b87e88cec39baaaaa0715d6803",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ComputePress.html",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a4dd0aa12e086b204214f0823ad212d17.html#a4dd0aa12e086b204214f0823ad212d17",
"classMayaFlux_1_1Registry_1_1BackendRegistry_af56fb38c9e3a502f619f6205beb49384.html#af56fb38c9e3a502f619f6205beb49384",
"classMayaFlux_1_1Vruta_1_1FrameClock_a8d6568a867af330c9306fd8f79f2bfd3.html#a8d6568a867af330c9306fd8f79f2bfd3",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_a0150aa70db20c27955c4ad7f0c05c4ac.html#a0150aa70db20c27955c4ad7f0c05c4ac",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix_a9ff3ff4f99a73737d22c980ff6c5399f.html#a9ff3ff4f99a73737d22c980ff6c5399f",
"classMayaFlux_1_1Yantra_1_1FeatureExtractor_aeaddb5ddfdc38ed226705164d54f45a0.html#aeaddb5ddfdc38ed226705164d54f45a0",
"classMayaFlux_1_1Yantra_1_1OperationRegistry_ad4fb72aec8a0feb457b0e7a2ba83e980.html#ad4fb72aec8a0feb457b0e7a2ba83e980",
"classMayaFlux_1_1Yantra_1_1UniversalAnalyzer_aa17041f06b3f457a9ce9a1262c07821f.html#aa17041f06b3f457a9ce9a1262c07821f",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_ace1dfab4ce0f281e9da5dec81d724b18.html#ace1dfab4ce0f281e9da5dec81d724b18",
"md_CONTRIBUTING.html#autotoc_md116",
"md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442",
"namespaceMayaFlux_1_1Core_a92d87611501b97a1bc7b059c30b46d18.html#a92d87611501b97a1bc7b059c30b46d18",
"namespaceMayaFlux_1_1Journal_a4d1f572381818d4f67c4a5294d68ef2d.html#a4d1f572381818d4f67c4a5294d68ef2dadba769728bf564bff7f85bc50e966a52",
"namespaceMayaFlux_1_1Kinesis_a244d0940ab7ca614369c46022d56b2d1.html#a244d0940ab7ca614369c46022d56b2d1aaac544aacc3615aada24897a215f5046",
"namespaceMayaFlux_1_1Portal_1_1Graphics_a8a7827841398873c9412cfe765ea35b7.html#a8a7827841398873c9412cfe765ea35b7",
"namespaceMayaFlux_1_1Yantra_a4e801f6f56c44e3baa3146957c67fcf3.html#a4e801f6f56c44e3baa3146957c67fcf3",
"namespaceMayaFlux_1_1Yantra_aeede47ef227de112c0d668d3fc0de51f.html#aeede47ef227de112c0d668d3fc0de51f",
"namespaceMayaFlux_ad2f0572dfef62bb7629c7ef74ff29449.html#ad2f0572dfef62bb7629c7ef74ff29449",
"structMayaFlux_1_1Buffers_1_1GeometryBindingsProcessor_1_1GeometryBinding_a86f7e404505c2d8ab82109e51b094a2d.html#a86f7e404505c2d8ab82109e51b094a2d",
"structMayaFlux_1_1Core_1_1ColorBlendAttachment_a1044c556f5559870eb0a08ac7031e7e4.html#a1044c556f5559870eb0a08ac7031e7e4",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_aa8298bf7e6d501a1aa8c9aadaa74cde6.html#aa8298bf7e6d501a1aa8c9aadaa74cde6a4abfc22ad3c3ef567d80c65263fd7754",
"structMayaFlux_1_1Core_1_1GraphicsSurfaceInfo_a090b5328004f669b39ecd617512a7b2e.html#a090b5328004f669b39ecd617512a7b2e",
"structMayaFlux_1_1Core_1_1InputValue_a151195a72fba6bc4620b3a1b89080e0b.html#a151195a72fba6bc4620b3a1b89080e0b",
"structMayaFlux_1_1Core_1_1SubpassDescription_aa2791a24455150c3c1a4e95d49e3c545.html#aa2791a24455150c3c1a4e95d49e3c545",
"structMayaFlux_1_1Core_1_1WindowState_ae97c54bbc19bd8598bdaaf1ad112af38.html#ae97c54bbc19bd8598bdaaf1ad112af38",
"structMayaFlux_1_1Kakshya_1_1DataConverter_3_01T_00_01T_01_4.html",
"structMayaFlux_1_1Kakshya_1_1RegionSegment_a52d26766a372388847af5ae71eb9ee11.html#a52d26766a372388847af5ae71eb9ee11",
"structMayaFlux_1_1Memory_1_1FixedStorage_a3f2ad964072b3feae7a2af0362919b6b.html#a3f2ad964072b3feae7a2af0362919b6b",
"structMayaFlux_1_1Nodes_1_1NodeConfig.html",
"structMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_1_1CommandBufferState_af627a8c8456fcd841f675e63a7e1ed1c.html#af627a8c8456fcd841f675e63a7e1ed1c",
"structMayaFlux_1_1Yantra_1_1ChannelEnergy_a7b5e7955281a431f9e2460e8776ffd41.html#a7b5e7955281a431f9e2460e8776ffd41",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01Eigen_1_1MatrixXd_01_4_a84925286b9a431b80ee50c822f794044.html#a84925286b9a431b80ee50c822f794044"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';