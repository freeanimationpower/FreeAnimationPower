# Brush Tips — Default Set

Brush tip images define the stamp shape used by the raster brush engine.
Each stamp is stamped repeatedly along the stroke path to produce a continuous mark.

## Image Requirements

- **Color space**: Grayscale (single-channel)
- **Interpretation**: White = fully opaque, Black = fully transparent
  (The alpha channel is derived from the grayscale value)
- **Recommended resolution**: 256x256 or 512x512 pixels
- **Format**: PNG (lossless, supports transparency)

## Expected Brush Tip Files

| File               | Description                                      |
|--------------------|--------------------------------------------------|
| `round-soft.png`   | Soft round brush with feathered edges            |
| `round-hard.png`   | Hard round brush with sharp edges                |
| `flat.png`         | Flat/rectangular tip (angled, low roundness)     |
| `calligraphy.png`  | Calligraphy nib shape (slanted thin ellipse)     |

## Creating Custom Brush Tips

1. Create a grayscale image in your preferred editor (Krita, GIMP, Photoshop).
2. Paint the brush shape in **white** on a **black** background.
3. Softer brushes use a Gaussian blur for feathered edges.
4. Save as PNG and place in a new directory under `resources/brushes/`.
5. Reference the directory in brush preset files via the `tip_path` field.

## Note

These files are placeholders. Replace them with actual grayscale PNG brush tip
images before using the brush engine.
