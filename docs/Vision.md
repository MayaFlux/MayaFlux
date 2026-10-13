# Vision: seeing as material

Computer vision usually ends in a measurement. A frame goes in, and what comes out is a box, a label, a score, a list of points. The intermediate pictures (the blurred copy, the gradient, the threshold, the flow field) are scaffolding, and they are thrown away once the answer exists. The pipeline is a black box between pixels and a verdict, and the frame is treated as evidence.

MayaFlux keeps the scaffolding. The gray frame, the edge map, the motion field, the threshold mask, the cut-out element and the annotation are all images. You can put them on screen, record them into a growing collection, delay them, drift them against each other and mix them with any other source. An edge map is a drawing of where the picture changes its mind. A flow field is a colour wheel of everything that is moving. A threshold mask is a stencil you did not have to cut.

You can still treat vision as a black box when that is what you want: ask a question, take the answer, move on. The same machinery does both, and this page goes from the questions down to the machinery and then to the clock.

| You want | You reach for |
|---|---|
| An answer: where are the elements, how bright is it, is anything moving | `VisionMatrix::analysis()` |
| A picture of the answer, live on screen | `VisionWorkflow::process_to_buffer` |
| The pictures as material over time | `process_to_stream` and `process_to_live_container` |
| One stage, one image, full control of the steps | `VisionAnalyzer`, `VisionExtractor`, `VisionSequence` |
| A vision pipeline running on a buffer, frame by frame | `ImageCVProcessor` |

All of the live examples below read a camera, and the camera is always the one you choose when the program starts:

```cpp
auto camera = vega.read_camera() | Graphics;
auto frames = get_associated_buffer(camera);
```

`read_camera()` opens a picker for the camera and its capture mode, and blocks until you choose. Piping it into `Graphics` hooks it to a texture buffer, and `get_associated_buffer` gives you that buffer. Every snippet below that says `camera` or `frames` means these two. To open a fixed device instead, pass a config: `vega.read_camera({ .device_name = ... })`.

## Setup

```cpp
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/WorkflowIncludes.hpp"

using namespace MayaFlux::Kinesis::Vision;
using namespace MayaFlux::Yantra;
```

`WorkflowIncludes.hpp` brings in the vision workflow. Define `MAYAFLUX_WORKFLOW_NO_VISION` before including it to leave vision out.

---

## 1. Ask a question: queries and workflows

### A query says what you want

A `VisionQuery` is a set of intents, and you can ask for several things from one frame:

```cpp
VisionQuery query;
query.intents = VisionIntent::FindElements | VisionIntent::DetectEdges;
```

An intent set without parameters runs with its defaults. Set the matching field (`query.find_elements`, `query.detect_edges`, and so on) to tune it.

| Intent | What it looks for | What it leaves behind |
|---|---|---|
| `FindElements` | Discrete things: thresholded, cleaned, labelled, outlined | The threshold image, boxes, contours, shape measures |
| `TrackObjects` | Points followed from frame to frame, with identity | Tracks with arrows, centroid, bounds, speed and age |
| `DetectFeatures` | Distinctive points, with no claim they persist | Keypoints, centroid, bounds, the Harris response |
| `DetectEdges` | Boundaries and gradients | The Canny edge image |
| `EstimateMotion` | Dense motion over the whole frame, with no identity | The flow field and its colour-wheel picture |
| `MeasureAppearance` | The frame itself: brightness, gradient, histogram | The gray frame and the Sobel image |

`Classify`, `EstimateDepth`, `DetectAnomaly` and `Register` are named so queries can grow into them. They have no backing operation yet and fail clearly if you ask for one.

`TrackObjects` and `EstimateMotion` share one sequence, so asking for both costs less than the two separately. Everything else is its own sequence, and sequences that begin with the same steps share that work.

### One object for a query and what you take from it

A `VisionMatrix` holds the analyzer for a query and, if you give it a mode, an extractor. It keeps the last frame, its analysis and the last extraction, and every image each stage of the analysis made.

```cpp
auto matrix = Vision::VisionMatrix::create(query, VisionExtractMode::Crop);
matrix->extractor()->set_output_size(256, 256);
matrix->extractor()->set_element_focus(ElementFocus { .criterion = ElementCriterion::Area, .largest = true });

matrix->execute(image);
matrix->execute(texture_container, 3);

const auto& analysis = matrix->analysis();
const auto frame = matrix->frame();
```

`execute` takes a GPU image, or a frame of a container: a texture container's layer, a window's last frame or a video stream's frame. Keep the matrix for as long as frames arrive, because tracking and motion compare each frame with the one before.

The answer is in `analysis()`, in normalised coordinates: boxes, outlines, tracks and points are fractions of the frame, so they apply at any size.

```cpp
if (const auto& found = analysis.find_elements; found && !found->boxes.empty()) {
    const auto& box = found->boxes.front();
    const auto& shape = found->shapes.front();
}
```

The pictures are in `result(intent)`, named by what made them:

| Intent | Images in its result |
|---|---|
| `FindElements` | `gray` or `images.rgba_to_hsv`, and `images.threshold` |
| `DetectEdges` | `images.canny` (`gray` holds the edges too) |
| `MeasureAppearance` | `gray`, `images.sobel` |
| `DetectFeatures`, `TrackObjects` | `images.harris_response` (`gray` holds blurred gradients) |
| `EstimateMotion` | `flow`, `images.flow_visualization`, and `gray` when `TrackObjects` is not set |

```cpp
if (const auto edges = matrix->result(VisionIntent::DetectEdges)) {
    show(edges->images.canny);
}
if (const auto elements = matrix->result(VisionIntent::FindElements)) {
    show(elements->images.threshold);
}
```

A result you hold stays valid as long as you hold it. Its images are reused, so they show the newest analysis of that kind, and the same image keeps the same role from frame to frame. Attach one to something once and it follows the analysis.

An analysis can be applied to a different frame than the one it came from:

```cpp
matrix->extract(matrix->analysis(), other_image);
show(matrix->extraction());
```

### What you take from an analysis

The extractor turns an analysis into something you can use. Most modes produce a picture:

| Mode | What it makes |
|---|---|
| `Crop` | The focused element, or the bounds of the tracks or keypoints, cut out at `set_output_size` |
| `Crops` | Every element resampled to a tile and laid side by side in one atlas (`set_tile_size`) |
| `Silhouettes` | Every element cut by its contour polygon, holes included, on transparent |
| `Patches` | A fixed-size crop at every keypoint, else every track, in one atlas (`set_patch_size`) |
| `Equalize` | The frame histogram-equalised from the histogram `MeasureAppearance` measured |
| `Expose` | The frame scaled to reach a target mean brightness (`set_exposure_target`) |
| `Edges` | The frame kept where an edge is, transparent elsewhere (`set_edge_threshold`) |
| `MotionMask` | The frame kept where it is moving fast enough, with enough confidence (`set_motion_limits`) |
| `Annotate` | Everything found, drawn on the frame |

`Annotate` draws element boxes in yellow, contours in cyan, tracks as arrows (green while tracked, red when lost) with their bounds and centroid in white, and keypoints in magenta with their bounds.

Three modes return numbers instead of a picture: `Sample` (mean, min and max over the focused element), `SampleRegions` (the same for every element) and `SamplePoints` (the mean in a small window at every keypoint or track, with flow as the source it is the motion under each point). `SelectElement` gives the index of the element ranked first. A live workflow needs a mode that produces an image.

Which element the single-element modes act on is a choice you make with an `ElementFocus`: rank by `Area`, `Compactness`, `AspectRatio`, `Isolation` (distance to the nearest neighbour) or `Proximity` (closest to a point), highest first or lowest first.

### Running it live

`create_vision_workflow()` gives you a workflow on the engine. Hold it: its runs stop when it goes away.

```cpp
static auto vision = create_vision_workflow();

auto window = create_window({ .title = "edges", .width = 1280, .height = 720 });

auto edges = vision->process_to_buffer({
    .source = camera,
    .query = { .intents = VisionIntent::DetectEdges },
    .extract = VisionExtractMode::Edges,
    .render = { .target_window = window },
});
window->show();
```

The source can be a camera, a video file or a dynamic stream. A file that does not loop stops its run at its end.

Without a `LiveConfig`, the work sets the pace: the analysis moves forward a step every frame and delivers when it is done, without waiting on the GPU. With an interval, a frame is analyzed on a fixed beat:

```cpp
.live = Vision::LiveConfig { .interval_seconds = 1.0 / 15.0 },
```

You can also analyze one thing and apply the result to another. Track a hand on the camera and draw the tracks over a video:

```cpp
auto [video, audio] = choose_video({});
auto picture = get_io_manager()->hook_video_container_to_buffer(video);

auto annotated = vision->process_to_buffer({
    .source = camera,
    .query = query,
    .extract = VisionExtractMode::Annotate,
    .render = { .target_window = window },
    .live = Vision::LiveConfig { .apply_to = picture },
});
```

To configure the extraction before the run, make the matrix yourself and hand it over:

```cpp
auto matrix = Vision::VisionMatrix::create(query, VisionExtractMode::Silhouettes);
matrix->extractor()->set_tile_size(128, 128);

auto shapes = vision->process_to_buffer(matrix, camera, { .target_window = window });
```

Several runs can share one window. Give each its own region and draw priority so they layer instead of fighting:

```cpp
auto moving = vision->process_to_buffer({
    .source = camera,
    .query = { .intents = VisionIntent::EstimateMotion },
    .extract = VisionExtractMode::MotionMask,
    .render = { .target_window = window, .draw_priority = 1 },
});
auto annotated = vision->process_to_buffer({
    .source = camera,
    .query = tracks,
    .extract = VisionExtractMode::Annotate,
    .render = {
        .target_window = window,
        .scissor = Kinesis::Scissor::from(Kinesis::AABB2D { .min = { 0.55F, 0.55F }, .max = { 0.98F, 0.98F } }),
        .draw_priority = 2,
    },
});
```

The full window then shows only the pixels that are moving, on black, with the annotated frame in a corner.

### Recording what it makes

`process_to_live_container` records each extraction into a `TextureCollection` that grows on the GPU, up to a byte cap, or as a ring that keeps the newest frames:

```cpp
auto recording = vision->process_to_live_container({
    .source = camera,
    .query = { .intents = VisionIntent::DetectEdges },
    .extract = VisionExtractMode::Edges,
    .collection = { .ring_layers = 120 },
});
```

Each layer stays a GPU image. `to_image(i)` hands it back without a copy, and `get_layer_count()`, `get_write_head()` and `is_full()` tell you where the recording is.

`process_to_container` does the same over a list of images or a texture container's layers, in order, on the calling thread, with tracking and motion carried from frame to frame. It needs no engine.

### Vision as Chimera layers

`process_to_stream` runs a matrix live and hands you a Chimera builder. What goes into the layers is up to you: any image the run makes, the camera, stills, rings, anything else. Here is every stage of one run, side by side:

```cpp
VisionQuery query;
query.intents = VisionIntent::EstimateMotion | VisionIntent::MeasureAppearance | VisionIntent::DetectEdges;
query.estimate_motion = EstimateMotionParams { .flow = { .visualize = true } };
auto matrix = Vision::VisionMatrix::create(query, VisionExtractMode::Equalize);

auto stream = vision->process_to_stream({
    .source = camera,
    .matrix = matrix,
    .layers = { .width = 640, .height = 360, .ring_frames = 6 },
    .fit = Portal::Graphics::FitMode::STRETCH,
});
stream.get_buffer()->set_mode(3);

static auto chimera = stream.render({ .target_window = window, .fragment_shader = "texture_array_tile.frag" })
    .layer().layer().layer().layer().layer().layer()
    .every(1.0 / 60.0, [matrix, frames](Kriya::Chimera& c) {
        c.set(0, frames->get_gpu_texture());
        if (const auto look = matrix->result(VisionIntent::MeasureAppearance)) {
            c.set(1, look->gray);
            c.set(2, look->images.sobel);
        }
        if (const auto edges = matrix->result(VisionIntent::DetectEdges)) {
            c.set(3, edges->images.canny);
        }
        if (const auto motion = matrix->result(VisionIntent::EstimateMotion)) {
            c.set(4, motion->images.flow_visualization);
        }
        c.set(5, matrix->extraction());
    })
    .start();
```

Because the layers are yours, the analysis can play the composition. The flow field can fade in as the scene moves, and the edges grow as the light drops:

```cpp
.every(1.0 / 60.0, [matrix](Kriya::Chimera& c) {
    const auto& a = matrix->analysis();
    const double motion = std::clamp(a.motion_activity.value_or(0.0F) * 40.0, 0.0, 2.0);
    const double dark = 1.0 - (a.measure_appearance ? a.measure_appearance->mean_brightness : 0.5F);
    c.level(1, motion);
    c.level(2, 1.5 * dark);
})
```

Or the analysis can move the layers. A camera inset follows whatever is being tracked, and a threshold mask sits on the largest element (with the `texture_array_parts.frag` shader, which places each layer by its params):

```cpp
if (a.track_objects && a.track_objects->tracked_count > 0) {
    const auto centre = a.track_objects->centroid;
    c.params(2, { centre.x, centre.y, 0.25F, 0.0F });
}
if (a.find_elements && !a.find_elements->boxes.empty()) {
    const auto& box = a.find_elements->boxes.front();
    c.params(3, { box.x + box.w * 0.5F, box.y + box.h * 0.5F, std::max(box.w, box.h), 0.0F });
}
```

The extractor can change while it runs, so a layer can switch from equalized to edges to the moving parts on a schedule:

```cpp
.every(4.0, [matrix](Kriya::Chimera&) {
    matrix->extractor()->set_mode(VisionExtractMode::MotionMask);
})
```

Stopping or dropping the Chimera stops its vision run. A run that ends first, at the end of a file for example, leaves the Chimera running with its last images.

### Vision images over time

To put vision images in time, record them. A `record` entry pairs a `TextureCollection` with a function that picks an image from the matrix, and the run appends to all of them together each time an analysis completes. Read them back with lag, a lag that wanders, speed and cut:

```cpp
const auto ring = [] {
    return std::make_shared<Kakshya::TextureCollection>(
        Kakshya::TextureCollectionSpec { .width = 640, .height = 360, .ring_layers = 90 });
};
auto frames = ring();
auto edges = ring();
auto motion = ring();

auto stream = vision->process_to_stream({
    .source = camera,
    .matrix = matrix,
    .layers = { .width = 1280, .height = 720, .ring_frames = 5 },
    .record = {
        { frames, [](Vision::VisionMatrix& m) { return m.frame(); } },
        { edges, [](Vision::VisionMatrix& m) {
              const auto lane = m.result(VisionIntent::DetectEdges);
              return lane ? lane->images.canny : nullptr;
          } },
        { motion, [](Vision::VisionMatrix& m) {
              const auto lane = m.result(VisionIntent::EstimateMotion);
              return lane ? lane->images.flow_visualization : nullptr;
          } },
    },
    .live = Vision::LiveConfig { .interval_seconds = 1.0 / 30.0 },
});

static auto chimera = stream.render({ .target_window = window, .fragment_shader = "texture_array_ghost.frag" })
    .layer_data()
    .layer().from(frames, 30.0).lag(0.0).level(0.6)
    .layer().from(edges, 30.0).lag(0.0)
    .layer().from(edges, 30.0).lag(Kinesis::TimeMaps::triangle(0.3, 2.5, 0.3, 0.0)).level(0.8)
    .layer().from(motion, 30.0).speed(0.5).level(0.8)
    .layer().from(frames, 30.0).lag(1.0).level(0.4)
    .every(3.0, [](Kriya::Chimera& c) { c.cut(3, 2.0); })
    .start();
```

The frame and its edges at lag zero always change together, because they are appended together. When the work sets the pace instead of an interval, `matrix->analysis_rate()` reports how many analyses arrive per second, and setting a layer again with that rate keeps its lag in real seconds.

The same Chimera can also record the camera into a ring and cut its echo back whenever something moves:

```cpp
static auto chimera = stream.render({ .target_window = window, .fragment_shader = "texture_array_ghost.frag" })
    .layer_data()
    .record(Kriya::BufferOperation::capture_to_stream(get_io_manager(), camera, 240))
    .layer().from_pipeline().lag(0.0).level(0.5)
    .layer().from_pipeline().lag(Kinesis::TimeMaps::triangle(0.3, 1.5, 0.2, 0.0)).smooth().level(0.7)
    .every(0.5, [matrix](Kriya::Chimera& c) {
        if (matrix->analysis().motion_activity.value_or(0.0F) > 0.02F) {
            c.cut(1, 1.2);
        }
    })
    .start();
```

Chimera reads a `TextureCollection` directly, so this is not only for vision. Any collection being appended to can feed a layer, entirely on the GPU, with no download and no upload:

```cpp
auto collection = std::make_shared<Kakshya::TextureCollection>(
    Kakshya::TextureCollectionSpec { .width = 640, .height = 360, .ring_layers = 120 });

static auto chimera = create_chimera(array)
    .layer().from(collection).lag(0.0)
    .layer().from(collection).lag(0.5)
    .layer().from(collection).speed(0.5)
    .every(1.0 / 60.0, [collection, frames](Kriya::Chimera&) {
        collection->append(frames->get_gpu_texture());
    })
    .start();
```

The second argument of `from(collection, frame_rate)` says how many appends make a second, and is one per frame if you leave it out. `smooth()` does not apply to collections.

---

## 2. Take it apart: analysis and extraction

A `VisionMatrix` is a thin holder around two operations, and you can use them directly when you want to decide the order, hold several analyses, or put your own logic between them.

### The analyzer asks

`VisionAnalyzer` resolves its query into sequences, runs them on its own persistent GPU executor and reduces the answers on the GPU.

```cpp
VisionAnalyzer analyzer(query);

auto analysis = analyzer.analyze_vision(image);
```

- **`analyze_vision(image)`** takes a GPU image. The overload that takes a container and an index serves a texture container's layer, a video stream or a window.

- **`advance(image)`** does the same work across several calls, one sequence at a time, without waiting on the GPU for motion and tracking. It returns the analysis once every sequence has completed, and nothing before. The live runs use it when the work sets the pace.

- **`get_results()`** gives the `VisionResult` of every sequence, with each step's image named by its op. They stay valid until the next analysis. `get_shared_results()` returns them owned, so they outlive later analyses.

- **`reset()`** abandons outstanding work and clears retained state. Call it when the source changes: a camera switch, a video seek.

What an analysis holds, one field per active intent:

| Field | Contents |
|---|---|
| `find_elements` | `contours` (sorted by area when `max_contours` is set), `boxes`, `shapes` (compactness and aspect ratio per element), `nearest_neighbor_distance_px` |
| `track_objects` | `tracks`, `centroid`, `velocity`, `bounds`, `mean_speed_px`, `mean_age`, `max_age`, `tracked_count`, `lost_count` |
| `detect_features` | `keypoints`, `centroid`, `bounds`, `mean_response` |
| `measure_appearance` | `gradient` (a `FieldSample`), a 256-bin `histogram`, `mean_brightness` |
| `detect_edges`, `estimate_motion` | The Canny and flow images themselves |
| `motion_activity` | A single number for how much is moving |

The centroids, bounds and means are GPU reductions, not loops over a downloaded list.

You can steer which things count. `VisionAnalysisContext` narrows the tracked subject by region or by a track id that persists from frame to frame, and narrows feature detection to a region. The subject is whatever you decide it is: a previous result, a UI selection, another sensor.

### The extractor takes

`VisionExtractor` is the other half: it takes an analysis and a frame and makes the picture or the numbers. Each method is also a mode (section 1), but called directly it takes the data and the dimensions.

| Method | Works on |
|---|---|
| `crop`, `crops`, `silhouette`, `silhouettes`, `mask`, `sample_regions`, `select_element` | `find_elements` |
| `patches`, `sample_points` | tracks and keypoints |
| `equalize`, `expose` | `measure_appearance` |
| `edges` | `detect_edges` |
| `motion_mask` | `estimate_motion` |
| `annotate` | any field |

`mask` and `silhouettes` look alike and differ in what they need. `mask` is pixel-precise, read from the label buffer, so it needs the analyzer's executor and `export_label_buffer` set when you ran the analysis. `silhouettes` is cut from the contour polygons, needs neither, and works on an analysis you kept from earlier.

Image-producing methods share one output slot per size, so a returned image stays valid only until the next call that makes an image of the same size. Use it (display it, copy it) before extracting again.

### An example: cutting out skin

A colour range in HSV, run through the analyzer, with the largest element lifted out two ways. The full window shows the pixel-precise mask on black, so only the hand, arm or face remains. A corner shows the same element rebuilt from its contour polygon, holes included.

```cpp
VisionQuery query;
query.intents = VisionIntent::FindElements;
query.find_elements = FindElementsParams {
    .components = { .export_label_buffer = true },
    .contours = { .min_area = 0.0005F },
    .segmentation = {
        .space = SegmentationSpace::Hsv,
        .method = ThresholdParams {
            .value = 0.0F,
            .channels = ChannelMask::Ch0 | ChannelMask::Ch1 | ChannelMask::Ch2,
            .bands = { { { .lo = 0.93F, .hi = 0.14F }, { .lo = 0.10F, .hi = 0.90F }, { .lo = 0.15F, .hi = 1.0F } } },
        },
    },
};

auto matrix = Vision::VisionMatrix::create(query, VisionExtractMode::Silhouettes);
matrix->extractor()->set_element_focus(ElementFocus { .criterion = ElementCriterion::Area, .largest = true });

schedule_metro(0.1, [frames, matrix, cutout, polygon]() {
    const auto image = frames->get_gpu_texture();
    if (!image)
        return;

    const auto analysis = matrix->analyze(image);
    if (!analysis.find_elements || analysis.find_elements->boxes.empty())
        return;

    const auto& extractor = matrix->extractor();
    if (auto* executor = matrix->analyzer()->get_executor())
        if (const auto masked = extractor->mask(*executor, image, analysis, image->get_width(), image->get_height()))
            cutout->set_gpu_texture(masked);

    if (const auto filled = extractor->silhouette(image, analysis, 384, 384))
        polygon->set_gpu_texture(filled);
}, "", Vruta::ProcessingToken::FRAME_ACCURATE);
```

The hue band `0.93` to `0.14` wraps through red. The same query shape works for any colour: change the three bands.

### Letting shape choose

`ElementFocus` ranks elements on the GPU from the analyzer's own measures. Changing it between frames changes what the extractor lifts out, so something as small as an index can steer a whole picture:

```cpp
static const std::array<ElementFocus, 5> focuses { {
    { .criterion = ElementCriterion::Area, .largest = true },
    { .criterion = ElementCriterion::Compactness, .largest = true },
    { .criterion = ElementCriterion::AspectRatio, .largest = true },
    { .criterion = ElementCriterion::Isolation, .largest = true },
    { .criterion = ElementCriterion::Proximity, .largest = true, .point = { 0.5F, 0.5F } },
} };

matrix->extractor()->set_element_focus(focuses[which]);
```

Every few seconds the focus can move to the next criterion: the largest, the roundest, the most elongated, the most alone, the closest to the middle. `select_element(analysis, focus)` tells you which one it picked.

---

## 3. The pieces underneath: operations and sequences

Beneath a query is a catalog of operations and a way to chain them. An intent is a request ("find the edges"). A `VisionSequence` is the mechanism: an ordered list of named steps, each with its own parameters.

| Group | Operations |
|---|---|
| Colour and size | `RgbaToGray`, `RgbaToHsv`, `GrayToRgba`, `Downsample2x` |
| Thresholds | `Threshold` (per-channel bands), `ThresholdAdaptive`, `ThresholdOtsu` |
| Normalise | `NormalizeInplace`, `NormalizeRange` |
| Filters | `GaussianBlur`, `FilterSeparable` |
| Gradients and edges | `Sobel`, `Scharr`, `Canny` |
| Morphology | `Erode`, `Dilate`, `Open`, `Close`, `MorphGradient` |
| Segmentation | `ConnectedComponents`, `FindContours` |
| Features and tracking | `HarrisResponse`, `ExtractPeaks`, `TrackKeypoints` |
| Motion | `OpticalFlowDense` |
| Working image | `Snapshot` (copies the current image into the result), `Confine` (narrows to a region of the frame) |

You build a sequence with a builder, one call per step:

```cpp
auto sequence = VisionSequence::Builder {}
                    .downsample_2x()
                    .rgba_to_gray()
                    .harris_response()
                    .extract_peaks({ .threshold = 0.3F, .nms_radius = 8 })
                    .track_keypoints({ .error_threshold = 0.1F })
                    .build();
```

The pieces to know:

- **Backend.** `build()` defaults to the GPU executor. `build(VisionBackend::CPU)` selects the host executor instead. The choice applies to the whole sequence, with no mixing and no fallback between steps.

- **`defer()`.** Marks the most recent step so its GPU work is submitted without waiting on the fence. The sequence resumes on a later call once it signals. Dense flow and tracking are the steps that benefit.

- **Seeds.** `resolve(query)` turns a query into sequences, and a later sequence can start from an earlier sequence's image so a shared `RgbaToGray` is paid once.

- **Images by step.** Each step that makes a picture leaves it in `VisionResult::images`, named for its op (`sobel`, `canny`, `threshold_otsu`, `flow_visualization`, and so on). The gray frame is `VisionResult::gray`, and structured results (tracks, contours, keypoints) are in `VisionResult::structured`.

- **`Confine`.** Narrows everything after it to a normalised rectangle. Place it before `RgbaToGray` in a sequence with flow or tracking, because the flow pyramid is built from the gray frame at that point.

---

## 4. Realtime: run and buffer

The workflow is the convenient way to be live. Underneath it, a sequence runs on a frame in one of two ways: you call an executor yourself, or you attach a processor to a buffer and let the buffer chain call it.

### Run: an executor on a frame

`VisionGpuExecutor` is the Vulkan implementation. It keeps its inter-frame state (flow pyramids, tracked points) between calls, so hold one executor per independent pipeline and call `run()` every frame:

```cpp
Yantra::VisionGpuExecutor executor;
auto pass = executor.run(sequence, image, image->get_width(), image->get_height());
```

`Kinesis::Vision::VisionExecutor` is the host version: it takes normalised float pixels in host memory and gives host results. Reach for it when the data already lives on the host, when there is no GPU, or when you need a step only it implements. Handing it a GPU image means a transfer.

### Buffer: a processor on the camera

`ImageCVProcessor` attaches a sequence to a texture buffer. Each time the chain runs, it runs the sequence on the buffer's current image and keeps the result:

```cpp
auto sequence = VisionSequence::Builder {}
                    .downsample_2x()
                    .rgba_to_gray()
                    .optical_flow_dense({ .visualize = true })
                    .build();

auto vis = std::make_shared<Buffers::ImageCVProcessor<Buffers::VideoContainerBuffer>>(sequence);
add_processor(vis, frames, Buffers::ProcessingToken::GRAPHICS_BACKEND);
```

Read the result when you want it, or be told when it arrives:

- **`get_result()`** is the most recent `VisionResult`, default-initialised until the first run completes.

- **`get_result_source()`** is a broadcast signalled with every result. Attach it with `Kriya::on_signal` to consume results from a coroutine without polling.

- **`set_eval_rate(fps)`** throttles how often it evaluates. Vision is heavy enough that running it on every render frame is often wasted work, and an evaluation in flight cannot be reissued for the next frame, so this one processor throttles itself.

- **`set_sequence(sequence)`** replaces the pipeline and resets the inter-frame state. Call it only when idle.

### Showing what it sees

The dense flow field as a colour wheel, drawn live:

```cpp
auto display = vega.TextureBuffer(960, 540, Portal::Graphics::ImageFormat::RGBA32F) | Graphics;
display->setup_rendering({ .target_window = window });

schedule_metro(1.0 / 60.0, [display, vis]() {
    const auto& result = vis->get_result();
    if (result.images.flow_visualization)
        display->set_gpu_texture(result.images.flow_visualization);
}, "", Vruta::ProcessingToken::FRAME_ACCURATE);
```

A stencil beside it, from a multichannel Otsu threshold over the saturation and value channels of the HSV frame, in the same sequence as the flow:

```cpp
auto sequence = VisionSequence::Builder {}
                    .downsample_2x()
                    .rgba_to_hsv()
                    .optical_flow_dense({ .visualize = true })
                    .threshold_otsu({ .channels = ChannelMask::Ch1 | ChannelMask::Ch2 })
                    .build();
```

The flow is `result.images.flow_visualization` and the stencil is `result.images.threshold_otsu`.

The tracker as points. What the tracker sees can be drawn as nothing but dots, with the camera processed and never rendered. Tracks are normalised, so map them to the window:

```cpp
auto sequence = VisionSequence::Builder {}
                    .downsample_2x()
                    .rgba_to_gray()
                    .harris_response()
                    .extract_peaks({ .threshold = 0.3F, .nms_radius = 8 })
                    .track_keypoints({ .error_threshold = 0.1F })
                    .build();

auto vis = std::make_shared<Buffers::ImageCVProcessor<Buffers::VideoContainerBuffer>>(sequence);
add_processor(vis, frames, Buffers::ProcessingToken::GRAPHICS_BACKEND);

auto points = vega.PointCollectionNode(512) | Graphics;
auto geo = vega.GeometryBuffer(points) | Graphics;
geo->setup_rendering({ .target_window = window });

schedule_metro(1.0 / 60.0, [points, vis]() {
    const auto* tracks = std::get_if<std::vector<TrackResult>>(&vis->get_result().structured);
    if (!tracks)
        return;

    const auto ndc = [](const glm::vec2& p) { return glm::vec3(p.x * 2.0F - 1.0F, -(p.y * 2.0F - 1.0F), 0.0F); };

    std::vector<Kakshya::PointVertex> verts;
    for (const auto& track : *tracks) {
        if (track.tracked && verts.size() < 512)
            verts.push_back({ .position = ndc(track.position), .color = { 1.0F, 1.0F, 1.0F }, .size = 5.0F });
    }
    points->set_points(verts);
}, "", Vruta::ProcessingToken::FRAME_ACCURATE);
```

Each `TrackResult` carries its `position`, its `previous` position, a stable `id`, its `age` and its residual `error`. Draw `position + (position - previous) * 8` for a point that leads the motion.

### Which way to run

| Situation | Reach for |
|---|---|
| A picture of the analysis on screen, or recorded, or as layers | `VisionWorkflow` |
| You decide when to analyze and what to do between | `VisionMatrix` or `VisionAnalyzer` called from a metro |
| A fixed pipeline on a buffer, results read each frame | `ImageCVProcessor` |
| Your own loop, your own images | `VisionGpuExecutor::run()` |

---

## Things to try

- Analyze the camera and annotate a film with it, then record the annotated film into a ring and play it back at half speed behind the live one.
- Lay out every stage of one analysis in a grid, then swap which stage sits in which cell on a schedule.
- Let the brightness of the room decide how much of the edges you see, and the motion decide how much of the flow.
- Record the edges of a performance into a collection and read them back with three lags that drift apart.
- Put a small window of the camera on whatever is being tracked, and the threshold mask on the biggest thing in the frame.
- Change the extraction mode every few seconds and let one layer stutter at fifteen frames.
- Rank elements by `Isolation` and keep only the loneliest one, cut out as a silhouette.
- Draw the tracker as nothing but points, and let the points move something else.

## Notes

- **Coordinates are normalised.** Boxes, contours, tracks and keypoints are fractions of the frame, with the origin at the top left. Convert to window space (y flipped) when you draw them.

- **Source changes.** Call `reset()` on the analyzer when the source jumps: a camera switch, a video seek. Otherwise tracking compares frames that have nothing to do with each other.

- **Images are reused in place.** A result you hold keeps showing the newest analysis of its kind. An extraction image holds until the next extraction of the same size. If you need a still, copy it.

## Where next

Vision is one use of a general machine. `VisionMatrix` is a Yantra compute matrix, and `VisionAnalyzer` and `VisionExtractor` are Yantra operations, so everything that composes there composes here: pipelines, grammar-driven selection, async chains and the executors underneath. The [Yantra Compute Guide](GPGPU-Guide.md) covers that general layer, and it is the place to go to run your own shaders and operations over the same data. For how data crosses between the CPU and GPU at each of these levels, see the [CPU-GPU IO map](GPU-CPU-IO-Map.md).
