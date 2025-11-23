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
      [ "The Paradigm", "md_README.html#autotoc_md42", [
        [ "What Becomes Possible", "md_README.html#autotoc_md43", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md45", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md47", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md48", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md49", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md50", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md52", [
        [ "Management Mode", "md_README.html#autotoc_md53", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md54", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md55", [
        [ "Requirements", "md_README.html#autotoc_md56", null ],
        [ "Build", "md_README.html#autotoc_md57", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md59", [
        [ "Basic Application Structure", "md_README.html#autotoc_md60", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md62", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md64", [
        [ "Tutorials", "md_README.html#autotoc_md65", null ],
        [ "API Documentation", "md_README.html#autotoc_md66", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md68", null ],
      [ "Philosophy", "md_README.html#autotoc_md70", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md72", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md74", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md75", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md76", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md77", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md78", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md80", null ],
      [ "Contributing", "md_README.html#autotoc_md82", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md84", null ],
      [ "Contact", "md_README.html#autotoc_md86", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md91", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md93", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md95", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md98", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md100", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md102", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md104", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md111", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md112", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md113", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md114", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md115", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md116", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md117", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md118", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md119", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md120", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md121", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md122", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md123", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md124", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md125", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md126", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md127", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md128", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md129", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md130", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md131", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md132", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md133", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md134", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md135", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md136", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md137", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md138", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md139", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md143", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md145", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md147", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md152", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md157", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md163", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md164", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md165", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md166", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md167", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md168", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md169", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md170", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md171", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md172", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md173", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md174", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md175", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md177", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md178", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md179", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md180", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md181", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md182", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md183", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md184", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md185", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md186", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md187", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md189", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md190", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md191", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md192", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md193", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md194", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md195", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md196", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md197", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md198", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md199", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md200", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md201", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md202", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md203", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md204", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md205", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md206", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md207", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md208", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md209", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md210", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md211", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md212", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md213", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md249", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md251", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md252", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md254", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md256", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md258", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md260", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md261", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md262", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md263", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md264", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md265", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md267", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md269", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md271", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md273", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md274", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md275", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md276", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md277", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md278", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md279", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md280", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md281", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md282", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md283", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md284", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md285", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md287", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md289", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md291", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md293", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md295", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md297", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md299", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md301", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md303", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md305", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md307", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md308", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md309", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md310", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md311", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md312", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md313", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md316", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md317", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md319", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md321", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md323", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md325", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md327", null ],
        [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md329", [
          [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md330", null ]
        ] ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md332", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md334", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md336", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md338", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md340", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md342", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md343", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md345", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md347", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md349", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md351", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md353", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md358", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md386", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ]
        ] ],
        [ "StreamWriteProcessor (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", null ],
          [ "Expansion 1: What StreamWriteProcessor Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md426", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md429", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md433", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md435", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md437", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md438", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md440", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md444", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md446", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md448", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md451", null ],
      [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md453", null ],
      [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
      [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ],
      [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
      [ "Expansion 5: What <tt>.read()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 4: ContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md493", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md495", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md497", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md499", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md500", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md501", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md503", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md505", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md507", null ],
        [ "Expansion 3: StreamWriteProcessor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md509", null ],
        [ "Expansion 4: FileBridgeBuffer—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md511", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md513", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md515", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md517", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md519", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md521", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md522", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md523", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md525", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md527", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md529", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md531", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md533", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md535", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md537", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md539", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md541", null ]
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
"Creator_8hpp.html#a06647b1dcbbdb0b2ca916102a97a68ad",
"FeatureExtractor_8hpp_source.html",
"GraphicsUtils_8hpp.html#acb58a6992cbd0ab2da77b7a6d20f96cca14d20da17f6d27fc43c5f311c17eb912",
"NDData_8hpp.html#a6858a30e8f410b5f7808389074aa296fae3e8d974260c689567678b70c16ce32c",
"RegionUtils_8cpp.html#ab5807ad27a7ccc13d39d900489722a5c",
"SpectralHelper_8hpp.html#a9c717e7997f5cca25982ca194f25a351",
"UniversalAnalyzer_8hpp.html#aa6110212517477ff050bd2920e2af3cda9df871acd510388a6a68a736c0e7040a",
"WindowManager_8cpp_source.html",
"classLila_1_1ClangInterpreter_ae244bb7c8c3d49155d5da291529d7889.html#ae244bb7c8c3d49155d5da291529d7889",
"classLila_a7dc80e5a30b28fa2e69be88f8d29ed6c.html#a7dc80e5a30b28fa2e69be88f8d29ed6ca9aa1b03934893d7134a660af4204f2a9",
"classMayaFlux_1_1Buffers_1_1BufferManager_a3b21013c05e4a634f5f36184a528fcd3.html#a3b21013c05e4a634f5f36184a528fcd3",
"classMayaFlux_1_1Buffers_1_1BufferSupplyMixing.html",
"classMayaFlux_1_1Buffers_1_1DescriptorBuffer_a805d98cc1ba4068972d1c898a55e3fe6.html#a805d98cc1ba4068972d1c898a55e3fe6",
"classMayaFlux_1_1Buffers_1_1InputAudioBuffer_a16e8d7449bd5d8ed649b75bf8f89e70f.html#a16e8d7449bd5d8ed649b75bf8f89e70f",
"classMayaFlux_1_1Buffers_1_1PresentProcessor_a6057fc472a98f636013b6089aa7600fe.html#a6057fc472a98f636013b6089aa7600fe",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_a4fd3baa2670bde6d3dff7daaab9797ce.html#a4fd3baa2670bde6d3dff7daaab9797ce",
"classMayaFlux_1_1Buffers_1_1TransferProcessor_a7fb4ecefdae5fb14311f065219052441.html#a7fb4ecefdae5fb14311f065219052441",
"classMayaFlux_1_1Core_1_1AudioSubsystem_a13beabc851eed1e1f211a7f5c790bdcc.html#a13beabc851eed1e1f211a7f5c790bdcc",
"classMayaFlux_1_1Core_1_1DescriptorUpdateBatch_afce66d626b274d1d646922cd6f2858a4.html#afce66d626b274d1d646922cd6f2858a4",
"classMayaFlux_1_1Core_1_1GraphicsSubsystem_a8edbb4e731ea5677f47b9de8cc583bcb.html#a8edbb4e731ea5677f47b9de8cc583bcb",
"classMayaFlux_1_1Core_1_1SubsystemManager_a3c26766f19f4239bb258fbe2e59192b7.html#a3c26766f19f4239bb258fbe2e59192b7",
"classMayaFlux_1_1Core_1_1VKDescriptorManager_a5c8b2ee11a9b37fbaf5dfb861039323a.html#a5c8b2ee11a9b37fbaf5dfb861039323a",
"classMayaFlux_1_1Core_1_1VKImage_a3d5ad8f49a9e74232769678319ea5d29.html#a3d5ad8f49a9e74232769678319ea5d29",
"classMayaFlux_1_1Core_1_1VKSwapchain.html",
"classMayaFlux_1_1CreationHandle_a4ed794de62d783c454ed3fd367587c57.html#a4ed794de62d783c454ed3fd367587c57",
"classMayaFlux_1_1IO_1_1SoundFileReader_a87b10deb4e9f9badd0992589caa94a95.html#a87b10deb4e9f9badd0992589caa94a95",
"classMayaFlux_1_1Journal_1_1RingBuffer_a888a328a7f8dded525049d71d990ec2b.html#a888a328a7f8dded525049d71d990ec2b",
"classMayaFlux_1_1Kakshya_1_1NDDataContainer.html",
"classMayaFlux_1_1Kakshya_1_1SignalSourceContainer_ae9dad37f0b166fd5c78b59ab5b7cf9b0.html#ae9dad37f0b166fd5c78b59ab5b7cf9b0",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_af22c354072afc20184cef3b88f7f9af1.html#af22c354072afc20184cef3b88f7f9af1",
"classMayaFlux_1_1Kriya_1_1BufferOperation_a4e61e95438b80ad8394552d3968d6490.html#a4e61e95438b80ad8394552d3968d6490",
"classMayaFlux_1_1Kriya_1_1CaptureBuilder_a418e5ed06c3eb0854d59d2a3edb1bac3.html#a418e5ed06c3eb0854d59d2a3edb1bac3",
"classMayaFlux_1_1Nodes_1_1ChainNode_a120705bcc62d3d943fc376e2465bd368.html#a120705bcc62d3d943fc376e2465bd368",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Impulse_a3a0a8482eea678abe1a73bba129d8e50.html#a3a0a8482eea678abe1a73bba129d8e50",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Phasor.html",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Sine_a73e2564a75e0fd28659470ddd4cf9e9e.html#a73e2564a75e0fd28659470ddd4cf9e9e",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointCollectionNode_a811ca568b8a5c27fb759bd66648f520d.html#a811ca568b8a5c27fb759bd66648f520d",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_ad56f5f12b2711cd6d27fd71bd015179b.html#ad56f5f12b2711cd6d27fd71bd015179b",
"classMayaFlux_1_1Nodes_1_1RootNode_aedb30b498e5a14a83be266669524d1ad.html#aedb30b498e5a14a83be266669524d1ad",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a4dd0aa12e086b204214f0823ad212d17.html#a4dd0aa12e086b204214f0823ad212d17",
"classMayaFlux_1_1Vruta_1_1ComplexRoutine_aa31fb8031db2486f3a2fa3c946e37520.html#aa31fb8031db2486f3a2fa3c946e37520",
"classMayaFlux_1_1Vruta_1_1GraphicsRoutine_a66d47178cdfd4352be4c5cc0f96d5b18.html#a66d47178cdfd4352be4c5cc0f96d5b18",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_aa3a85946f4294c74596d61431cb8a228.html#aa3a85946f4294c74596d61431cb8a228",
"classMayaFlux_1_1Yantra_1_1ComputeOperation_a8f21d9df92876fb9a6e594cad9331ec8.html#a8f21d9df92876fb9a6e594cad9331ec8",
"classMayaFlux_1_1Yantra_1_1GrammarAwareComputeMatrix_a464ccfe6a20b5f5913f1d9544e91239b.html#a464ccfe6a20b5f5913f1d9544e91239b",
"classMayaFlux_1_1Yantra_1_1StandardSorter_ab0cab009ecf55edb7831341dfafb4dbd.html#ab0cab009ecf55edb7831341dfafb4dbd",
"classMayaFlux_1_1Yantra_1_1UniversalExtractor_ab5944006bcd00920e40149451f125050.html#ab5944006bcd00920e40149451f125050",
"dir_68267d1309a1af8e8297ef4c3efbcdba.html",
"md_docs_2Digital__Transformation__Paradigm.html#autotoc_md176",
"namespaceMayaFlux_1_1Buffers_a4b2f475023a8f84a3f134843f8bf4930.html#a4b2f475023a8f84a3f134843f8bf4930",
"namespaceMayaFlux_1_1Kakshya_a2479c7817b562a13275654367e0b7c6b.html#a2479c7817b562a13275654367e0b7c6b",
"namespaceMayaFlux_1_1Nodes_aa995eb0e67c4e7da29783ff14293700d.html#aa995eb0e67c4e7da29783ff14293700d",
"namespaceMayaFlux_1_1Yantra_a1ac4b661cf28f19dfe1add5ee231b216.html#a1ac4b661cf28f19dfe1add5ee231b216",
"namespaceMayaFlux_1_1Yantra_ac1be280f4546a882ab996233f09b3265.html#ac1be280f4546a882ab996233f09b3265",
"namespaceMayaFlux_aac38992207484764f0a4d9d0e3d82d6b.html#aac38992207484764f0a4d9d0e3d82d6ba1527f8d8a23548d98cd4bab285c66486",
"structMayaFlux_1_1Buffers_1_1MixSource_a40e160c709f2d9294e2d679e32a64857.html#a40e160c709f2d9294e2d679e32a64857",
"structMayaFlux_1_1Core_1_1ColorBlendAttachment_ad750dc9b53267399e2acd91e66970a62.html#ad750dc9b53267399e2acd91e66970a62",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_aefb0f44c0b80731af94f0e775e4777f5.html#aefb0f44c0b80731af94f0e775e4777f5",
"structMayaFlux_1_1Core_1_1GraphicsSurfaceInfo_a3b8bc7e8df5fab908ad3ce611b1364b3.html#a3b8bc7e8df5fab908ad3ce611b1364b3",
"structMayaFlux_1_1Core_1_1VideoMode_a6a28353668fcc60a3e9b10cc8980e8a2.html#a6a28353668fcc60a3e9b10cc8980e8a2",
"structMayaFlux_1_1IO_1_1ImageData_a9f6a686f0d5c87ee0c22e14b4f5503e2.html#a9f6a686f0d5c87ee0c22e14b4f5503e2",
"structMayaFlux_1_1Kakshya_1_1DataDimension_abbfb65596b07668da0d768b4add9bde3.html#abbfb65596b07668da0d768b4add9bde3",
"structMayaFlux_1_1Kakshya_1_1VertexLayout.html",
"structMayaFlux_1_1Portal_1_1Graphics_1_1RasterizationConfig_a4c2a9d0bd1bb7f750a5da7e67398a5a4.html#a4c2a9d0bd1bb7f750a5da7e67398a5a4",
"structMayaFlux_1_1Vruta_1_1TaskEntry.html",
"structMayaFlux_1_1Yantra_1_1IO_aebaed2044cdab12311057b933f73adc0.html#aebaed2044cdab12311057b933f73adc0"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';