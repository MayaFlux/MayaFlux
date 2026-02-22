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
      [ "The Paradigm", "md_README.html#autotoc_md65", [
        [ "New Possibilities", "md_README.html#autotoc_md66", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md68", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md70", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md71", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md72", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md73", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md75", [
        [ "Management Mode", "md_README.html#autotoc_md76", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md77", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md78", [
        [ "Requirements", "md_README.html#autotoc_md79", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md80", null ],
        [ "Build", "md_README.html#autotoc_md81", null ]
      ] ],
      [ "Releases & Builds", "md_README.html#autotoc_md83", [
        [ "Stable Releases", "md_README.html#autotoc_md84", null ],
        [ "Development Builds", "md_README.html#autotoc_md85", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md87", [
        [ "Basic Application Structure", "md_README.html#autotoc_md88", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md90", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md92", [
        [ "Tutorials", "md_README.html#autotoc_md93", null ],
        [ "API Documentation", "md_README.html#autotoc_md94", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md96", null ],
      [ "Philosophy", "md_README.html#autotoc_md98", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md100", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md102", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md103", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md104", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md105", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md106", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md108", null ],
      [ "Contributing", "md_README.html#autotoc_md110", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md112", null ],
      [ "Contact", "md_README.html#autotoc_md114", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md119", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md121", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md123", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md126", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md128", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md130", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md132", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md139", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md140", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md141", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md142", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md143", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md144", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md145", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md146", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md148", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md149", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md150", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md151", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md152", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md153", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md154", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md155", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md156", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md157", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md158", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md159", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md160", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md161", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md162", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md163", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md164", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md165", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md166", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md167", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md171", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md173", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md175", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md180", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md185", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md191", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md192", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md193", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md194", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md195", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md197", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md198", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md199", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md200", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md201", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md202", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md203", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md204", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md205", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md206", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md207", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md208", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md209", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md210", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md211", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md212", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md213", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md214", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md215", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md217", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md218", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md219", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md220", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md221", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md222", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md223", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md224", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md225", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md226", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md227", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md228", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md229", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md230", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md231", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md232", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md233", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md234", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md235", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md236", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md237", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md238", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md239", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md240", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md241", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md288", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md290", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md291", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md293", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md295", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md297", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md299", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md300", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md301", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md302", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md303", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md304", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md306", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md308", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md310", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md312", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md313", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md314", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md315", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md316", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md317", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md318", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md319", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md320", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md321", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md322", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md323", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md324", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md326", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md328", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md330", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md332", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md334", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md336", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md338", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md340", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md342", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md344", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md346", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md347", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md348", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md349", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md350", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md351", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md352", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md355", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md356", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md357", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md358", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md359", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md366", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md368", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md375", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md377", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md383", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md384", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md391", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md393", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md400", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md402", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md409", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md411", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", null ]
        ] ],
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md417", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md419", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md426", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md428", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md429", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md430", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md432", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md434", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md435", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md436", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md437", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md438", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md439", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md441", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md443", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md444", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md445", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md447", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md449", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md452", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md453", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md454", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md455", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md456", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md463", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md464", null ],
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md477", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md479", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md488", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md490", null ],
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md492", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md493", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md494", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md495", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md496", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md497", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md499", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md500", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md501", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md502", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md503", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md504", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md505", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md506", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md508", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md509", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md510", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md513", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md515", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md517", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md519", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md521", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md523", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md525", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md527", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md529", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md532", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md534", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md536", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md538", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md540", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md542", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md544", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md547", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md550", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md552", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md553", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md555", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md557", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md560", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md561", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md563", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md565", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md567", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md569", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md571", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md572", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md574", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md576", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md578", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md580", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md582", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md584", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md586", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md588", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md590", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md592", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md594", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md596", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md598", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md600", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md601", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md603", null ]
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
"CoordUtils_8cpp.html#ab1d4d86d7f3183462598c145ce313388",
"EnergyAnalyzer_8hpp.html#a1ecc68d5d1612ca66db3a23a1055ae35",
"GlobalGraphicsInfo_8hpp.html#a0f3640bfdb5da9732c985512f5ddd055a1e23852820b9154316c7c06e2b7ba051",
"HIDBackend_8hpp_source.html",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62a21c2e59531c8710156d34a3c30ac81d5",
"MotionCurves_8cpp.html#a20bcc988582b6b8dfd3de3c199455fa6",
"PhysicsOperator_8hpp.html#a2f18fac118fdbf50fb9e689789987d0ca4294c746bb33d3a99508391542b33435",
"RenderProcessor_8cpp_source.html",
"StatisticalAnalyzer_8hpp.html#ac907c2e6c12bbbea4d1ed44eb436e84fa50af38ed06c82d7bc9af1f6d34d4ffca",
"UniversalSorter_8hpp.html#af7ae54e9c5df2848e474f0568ccbdb7e",
"Yantra_8cpp.html#a6b5aed304676ea30ffe84686c4f2bfeb",
"classLila_1_1EventBus.html",
"classMayaFlux_1_1Buffers_1_1AudioBuffer.html",
"classMayaFlux_1_1Buffers_1_1BufferManager_a63ff4112e435b0c4ab84d6691ec58068.html#a63ff4112e435b0c4ab84d6691ec58068",
"classMayaFlux_1_1Buffers_1_1BufferSupplyMixing.html",
"classMayaFlux_1_1Buffers_1_1DescriptorBindingsProcessor_a9a9dae2e03d2376dd5bf4b1e5a7884f5.html#a9a9dae2e03d2376dd5bf4b1e5a7884f5a3932d629fb5e2be9d09b3a4485b3cc9d",
"classMayaFlux_1_1Buffers_1_1LogicProcessor_a3728b3f430e1174bb249238eaf9f75bc.html#a3728b3f430e1174bb249238eaf9f75bca9eeb52badb613229884838847294b90d",
"classMayaFlux_1_1Buffers_1_1NodeTextureBuffer_ae4283d11fe6637b7070b88c8df94484a.html#ae4283d11fe6637b7070b88c8df94484a",
"classMayaFlux_1_1Buffers_1_1RootBuffer_a2b9197d91c068da2f1e55d7d64653562.html#a2b9197d91c068da2f1e55d7d64653562",
"classMayaFlux_1_1Buffers_1_1SoundFileBridge_ad0bedd820e321b8c8f046d5989ed0285.html#ad0bedd820e321b8c8f046d5989ed0285",
"classMayaFlux_1_1Buffers_1_1TokenUnitManager_ae6aa0b974f630058d58159a1b03560a2.html#ae6aa0b974f630058d58159a1b03560a2",
"classMayaFlux_1_1Core_1_1AudioBackendFactory_aad4ce811c754786f9ce16b91b3cfc622.html#aad4ce811c754786f9ce16b91b3cfc622",
"classMayaFlux_1_1Core_1_1BufferProcessingHandle_a71843f9187ec9d8a3b9e67c789c77fa4.html#a71843f9187ec9d8a3b9e67c789c77fa4",
"classMayaFlux_1_1Core_1_1GlfwWindow_a9295bb3be9ff002f9d5ea7477d65d600.html#a9295bb3be9ff002f9d5ea7477d65d600",
"classMayaFlux_1_1Core_1_1IGraphicsBackend_a595024e6085e995c8c30d5b5d03a40af.html#a595024e6085e995c8c30d5b5d03a40af",
"classMayaFlux_1_1Core_1_1InputSubsystem_afb36f8317c1a11b1016b563f970dbf99.html#afb36f8317c1a11b1016b563f970dbf99",
"classMayaFlux_1_1Core_1_1SubsystemManager_a746de994c9b4ea71b4a9b6f7552022db.html#a746de994c9b4ea71b4a9b6f7552022db",
"classMayaFlux_1_1Core_1_1VKDescriptorManager_a617aab586a0fdda8f8ab15e514d65c21.html#a617aab586a0fdda8f8ab15e514d65c21",
"classMayaFlux_1_1Core_1_1VKImage_a24eb995f9dfa876d305e9d3ce91f0a34.html#a24eb995f9dfa876d305e9d3ce91f0a34",
"classMayaFlux_1_1Core_1_1VKShaderModule_af2ee184bf6f991e222bf0d1bdcebc6fe.html#af2ee184bf6f991e222bf0d1bdcebc6fe",
"classMayaFlux_1_1Core_1_1Window_a8d6d1da9f029538a2a03637b18c4313e.html#a8d6d1da9f029538a2a03637b18c4313e",
"classMayaFlux_1_1IO_1_1ImageReader_ab7d777f9746d217057998da8de4e7318.html#ab7d777f9746d217057998da8de4e7318",
"classMayaFlux_1_1Journal_1_1Archivist_a92bdd85079ca22bfc75a35ff656a3d82.html#a92bdd85079ca22bfc75a35ff656a3d82",
"classMayaFlux_1_1Kakshya_1_1DynamicSoundStream_a500d7fec8f9e27a2ed8ca11cf8a017b0.html#a500d7fec8f9e27a2ed8ca11cf8a017b0",
"classMayaFlux_1_1Kakshya_1_1RegionOrganizationProcessor_a7675770cf73a3a0b797b8ac9f05e1598.html#a7675770cf73a3a0b797b8ac9f05e1598",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a8e7740ffcd9e55d0c62fa5ce78ebd769.html#a8e7740ffcd9e55d0c62fa5ce78ebd769",
"classMayaFlux_1_1Kinesis_1_1Stochastic_1_1Stochastic_a9d57386d7231ae1612b96776771e7950.html#a9d57386d7231ae1612b96776771e7950",
"classMayaFlux_1_1Kriya_1_1BufferOperation_af03c7f3df8b0ba1d2dc0c2e1b466d46b.html#af03c7f3df8b0ba1d2dc0c2e1b466d46b",
"classMayaFlux_1_1Kriya_1_1EventChain_ac2de8268fd9e0da977d704520e51385e.html#ac2de8268fd9e0da977d704520e51385e",
"classMayaFlux_1_1Nodes_1_1BinaryOpNode_a757bd5eed3572abf3c4335a767b0fc72.html#a757bd5eed3572abf3c4335a767b0fc72",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Generator_a2fa6a7186a5e50a3ac172b7cc467b058.html#a2fa6a7186a5e50a3ac172b7cc467b058",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a9c597a44f3eec5bcb6f505b6644bad36.html#a9c597a44f3eec5bcb6f505b6644bad36",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_abedaeb82771cdf17775566786e19d868.html#abedaeb82771cdf17775566786e19d868",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1GeometryWriterNode_a2a7e73e5e9dff22d52967e3bb289c5ab.html#a2a7e73e5e9dff22d52967e3bb289c5ab",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointNode_ae31361e8430da0e07e295c33704133f9.html#ae31361e8430da0e07e295c33704133f9",
"classMayaFlux_1_1Nodes_1_1Input_1_1InputNode_a481d16870afefb863adea561b98047d8.html#a481d16870afefb863adea561b98047d8",
"classMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_aec29355c52c7bcb3ecdbbb490ce2ea15.html#aec29355c52c7bcb3ecdbbb490ce2ea15",
"classMayaFlux_1_1Nodes_1_1Network_1_1PathOperator_a2bfd2bf60eae17e7ba7bd5eebcebc318.html#a2bfd2bf60eae17e7ba7bd5eebcebc318",
"classMayaFlux_1_1Nodes_1_1Network_1_1PointCloudNetwork_acc330c7695cd9d2d3e46e4acc38c0ee0.html#acc330c7695cd9d2d3e46e4acc38c0ee0",
"classMayaFlux_1_1Nodes_1_1NodeGraphManager_a1797fc9ad51932332bb64f23d6603fa7.html#a1797fc9ad51932332bb64f23d6603fa7",
"classMayaFlux_1_1Nodes_1_1RootNode_a54874a3f0e12685053005d562588f7cc.html#a54874a3f0e12685053005d562588f7cc",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_a148fdbfbb76b1df59986ea791e772885.html#a148fdbfbb76b1df59986ea791e772885",
"classMayaFlux_1_1Portal_1_1Graphics_1_1TextureLoom_ac5e1ee8ca32707cc9150992a2ce16ad1.html#ac5e1ee8ca32707cc9150992a2ce16ad1",
"classMayaFlux_1_1Vruta_1_1Event_a7b1ccb20fa42bf4a85e7117119ec3d80.html#a7b1ccb20fa42bf4a85e7117119ec3d80",
"classMayaFlux_1_1Vruta_1_1SoundRoutine_a19277a7aeb11606b127fd8645e561a98.html#a19277a7aeb11606b127fd8645e561a98",
"classMayaFlux_1_1Yantra_1_1ComputeMatrix.html",
"classMayaFlux_1_1Yantra_1_1EnergyAnalyzer_aec13d86bb3cf8d0e238880302e886e04.html#aec13d86bb3cf8d0e238880302e886e04",
"classMayaFlux_1_1Yantra_1_1OperationPool_a9c707f65328413db17f376fb7beabc76.html#a9c707f65328413db17f376fb7beabc76",
"classMayaFlux_1_1Yantra_1_1TemporalTransformer_ad3cba0a5fb540d79d413d9a6a54d3316.html#ad3cba0a5fb540d79d413d9a6a54d3316",
"classMayaFlux_1_1Yantra_1_1UniversalTransformer_a30dc29dfd593e6cc283bae07a3cb5249.html#a30dc29dfd593e6cc283bae07a3cb5249",
"globals_defs.html",
"md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374",
"namespaceMayaFlux_1_1Config_acbb1cccf0d87301df74cdfbabd681ac9.html#acbb1cccf0d87301df74cdfbabd681ac9",
"namespaceMayaFlux_1_1Journal_1_1AnsiColors_a24ea7713a8747a5933cc3389ccd52c23.html#a24ea7713a8747a5933cc3389ccd52c23",
"namespaceMayaFlux_1_1Kakshya_ad5ca0e4f289a6b37c0100c87095c8c97.html#ad5ca0e4f289a6b37c0100c87095c8c97",
"namespaceMayaFlux_1_1Nodes_1_1Network_a2f18fac118fdbf50fb9e689789987d0c.html#a2f18fac118fdbf50fb9e689789987d0caff3480bb59f4249a9453a0caaa1b2236",
"namespaceMayaFlux_1_1Yantra_a0fdcb54bbbc55da5e4974ccc18e56e4b.html#a0fdcb54bbbc55da5e4974ccc18e56e4ba6a04f9e9b7a0c60c00fe0daa2e12eca0",
"namespaceMayaFlux_1_1Yantra_aad2895941e12cdd28c214ba4fe0a6de3.html#aad2895941e12cdd28c214ba4fe0a6de3afba5cdfa4bcb408f641c743519a0fe19",
"namespaceMayaFlux_a704691e40254e6479f2e45b17ab69902.html#a704691e40254e6479f2e45b17ab69902",
"structLila_1_1ErrorEvent.html",
"structMayaFlux_1_1Buffers_1_1RootGraphicsUnit_a38146b801bc81325357f7295fa6fab54.html#a38146b801bc81325357f7295fa6fab54",
"structMayaFlux_1_1Core_1_1GlfwPreInitConfig_adca3b193580f605945f5a47c5c8cd816.html#adca3b193580f605945f5a47c5c8cd816",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_a142457d96c976083f52d1750a6ee299a.html#a142457d96c976083f52d1750a6ee299a",
"structMayaFlux_1_1Core_1_1HIDDeviceInfoExt_aa7c4483a4fd2e3898b4248d6123dd0c3.html#aa7c4483a4fd2e3898b4248d6123dd0c3",
"structMayaFlux_1_1Core_1_1OSCBackendInfo_ac3142cc2d9a745ff3a1847b494154352.html#ac3142cc2d9a745ff3a1847b494154352",
"structMayaFlux_1_1Core_1_1WindowCreateInfo_a36699320aebdf9db78cdd68a70e6efcf.html#a36699320aebdf9db78cdd68a70e6efcf",
"structMayaFlux_1_1Journal_1_1JournalEntry_af4333c4e368e4e292ce4589afd1cb87a.html#af4333c4e368e4e292ce4589afd1cb87a",
"structMayaFlux_1_1Kakshya_1_1OrganizedRegion_a28457438cb346672d18e23166da27351.html#a28457438cb346672d18e23166da27351",
"structMayaFlux_1_1Kinesis_1_1BasisMatrices_a809e3a4960b04a7c7a2a35a2df077a21.html#a809e3a4960b04a7c7a2a35a2df077a21",
"structMayaFlux_1_1Nodes_1_1Input_1_1HIDConfig_a64bb5b34f975c0f525d8a3cac56e15e5.html#a64bb5b34f975c0f525d8a3cac56e15e5",
"structMayaFlux_1_1Portal_1_1Graphics_1_1BlendAttachmentConfig_a9747e4d909100d2213cd161b20b9bf04.html#a9747e4d909100d2213cd161b20b9bf04",
"structMayaFlux_1_1Registry_1_1Service_1_1BufferService.html",
"structMayaFlux_1_1Yantra_1_1DataStructureInfo.html",
"structMayaFlux_1_1Yantra_1_1extraction__traits__d_3_01std_1_1vector_3_01Kakshya_1_1DataVariant_01_4_01_4_a39379cb3c7a6b9b28e8ab8caa076f805.html#a39379cb3c7a6b9b28e8ab8caa076f805"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';