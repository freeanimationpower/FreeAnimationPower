# Paper Textures

Paper textures modulate brush opacity to simulate how traditional media
interacts with physical paper grain. The brush engine samples the texture
at each stamp position and multiplies the brush alpha by the sampled value.

## Image Requirements

- **Color space**: Grayscale (single-channel)
- **Tiling**: Must be seamlessly tileable (wrap horizontally and vertically)
- **Interpretation**: White = full opacity (no grain), Black = maximum grain
- **Recommended resolution**: 512x512 or 1024x1024 pixels
- **Formats**: PNG (preferred), BMP

## Expected Texture Files

| File                    | Description                                  |
|-------------------------|----------------------------------------------|
| `paper-smooth.png`      | Minimal grain, nearly white (smooth paper)   |
| `paper-rough.png`       | Medium grain, sketchbook / cartridge paper   |
| `paper-watercolor.png`  | Heavy irregular grain for watercolor effect  |
| `paper-canvas.png`      | Woven fabric pattern (canvas texture)        |

## How Paper Textures Work

1. The brush engine stamps the brush tip along the stroke path.
2. At each stamp position, the corresponding pixel from the paper texture
   is sampled (tiling wraps at texture edges).
3. The sampled grayscale value modulates the brush alpha:
   - `final_alpha = brush_alpha * (paper_value / 255)`
4. The result is composited onto the active layer.

## Creating Custom Paper Textures

1. Start with a square canvas at your chosen resolution.
2. Use noise filters, scanned paper sheets, or procedural generation.
3. Ensure the texture tiles seamlessly (use offset + clone-stamp in GIMP/PS).
4. Convert to grayscale and save as PNG.
5. Place in `resources/textures/` and reference by filename in the UI.

## Note

These files are placeholders. Replace them with actual grayscale PNG paper
textures before the brush engine can simulate traditional media.
