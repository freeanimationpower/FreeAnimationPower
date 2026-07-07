# Free Animation Power — Domain Context

## Domain Language

- **Canvas**: The drawing surface where animation frames are created
- **Frame**: A single image in the animation sequence
- **Layer**: An independent drawing level within a frame, composited bottom-to-top
- **Stroke**: A continuous mark made by a brush tool, defined by a series of points
- **Brush Preset**: A saved configuration of brush parameters (size, shape, dynamics)
- **Paper Texture**: A grayscale image that modulates brush opacity to simulate traditional media
- **Onion Skin**: Semi-transparent display of previous/next frames for animation reference
- **Timeline**: The temporal organization of frames and keyframes
- **Exposure**: How many frames a single drawing is displayed (on ones, twos, threes)
- **Keyframe**: A frame where a significant change occurs (new drawing or transform)
- **Inbetween**: A frame generated between two keyframes
- **Raster/Bitmap**: Pixel-based graphics representation
- **Vector**: Path-based graphics using mathematical curves (Bezier)
- **Blend Mode**: Algorithm determining how overlapping pixels combine
- **Rigging**: Creating a skeletal structure for cut-out animation
- **IK (Inverse Kinematics)**: Computing joint rotations from end-effector position

## Core Entities

- **Document**: Top-level container for an animation project (canvas size, FPS, layers, frames)
- **Layer**: Holds frame data; types: Raster, Vector, Group, Audio, Camera
- **Timeline**: Manages frame sequencing, playback, and keyframe interpolation
- **BrushEngine**: Processes input events into rendered strokes on layers
- **CanvasWidget**: Renders the viewport, handles zoom/pan/rotate, dispatches input

## Architecture Decisions

1. **Hybrid Vector+Raster**: Both representations coexist; layers choose their type
2. **CPU Rendering for Brush**: Brush strokes are rendered on CPU then uploaded as textures (like TVPaint)
3. **Qt 6 for UI**: Cross-platform native widgets with dark theme
4. **ZIP-based File Format**: .fap uses ZIP container with PNG frames + JSON metadata
5. **FFmpeg for Export**: Shell out to FFmpeg for video encoding
