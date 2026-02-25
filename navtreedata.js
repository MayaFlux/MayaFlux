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
      [ "The Paradigm", "md_README.html#autotoc_md70", [
        [ "New Possibilities", "md_README.html#autotoc_md71", null ]
      ] ],
      [ "Architecture", "md_README.html#autotoc_md73", null ],
      [ "Current Implementation Status", "md_README.html#autotoc_md75", [
        [ "✓ Production-Ready", "md_README.html#autotoc_md76", null ],
        [ "✓ Proof-of-Concept (Validated)", "md_README.html#autotoc_md77", null ],
        [ "→ In Active Development", "md_README.html#autotoc_md78", null ]
      ] ],
      [ "Quick Start(Projects) WEAVE", "md_README.html#autotoc_md80", [
        [ "Management Mode", "md_README.html#autotoc_md81", null ],
        [ "Project Creation Mode", "md_README.html#autotoc_md82", null ]
      ] ],
      [ "Quick Start (DEVELOPER)", "md_README.html#autotoc_md83", [
        [ "Requirements", "md_README.html#autotoc_md84", null ],
        [ "macOS Requirements", "md_README.html#autotoc_md85", null ],
        [ "Build", "md_README.html#autotoc_md86", null ]
      ] ],
      [ "Releases & Builds", "md_README.html#autotoc_md88", [
        [ "Stable Releases", "md_README.html#autotoc_md89", null ],
        [ "Development Builds", "md_README.html#autotoc_md90", null ]
      ] ],
      [ "Using MayaFlux", "md_README.html#autotoc_md92", [
        [ "Basic Application Structure", "md_README.html#autotoc_md93", null ],
        [ "Live Code Modification (Lila)", "md_README.html#autotoc_md95", null ]
      ] ],
      [ "Documentation", "md_README.html#autotoc_md97", [
        [ "Tutorials", "md_README.html#autotoc_md98", null ],
        [ "API Documentation", "md_README.html#autotoc_md99", null ]
      ] ],
      [ "Project Maturity", "md_README.html#autotoc_md101", null ],
      [ "Philosophy", "md_README.html#autotoc_md103", null ],
      [ "For Researchers & Developers", "md_README.html#autotoc_md105", null ],
      [ "Roadmap (Provisional)", "md_README.html#autotoc_md107", [
        [ "Phase 1 (Now)", "md_README.html#autotoc_md108", null ],
        [ "Phase 2 (Q1 2026)", "md_README.html#autotoc_md109", null ],
        [ "Phase 3 (Q2-Q3 2026)", "md_README.html#autotoc_md110", null ],
        [ "Phase 4+ (TBD)", "md_README.html#autotoc_md111", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md113", null ],
      [ "Contributing", "md_README.html#autotoc_md115", null ],
      [ "Authorship & Ethics", "md_README.html#autotoc_md117", null ],
      [ "Contact", "md_README.html#autotoc_md119", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", null ],
    [ "Contributing to MayaFlux", "md_CONTRIBUTING.html", [
      [ "🧩 Contribution Philosophy", "md_CONTRIBUTING.html#autotoc_md124", null ],
      [ "🔧 Contribution Workflow", "md_CONTRIBUTING.html#autotoc_md126", null ],
      [ "🚀 Contribution Areas", "md_CONTRIBUTING.html#autotoc_md128", null ],
      [ "🧱 Requirements for All PRs", "md_CONTRIBUTING.html#autotoc_md131", null ],
      [ "🤝 Communication & Etiquette", "md_CONTRIBUTING.html#autotoc_md133", null ],
      [ "⚖️ Legal & Licensing", "md_CONTRIBUTING.html#autotoc_md135", null ],
      [ "🪜 Where to Start", "md_CONTRIBUTING.html#autotoc_md137", null ]
    ] ],
    [ "Advanced System Architecture: Complete Replacement and Custom Implementation", "md_docs_2Advanced__Context__Control.html", [
      [ "Processing Handles: Token-Scoped System Access", "md_docs_2Advanced__Context__Control.html#autotoc_md144", [
        [ "BufferProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md145", [
          [ "Direct handle creation", "md_docs_2Advanced__Context__Control.html#autotoc_md146", null ]
        ] ],
        [ "NodeProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md147", null ],
        [ "TaskSchedulerHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md148", null ],
        [ "SubsystemProcessingHandle", "md_docs_2Advanced__Context__Control.html#autotoc_md149", [
          [ "Handle composition and custom contexts", "md_docs_2Advanced__Context__Control.html#autotoc_md150", null ]
        ] ],
        [ "Why Processing Handles Instead of Direct Manager Access?", "md_docs_2Advanced__Context__Control.html#autotoc_md151", [
          [ "Handle Performance Characteristics", "md_docs_2Advanced__Context__Control.html#autotoc_md152", null ]
        ] ]
      ] ],
      [ "SubsystemManager: Computational Domain Orchestration", "md_docs_2Advanced__Context__Control.html#autotoc_md153", [
        [ "Subsystem Registration and Lifecycle", "md_docs_2Advanced__Context__Control.html#autotoc_md154", [
          [ "Cross-Subsystem Data Access Control", "md_docs_2Advanced__Context__Control.html#autotoc_md155", null ],
          [ "Processing Hooks and Custom Integration", "md_docs_2Advanced__Context__Control.html#autotoc_md156", null ],
          [ "Direct SubsystemManager Control", "md_docs_2Advanced__Context__Control.html#autotoc_md157", null ]
        ] ]
      ] ],
      [ "Subsystems: Computational Domain Implementation", "md_docs_2Advanced__Context__Control.html#autotoc_md158", [
        [ "ISubsystem Interface and Implementation Patterns", "md_docs_2Advanced__Context__Control.html#autotoc_md159", null ],
        [ "Specialized Subsystem Examples", "md_docs_2Advanced__Context__Control.html#autotoc_md160", [
          [ "AudioSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md161", null ],
          [ "GraphicsSubsystem", "md_docs_2Advanced__Context__Control.html#autotoc_md162", null ],
          [ "Subsystem Communication and Coordination", "md_docs_2Advanced__Context__Control.html#autotoc_md163", null ]
        ] ],
        [ "Direct Subsystem Management", "md_docs_2Advanced__Context__Control.html#autotoc_md164", null ]
      ] ],
      [ "Backends: Hardware and Platform Abstraction", "md_docs_2Advanced__Context__Control.html#autotoc_md165", [
        [ "Audio Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md166", [
          [ "IAudioBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md167", null ],
          [ "Backend Factory Expansion", "md_docs_2Advanced__Context__Control.html#autotoc_md168", null ]
        ] ],
        [ "Graphics Backend Architecture", "md_docs_2Advanced__Context__Control.html#autotoc_md169", [
          [ "IGraphicsBackend Interface", "md_docs_2Advanced__Context__Control.html#autotoc_md170", null ],
          [ "Custom Rendering Pipeline", "md_docs_2Advanced__Context__Control.html#autotoc_md171", null ]
        ] ],
        [ "Network Backend for Distributed Processing", "md_docs_2Advanced__Context__Control.html#autotoc_md172", null ]
      ] ]
    ] ],
    [ "🧱 Build Operations & Distribution", "md_docs_2BuildOps.html", [
      [ "✅ Current State", "md_docs_2BuildOps.html#autotoc_md176", null ],
      [ "🧭 Phase 1 — Documentation & First-Time Experience", "md_docs_2BuildOps.html#autotoc_md178", null ],
      [ "⚙️ Phase 2 — CI/CD & Binary Generation", "md_docs_2BuildOps.html#autotoc_md180", null ],
      [ "📦 Phase 3 — Package Manager Integration", "md_docs_2BuildOps.html#autotoc_md185", null ],
      [ "🧰 Phase 4 — Distribution Packaging", "md_docs_2BuildOps.html#autotoc_md190", null ]
    ] ],
    [ "Digital Transformation Paradigms: Thinking in Data Flow", "md_docs_2Digital__Transformation__Paradigm.html", [
      [ "Introduction", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md196", null ],
      [ "MayaFlux", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md197", [
        [ "Nodes", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md198", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md199", null ],
          [ "Flow logic", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md200", null ],
          [ "Processing as events", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md201", null ]
        ] ],
        [ "Buffers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md202", [
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md203", null ],
          [ "Processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md204", null ],
          [ "Processing chain", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md205", null ],
          [ "Custom processing", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md206", null ]
        ] ],
        [ "Coroutines: Time as a Creative Material", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md207", [
          [ "The Temporal Paradigm", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md208", null ],
          [ "Definitions", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md209", null ],
          [ "Temporal Domains and Coordination", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md210", null ],
          [ "Kriya Temporal Patterns", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md211", null ],
          [ "EventChains and Temporal Composition", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md212", null ],
          [ "Buffer Integration and Capture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md213", null ]
        ] ],
        [ "Containers", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md214", [
          [ "Data Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md215", null ],
          [ "Data Modalities and Detection", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md216", null ],
          [ "SoundFileContainer: Foundational Implementation", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md217", null ],
          [ "Region-Based Data Access", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md218", null ],
          [ "RegionGroups and Metadata", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md219", null ]
        ] ]
      ] ],
      [ "Digital Data Flow Architecture", "md_docs_2Digital__Transformation__Paradigm.html#autotoc_md220", null ]
    ] ],
    [ "Domains and Control: Computational Contexts in Digital Creation", "md_docs_2Domain__and__Control.html", [
      [ "Processing Tokens: Computational Identity", "md_docs_2Domain__and__Control.html#autotoc_md222", [
        [ "Nodes::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md223", null ],
        [ "Buffers::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md224", null ],
        [ "Vruta::ProcessingTokens", "md_docs_2Domain__and__Control.html#autotoc_md225", null ],
        [ "Domain Composition: Unified Computational Environments", "md_docs_2Domain__and__Control.html#autotoc_md226", null ]
      ] ],
      [ "Engine Control vs User Control: Computational Autonomy", "md_docs_2Domain__and__Control.html#autotoc_md227", [
        [ "Nodes", "md_docs_2Domain__and__Control.html#autotoc_md228", [
          [ "NodeGraphManager", "md_docs_2Domain__and__Control.html#autotoc_md229", [
            [ "Explicit user control", "md_docs_2Domain__and__Control.html#autotoc_md230", null ]
          ] ],
          [ "RootNode", "md_docs_2Domain__and__Control.html#autotoc_md231", null ],
          [ "Direct Node Management", "md_docs_2Domain__and__Control.html#autotoc_md232", [
            [ "Chaining", "md_docs_2Domain__and__Control.html#autotoc_md233", null ]
          ] ]
        ] ],
        [ "Buffers", "md_docs_2Domain__and__Control.html#autotoc_md234", [
          [ "BufferManager", "md_docs_2Domain__and__Control.html#autotoc_md235", null ],
          [ "RootBuffer", "md_docs_2Domain__and__Control.html#autotoc_md236", null ],
          [ "Direct buffer management and processing", "md_docs_2Domain__and__Control.html#autotoc_md237", null ]
        ] ],
        [ "Coroutines", "md_docs_2Domain__and__Control.html#autotoc_md238", [
          [ "TaskScheduler", "md_docs_2Domain__and__Control.html#autotoc_md239", null ],
          [ "Kriya", "md_docs_2Domain__and__Control.html#autotoc_md240", null ],
          [ "Clock Systems", "md_docs_2Domain__and__Control.html#autotoc_md241", null ],
          [ "Direct Coroutine Management", "md_docs_2Domain__and__Control.html#autotoc_md242", [
            [ "Self-managed SoundRoutine Creation", "md_docs_2Domain__and__Control.html#autotoc_md243", null ],
            [ "API-based Awaiter Patterns", "md_docs_2Domain__and__Control.html#autotoc_md244", null ]
          ] ],
          [ "Direct Routine Control and State Management", "md_docs_2Domain__and__Control.html#autotoc_md245", null ],
          [ "Multi-domain Coroutine Coordination", "md_docs_2Domain__and__Control.html#autotoc_md246", null ]
        ] ]
      ] ]
    ] ],
    [ "MayaFlux <tt>settings()</tt> - Engine Configuration Guide", "md_docs_2Settings.html", [
      [ "Overview", "md_docs_2Settings.html#autotoc_md293", null ],
      [ "Two Configuration Levels", "md_docs_2Settings.html#autotoc_md295", [
        [ "1. <strong>Audio Stream Configuration</strong> (GlobalStreamInfo)", "md_docs_2Settings.html#autotoc_md296", null ],
        [ "2. <strong>Graphics Configuration</strong> (GlobalGraphicsConfig)", "md_docs_2Settings.html#autotoc_md298", null ],
        [ "3. <strong>Node Graph Semantics</strong> (Rarely needed)", "md_docs_2Settings.html#autotoc_md300", null ],
        [ "4. <strong>Logging / Journal</strong>", "md_docs_2Settings.html#autotoc_md302", null ]
      ] ],
      [ "Common Scenarios", "md_docs_2Settings.html#autotoc_md304", [
        [ "Scenario 1: Simple Audio (Default)", "md_docs_2Settings.html#autotoc_md305", null ],
        [ "Scenario 2: Low-Latency Real-Time (Music Performance)", "md_docs_2Settings.html#autotoc_md306", null ],
        [ "Scenario 3: Studio Recording (High Quality)", "md_docs_2Settings.html#autotoc_md307", null ],
        [ "Scenario 4: Headless / Offline Processing (No Graphics)", "md_docs_2Settings.html#autotoc_md308", null ],
        [ "Scenario 5: Linux with Wayland (Platform-Specific)", "md_docs_2Settings.html#autotoc_md309", null ]
      ] ],
      [ "Important Notes", "md_docs_2Settings.html#autotoc_md311", null ],
      [ "Full Example: user_project.hpp", "md_docs_2Settings.html#autotoc_md313", null ],
      [ "Accessing Configuration Later", "md_docs_2Settings.html#autotoc_md315", null ]
    ] ],
    [ "Contributing: Logging System Migration", "md_docs_2StarterTasks.html", [
      [ "Overview", "md_docs_2StarterTasks.html#autotoc_md317", null ],
      [ "🎯 Why This Matters", "md_docs_2StarterTasks.html#autotoc_md318", null ],
      [ "📋 Prerequisites", "md_docs_2StarterTasks.html#autotoc_md319", null ],
      [ "🚀 Getting Started", "md_docs_2StarterTasks.html#autotoc_md320", [
        [ "Step 1: Pick a File", "md_docs_2StarterTasks.html#autotoc_md321", null ],
        [ "Step 2: Claim Your File", "md_docs_2StarterTasks.html#autotoc_md322", null ]
      ] ],
      [ "🔄 Migration Patterns", "md_docs_2StarterTasks.html#autotoc_md323", [
        [ "Pattern 1: Replace <tt>std::cerr</tt> with Logging", "md_docs_2StarterTasks.html#autotoc_md324", null ],
        [ "Pattern 2: Replace <tt>throw</tt> with <tt>error()</tt>", "md_docs_2StarterTasks.html#autotoc_md325", null ],
        [ "Pattern 3: Replace <tt>throw</tt> inside <tt>catch</tt> with <tt>error_rethrow()</tt>", "md_docs_2StarterTasks.html#autotoc_md326", null ],
        [ "Pattern 4: Replace <tt>std::cout</tt> debug output", "md_docs_2StarterTasks.html#autotoc_md327", null ]
      ] ],
      [ "📊 Decision Tree: Throw vs Fatal vs Log", "md_docs_2StarterTasks.html#autotoc_md328", null ],
      [ "🗺️ Context Guide", "md_docs_2StarterTasks.html#autotoc_md329", [
        [ "Real-Time Contexts", "md_docs_2StarterTasks.html#autotoc_md331", null ],
        [ "Backend Contexts", "md_docs_2StarterTasks.html#autotoc_md333", null ],
        [ "GPU Contexts", "md_docs_2StarterTasks.html#autotoc_md335", null ],
        [ "Subsystem Contexts", "md_docs_2StarterTasks.html#autotoc_md337", null ],
        [ "Processing Contexts", "md_docs_2StarterTasks.html#autotoc_md339", null ],
        [ "Worker Contexts", "md_docs_2StarterTasks.html#autotoc_md341", null ],
        [ "Lifecycle Contexts", "md_docs_2StarterTasks.html#autotoc_md343", null ],
        [ "User Interaction Contexts", "md_docs_2StarterTasks.html#autotoc_md345", null ],
        [ "Coordination Contexts", "md_docs_2StarterTasks.html#autotoc_md347", null ],
        [ "Special Contexts", "md_docs_2StarterTasks.html#autotoc_md349", null ],
        [ "Default Choice", "md_docs_2StarterTasks.html#autotoc_md351", null ]
      ] ],
      [ "🧩 Component Guide", "md_docs_2StarterTasks.html#autotoc_md352", null ],
      [ "✅ Checklist Before Submitting PR", "md_docs_2StarterTasks.html#autotoc_md353", null ],
      [ "📝 PR Template", "md_docs_2StarterTasks.html#autotoc_md354", null ],
      [ "🤔 When to Ask for Help", "md_docs_2StarterTasks.html#autotoc_md355", null ],
      [ "🎓 Learning Resources", "md_docs_2StarterTasks.html#autotoc_md356", null ],
      [ "🏆 Recognition", "md_docs_2StarterTasks.html#autotoc_md357", null ]
    ] ],
    [ "ProcessingExpression", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html", [
      [ "Tutorial: Polynomial Waveshaping", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md360", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md361", null ],
        [ "Expansion 1: Why Polynomials Shape Sound", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md362", null ],
        [ "Expansion 2: What <tt>vega.Polynomial()</tt> Creates", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md363", null ],
        [ "Expansion 3: PolynomialMode::DIRECT", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md364", null ],
        [ "Expansion 4: What <tt>create_processor()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md365", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md367", null ]
      ] ],
      [ "Tutorial: Recursive Polynomials (Filters and Feedback)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md369", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md370", null ],
        [ "Expansion 1: Why This Is a Filter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md371", null ],
        [ "Expansion 2: The History Buffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md372", null ],
        [ "Expansion 3: Stability Warning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md373", null ],
        [ "Expansion 4: Initial Conditions", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md374", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md376", null ]
      ] ],
      [ "Tutorial: Logic as Decision Maker", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md378", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md379", null ],
        [ "Expansion 1: What Logic Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md380", null ],
        [ "Expansion 2: Logic node needs an input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md381", null ],
        [ "Expansion 3: LogicOperator Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md382", null ],
        [ "Expansion 4: ModulationType - Readymade Transformations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md383", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md385", null ]
      ] ],
      [ "Tutorial: Combining Polynomial + Logic", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md387", [
        [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md388", null ],
        [ "Expansion 1: Processing Chains as Transformation Pipelines", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md389", null ],
        [ "Expansion 2: Chain Order Matters", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md390", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md392", null ]
      ] ],
      [ "Tutorial: Processing Chains and Buffer Architecture", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md394", [
        [ "Tutorial: Explicit Chain Building", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md395", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md396", null ]
        ] ],
        [ "Expansion 1: What <tt>create_processor()</tt> Was Doing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md397", null ],
        [ "Expansion 2: Chain Execution Order", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md398", null ],
        [ "Expansion 3: Default Processors vs. Chain Processors", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md399", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md401", null ]
      ] ],
      [ "Tutorial: Various Buffer Types", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md403", [
        [ "Generating from Nodes (NodeBuffer)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md404", [
          [ "The Next Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md405", null ],
          [ "Expansion 1: What NodeBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md406", null ],
          [ "Expansion 2: The <tt>clear_before_process</tt> Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md407", null ],
          [ "Expansion 3: NodeSourceProcessor Mix Parameter", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md408", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md410", null ]
        ] ],
        [ "FeedbackBuffer (Recursive Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md412", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md413", null ],
          [ "Expansion 1: What FeedbackBuffer Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md414", null ],
          [ "Expansion 2: FeedbackBuffer Limitations", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md415", null ],
          [ "Expansion 3: When to Use FeedbackBuffer", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md416", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md418", null ]
        ] ],
        [ "SoundStreamWriter (Capturing Audio)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md420", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md421", null ],
          [ "Expansion 1: What SoundStreamWriter Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md422", null ],
          [ "Expansion 2: Channel-Aware Writing", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md423", null ],
          [ "Expansion 3: Position Management", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md424", null ],
          [ "Expansion 4: Circular Mode", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md425", null ],
          [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md427", null ]
        ] ],
        [ "Closing: The Buffer Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md428", null ]
      ] ],
      [ "Tutorial: Audio Input, Routing, and Multi-Channel Distribution", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md430", [
        [ "Tutorial: Capturing Audio Input", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md431", [
          [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md432", null ]
        ] ],
        [ "Expansion 1: What <tt>create_input_listener_buffer()</tt> Does", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md433", null ],
        [ "Expansion 2: Manual Input Registration", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md434", null ],
        [ "Expansion 3: Input Without Playback", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md435", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md437", null ],
        [ "Tutorial: Buffer Supply (Routing to Multiple Channels)", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md439", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md440", null ]
        ] ],
        [ "Expansion 1: What \"Supply\" Means", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md441", null ],
        [ "Expansion 2: Mix Levels", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md442", null ],
        [ "Expansion 3: Removing Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md443", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md444", null ],
        [ "Tutorial: Buffer Cloning", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md446", [
          [ "The Pattern", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md447", null ]
        ] ],
        [ "Expansion 1: Clone vs. Supply", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md448", null ],
        [ "Expansion 2: Cloning Preserves Structure", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md449", null ],
        [ "Expansion 3: Post-Clone Modification", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md450", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md452", null ],
        [ "Closing: The Routing Ecosystem", "md_docs_2Tutorials_2SculptingData_2ProcessingExpression.html#autotoc_md454", null ]
      ] ]
    ] ],
    [ "MayaFlux Tutorial: Sculpting Data Part I", "md_docs_2Tutorials_2SculptingData_2SculptingData.html", [
      [ "Tutorial: The Simplest First Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md457", [
        [ "Expansion 1: What Is a Container?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md458", null ],
        [ "Expansion 2: Memory, Ownership, and Smart Pointers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md459", null ],
        [ "Expansion 3: What is <tt>vega</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md460", null ],
        [ "Expansion 4: The Container's Processor", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md461", null ],
        [ "Expansion 5: What <tt>.read_audio()</tt> Does NOT Do", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md462", null ]
      ] ],
      [ "Tutorial: Connect to Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md465", [
        [ "The Next Step", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md466", null ],
        [ "Expansion 1: What Are Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md467", null ],
        [ "Expansion 2: Why Per-Channel Buffers?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md468", null ],
        [ "Expansion 3: The Buffer Manager and Buffer Lifecycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md469", null ],
        [ "Expansion 4: SoundContainerBuffer—The Bridge", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md470", null ],
        [ "Expansion 5: Processing Token—AUDIO_BACKEND", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md471", null ],
        [ "Expansion 6: Accessing the Buffers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md472", null ],
        [ "</details>", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md473", null ],
        [ "The Fluent vs. Explicit Comparison", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md474", [
          [ "Fluent (What happens behind the scenes)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md475", null ],
          [ "Explicit (What's actually happening)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md476", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md478", null ]
      ] ],
      [ "Tutorial: Buffers Own Chains", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md480", [
        [ "The Simplest Path", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md481", null ],
        [ "Expansion 1: What Is <tt>vega.IIR()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md482", null ],
        [ "Expansion 2: What Is <tt>MayaFlux::create_processor()</tt>?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md483", null ],
        [ "Expansion 3: What Is a Processing Chain?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md484", null ],
        [ "Expansion 4: Adding Processor to Another Channel (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md485", null ],
        [ "Expansion 5: What Happens Inside", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md486", null ],
        [ "Expansion 6: Processors Are Reusable Building Blocks", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md487", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md489", null ]
      ] ],
      [ "Tutorial: Timing, Streams, and Bridges", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md491", [
        [ "The Current Continous Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md492", null ],
        [ "Where We're Going", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md493", null ],
        [ "Expansion 1: The Architecture of Containers", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md494", null ],
        [ "Expansion 2: Enter DynamicSoundStream", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md495", null ],
        [ "Expansion 3: SoundStreamWriter", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md496", null ],
        [ "Expansion 4: SoundFileBridge—Controlled Flow", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md497", null ],
        [ "Expansion 5: Why This Architecture?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md498", null ],
        [ "Expansion 6: From File to Cycle", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md499", null ],
        [ "The Three Key Concepts", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md500", null ],
        [ "Why This Section Has No Audio Code", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md501", null ],
        [ "What You Should Internalize", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md502", null ]
      ] ],
      [ "Tutorial: Buffer Pipelines (Teaser)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md504", [
        [ "The Next Level", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md505", null ],
        [ "A Taste", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md506", null ],
        [ "Expansion 1: What Is a Pipeline?", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md507", null ],
        [ "Expansion 2: BufferOperation Types", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md508", null ],
        [ "Expansion 3: The <tt>on_capture_processing</tt> Pattern", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md509", null ],
        [ "Expansion 4: Why This Matters", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md510", null ],
        [ "What Happens Next", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md511", null ],
        [ "Try It (Optional)", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md513", null ],
        [ "The Philosophy", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md514", null ],
        [ "Next: The Full Pipeline Tutorial", "md_docs_2Tutorials_2SculptingData_2SculptingData.html#autotoc_md515", null ]
      ] ]
    ] ],
    [ "<strong>Visual Materiality: Part I</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html", [
      [ "<strong>Tutorial: Points in Space</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md518", [
        [ "Expansion 1: What Is PointNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md520", null ],
        [ "Expansion 2: GeometryBuffer Connects Node → GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md522", null ],
        [ "Expansion 3: setup_rendering() Adds Draw Calls", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md524", null ],
        [ "Expansion 4: Windowing and GLFW", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md526", null ],
        [ "Expansion 5: The Fluent API and Separation of Concerns", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md528", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md530", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md532", null ]
      ] ],
      [ "<strong>Tutorial: Collections and Aggregation</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md534", [
        [ "Expansion 1: What Is PointCollectionNode?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md537", null ],
        [ "Expansion 2: One Buffer, One Draw Call", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md539", null ],
        [ "Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md541", null ],
        [ "Expansion 4: Dynamic Rendering (Vulkan 1.3)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md543", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md545", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md547", null ]
      ] ],
      [ "<strong>Tutorial: Time and Updates</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md549", [
        [ "Expansion 1: No Draw Loop, <tt>compose()</tt> Runs Once", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md552", null ],
        [ "Expansion 2: Multiple Windows Without Offset Hacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md555", null ],
        [ "Expansion 3: Update Timing: Three Approaches", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md557", [
          [ "Approach 1: <tt>schedule_metro</tt> (Simplest)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md558", null ],
          [ "Approach 2: Coroutines with <tt>Sequence</tt> or <tt>EventChain</tt>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md560", null ],
          [ "Approach 3: Node <tt>on_tick</tt> Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md562", null ]
        ] ],
        [ "Expansion 4: Clearing vs. Replacing vs. Updating", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md565", [
          [ "Pattern 1: Additive Growth (Original Example)", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md566", null ],
          [ "Pattern 2: Full Replacement", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md568", null ],
          [ "Pattern 3: Selective Updates", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md570", null ],
          [ "Pattern 4: Conditional Clearing", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md572", null ]
        ] ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md574", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md576", null ]
      ] ],
      [ "<strong>Tutorial: Audio → Geometry</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md577", [
        [ "Expansion 1: What Are NodeNetworks?", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md579", null ],
        [ "Expansion 2: Parameter Mapping from Buffers", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md581", null ],
        [ "Expansion 3: NetworkGeometryBuffer Aggregates for GPU", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md583", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md585", null ]
      ] ],
      [ "<strong>Tutorial: Logic Events → Visual Impulse</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md587", [
        [ "Expansion 1: Logic Processor Callbacks", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md589", null ],
        [ "Expansion 2: Transient Detection Chain", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md591", null ],
        [ "Expansion 3: Per-Particle Impulse vs Global Impulse", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md593", null ],
        [ "Try It", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md595", null ]
      ] ],
      [ "<strong>Tutorial: Topology and Emergent Form</strong>", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md597", [
        [ "Expansion 1: Topology Types", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md599", null ],
        [ "Expansion 2: Material Properties as Audio Targets", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md601", null ],
        [ "What You've Learned", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md603", null ]
      ] ],
      [ "Conclusion", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md605", [
        [ "The Deeper Point", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md606", null ],
        [ "What Comes Next", "md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md608", null ]
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
"CoordUtils_8cpp.html#a881b55d1017bceb328a7ad40d357a510",
"EnergyAnalyzer_8hpp.html#a0fdcb54bbbc55da5e4974ccc18e56e4baa03a31a285bdb57fa3584642917b178c",
"GlfwWindow_8hpp_source.html",
"HIDBackend_8cpp.html",
"Keys_8hpp.html#a36a3852542b551e90c97c41c9ed56f62a105b296a83f9c105355403f3332af50f",
"ModalNetwork_8cpp_source.html",
"PathGeneratorNode_8cpp_a0954e589cd2387ad3eb9d600e5af9014.html#a0954e589cd2387ad3eb9d600e5af9014",
"Region_8hpp.html#aa113c7b24400a5fa1501d143f134744ca8c009c8dd4d4f9dde7515c00d5cd4661",
"StagingUtils_8hpp.html#ab91dc7fea50f8badadc42ab1cea40a06",
"UniversalExtractor_8hpp.html#a1b9e1e32a6d52cccf873bee90c60aa5caf6424358132ee9cae3868f7ce584ffe3",
"Windowing_8cpp.html#adc6ccccdab7a148f5b372e925320153d",
"classLila_1_1ClangInterpreter_ab8e94f44c75106129efdb53cb6bb478a.html#ab8e94f44c75106129efdb53cb6bb478a",
"classLila_1_1Subscription.html",
"classMayaFlux_1_1Buffers_1_1BufferInputControl_a1c92b06bc31c289cc4032f943ae301c2.html#a1c92b06bc31c289cc4032f943ae301c2",
"classMayaFlux_1_1Buffers_1_1BufferProcessingControl_a897f4e00381ea15fb609a86ec3f65df9.html#a897f4e00381ea15fb609a86ec3f65df9",
"classMayaFlux_1_1Buffers_1_1ComputeProcessor_a396a08372efa9e54744f1bc9a5244db3.html#a396a08372efa9e54744f1bc9a5244db3",
"classMayaFlux_1_1Buffers_1_1InputAccessProcessor_ac22a0c5077a3e855abfe508f81b76f3b.html#ac22a0c5077a3e855abfe508f81b76f3b",
"classMayaFlux_1_1Buffers_1_1NodeSourceProcessor_a154cd6e21740bcb7f9d1d4e75d7aecd9.html#a154cd6e21740bcb7f9d1d4e75d7aecd9",
"classMayaFlux_1_1Buffers_1_1RenderProcessor_ad2f13da0e8b431ece180cc85de29a599.html#ad2f13da0e8b431ece180cc85de29a599",
"classMayaFlux_1_1Buffers_1_1ShaderProcessor_ac5fc75dab25e733b65edc51c9b5510aa.html#ac5fc75dab25e733b65edc51c9b5510aa",
"classMayaFlux_1_1Buffers_1_1TextureProcessor_afbede3e8fe823d39af69c4fd0f882788.html#afbede3e8fe823d39af69c4fd0f882788",
"classMayaFlux_1_1Buffers_1_1VKBuffer_abe50725088ac159a3dccb159c0e73c34.html#abe50725088ac159a3dccb159c0e73c34",
"classMayaFlux_1_1Core_1_1BackendWindowHandler_a752427eb95e44c0e764d126d4f8eaeea.html#a752427eb95e44c0e764d126d4f8eaeea",
"classMayaFlux_1_1Core_1_1GlfwWindow_a0c2ba2fa2539f3d625fd40c00806f302.html#a0c2ba2fa2539f3d625fd40c00806f302",
"classMayaFlux_1_1Core_1_1HIDBackend_abf637973d3fcbc53182c297db8c50ced.html#abf637973d3fcbc53182c297db8c50ced",
"classMayaFlux_1_1Core_1_1InputSubsystem_a3dd69e344e66f1f17b961e6fb212d1ef.html#a3dd69e344e66f1f17b961e6fb212d1ef",
"classMayaFlux_1_1Core_1_1RtAudioStream_a838b8208585c81651e5feeb736348084.html#a838b8208585c81651e5feeb736348084",
"classMayaFlux_1_1Core_1_1VKContext_a41228db586851246809c40f704b64292.html#a41228db586851246809c40f704b64292",
"classMayaFlux_1_1Core_1_1VKGraphicsPipeline_a8b8ef4db64085f44422d2aea8dd3e010.html#a8b8ef4db64085f44422d2aea8dd3e010",
"classMayaFlux_1_1Core_1_1VKShaderModule_a57a7241ae4dfcbaa52cf3098ca587954.html#a57a7241ae4dfcbaa52cf3098ca587954",
"classMayaFlux_1_1Core_1_1WindowManager_aa8f06e54dee7a8e5bc2ad14e836432a3.html#aa8f06e54dee7a8e5bc2ad14e836432a3",
"classMayaFlux_1_1IO_1_1FileWriter_aa3f38358d30fae7f9659ef01f16cdefe.html#aa3f38358d30fae7f9659ef01f16cdefe",
"classMayaFlux_1_1Journal_1_1Archivist_1_1Impl_a3ba74f8d9ed376611aee28b8984b2818.html#a3ba74f8d9ed376611aee28b8984b2818",
"classMayaFlux_1_1Kakshya_1_1DataInsertion_afd3267ff190d889b84537ca75c7fff32.html#afd3267ff190d889b84537ca75c7fff32",
"classMayaFlux_1_1Kakshya_1_1RegionCacheManager_a6ad7998bdd45da88eeee009dde685829.html#a6ad7998bdd45da88eeee009dde685829",
"classMayaFlux_1_1Kakshya_1_1SoundStreamContainer_a38b7b8992accf0c0a1657450de63c2f6.html#a38b7b8992accf0c0a1657450de63c2f6",
"classMayaFlux_1_1Kakshya_1_1StructuredView_1_1iterator_ad6b00c99fee7753ee4a288cc168fbf2f.html#ad6b00c99fee7753ee4a288cc168fbf2f",
"classMayaFlux_1_1Kinesis_1_1Stochastic_1_1Stochastic_a8979e544684ae33d6648d9994c1770a6.html#a8979e544684ae33d6648d9994c1770a6",
"classMayaFlux_1_1Kriya_1_1BufferOperation_adbad4a55612f42b94f6ed892c61fd82b.html#adbad4a55612f42b94f6ed892c61fd82badceae9409e3ff24c6c0af99860ba44ac",
"classMayaFlux_1_1Kriya_1_1EventChain_a97768423d0178dadc6b59944461c4bef.html#a97768423d0178dadc6b59944461c4bef",
"classMayaFlux_1_1Nodes_1_1BinaryOpNode_a5b414570d2c7d97ac8b3ccc5dae7c3cd.html#a5b414570d2c7d97ac8b3ccc5dae7c3cd",
"classMayaFlux_1_1Nodes_1_1Filters_1_1Filter_afc14dbd3ccc6510669e6853d8134ad5d.html#afc14dbd3ccc6510669e6853d8134ad5d",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Logic_a79c2d22ab04d90d21286ecbc087bfaa1.html#a79c2d22ab04d90d21286ecbc087bfaa1",
"classMayaFlux_1_1Nodes_1_1Generator_1_1Polynomial_a622fa6eda4d3ac51970be6e6619a3c32.html#a622fa6eda4d3ac51970be6e6619a3c32",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1ComputeOutNode_a8511465b98edd57a7813acb7426162f1.html#a8511465b98edd57a7813acb7426162f1",
"classMayaFlux_1_1Nodes_1_1GpuSync_1_1PointCollectionNode_a649829e953dcb8596ef9ac7398d95539.html#a649829e953dcb8596ef9ac7398d95539",
"classMayaFlux_1_1Nodes_1_1Input_1_1HIDNode_aead79997c067cb55df35d99d61645983.html#aead79997c067cb55df35d99d61645983",
"classMayaFlux_1_1Nodes_1_1Network_1_1ModalNetwork_a8cc264ecf1cf57e0ac9336c3c92d5fef.html#a8cc264ecf1cf57e0ac9336c3c92d5fef",
"classMayaFlux_1_1Nodes_1_1Network_1_1ParticleNetwork_ae21648d0e655ecd69c7da634887c6000.html#ae21648d0e655ecd69c7da634887c6000",
"classMayaFlux_1_1Nodes_1_1Network_1_1PointCloudNetwork_a8c51a6028f5de39f80e605e1fa9066bf.html#a8c51a6028f5de39f80e605e1fa9066bf",
"classMayaFlux_1_1Nodes_1_1Network_1_1WaveguideNetwork_a8d599a17781b899051963d6a167a961a.html#a8d599a17781b899051963d6a167a961a",
"classMayaFlux_1_1Nodes_1_1Node_a34229a87c1855b9462c791b75bf21f1d.html#a34229a87c1855b9462c791b75bf21f1d",
"classMayaFlux_1_1Portal_1_1Graphics_1_1RenderFlow_a5caaefa7ac32568cf867e030ad65a48e.html#a5caaefa7ac32568cf867e030ad65a48e",
"classMayaFlux_1_1Portal_1_1Graphics_1_1ShaderFoundry_ab658d2764d9c73db471387661e1b972e.html#ab658d2764d9c73db471387661e1b972e",
"classMayaFlux_1_1Vruta_1_1Event.html",
"classMayaFlux_1_1Vruta_1_1GraphicsRoutine_adcbfe42eb4da5017995a1200e7a46b11.html#adcbfe42eb4da5017995a1200e7a46b11",
"classMayaFlux_1_1Vruta_1_1TaskScheduler_ad4b39210c37de598ba53560a32962246.html#ad4b39210c37de598ba53560a32962246",
"classMayaFlux_1_1Yantra_1_1ComputeOperation_ab37272995b9511bd2cb3ca2e06067832.html#ab37272995b9511bd2cb3ca2e06067832",
"classMayaFlux_1_1Yantra_1_1MathematicalTransformer_a16e2431e3167923d2763eba3593492de.html#a16e2431e3167923d2763eba3593492de",
"classMayaFlux_1_1Yantra_1_1StandardSorter_af4e5f28000163cedbc0fda0a1696f344.html#af4e5f28000163cedbc0fda0a1696f344",
"classMayaFlux_1_1Yantra_1_1UniversalMatcher_a16eff0bc115704c1a42baef74c6e18a6.html#a16eff0bc115704c1a42baef74c6e18a6",
"dir_898db27612e61ad38a12d92b37232b29.html",
"md_docs_2Digital__Transformation__Paradigm.html#autotoc_md199",
"md_docs_2Tutorials_2SculptingData_2VisualMateriality.html#autotoc_md574",
"namespaceMayaFlux_1_1IO_a36a3852542b551e90c97c41c9ed56f62.html#a36a3852542b551e90c97c41c9ed56f62a7548b0e7d73620adbd6b7a25c42acf43",
"namespaceMayaFlux_1_1Kakshya_a4d6dbfe4ade32fa9c748c6cf36988124.html#a4d6dbfe4ade32fa9c748c6cf36988124",
"namespaceMayaFlux_1_1Kinesis_afdb1ec2f9c871867b5a8e1e8ee7d488d.html#afdb1ec2f9c871867b5a8e1e8ee7d488d",
"namespaceMayaFlux_1_1Portal_1_1Graphics_a9fa38a89c88d0526a7773ffcd5ade928.html#a9fa38a89c88d0526a7773ffcd5ade928a19f22a1cbc2e2733484acbf9fc1bb4cd",
"namespaceMayaFlux_1_1Yantra_a566b0653178842b6393902f972de2cb6.html#a566b0653178842b6393902f972de2cb6",
"namespaceMayaFlux_1_1Yantra_af353cb8c2e8c36a11b87ac9d335184f0.html#af353cb8c2e8c36a11b87ac9d335184f0",
"namespaceMayaFlux_ada9c26211fe6eaddce876cd5d5a52b2c.html#ada9c26211fe6eaddce876cd5d5a52b2c",
"structMayaFlux_1_1Buffers_1_1DistributionDecision_a7118fa33725c7bd6443781b1066e3181.html#a7118fa33725c7bd6443781b1066e3181",
"structMayaFlux_1_1Core_1_1AttachmentDescription.html",
"structMayaFlux_1_1Core_1_1GlobalStreamInfo_a7cbcd312755179ca5797c0c0534647ce.html#a7cbcd312755179ca5797c0c0534647cea139882c654db8a57f7c3092de1dd0b02",
"structMayaFlux_1_1Core_1_1GraphicsPipelineConfig_af486fda9105da1c734a544e534cbaf2a.html#af486fda9105da1c734a544e534cbaf2a",
"structMayaFlux_1_1Core_1_1InputValue_1_1MIDIMessage.html",
"structMayaFlux_1_1Core_1_1SubpassDependency_a58202168332249b974c5a32cf79154b5.html#a58202168332249b974c5a32cf79154b5",
"structMayaFlux_1_1Core_1_1WindowRenderContext_acd383b94da084d71727d048d57770fe3.html#acd383b94da084d71727d048d57770fe3",
"structMayaFlux_1_1Kakshya_1_1ContainerDataStructure_af7ec021a3fad5238ce28b38052c3954e.html#af7ec021a3fad5238ce28b38052c3954e",
"structMayaFlux_1_1Kakshya_1_1RegionGroup_aaacf4ee621088c995c6cd4c74d9bd9e4.html#aaacf4ee621088c995c6cd4c74d9bd9e4",
"structMayaFlux_1_1Kriya_1_1GetPromiseBase_ad950f136421d418ef0278f71dca0cc89.html#ad950f136421d418ef0278f71dca0cc89",
"structMayaFlux_1_1Nodes_1_1LineVertex_a998652e2cbfe762ab208c347a43c56e8.html#a998652e2cbfe762ab208c347a43c56e8",
"structMayaFlux_1_1Portal_1_1Graphics_1_1RenderConfig_ac550e0fc123df7a91862073bfc1180e8.html#ac550e0fc123df7a91862073bfc1180e8",
"structMayaFlux_1_1Vruta_1_1EventFilter.html",
"structMayaFlux_1_1Yantra_1_1IO_aa6dcf6b6eabdda1c4360e3b6c875bc7c.html#aa6dcf6b6eabdda1c4360e3b6c875bc7c"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';