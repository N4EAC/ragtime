# Ragtime + Data2G branding assets

`ragtime-data2g-logo-transparent.png` is a transparent RGBA PNG for video
overlays and other compositing. It preserves the glass Earth, binary digits,
and Data2G/Ragtime badge from the approved macOS icon. The app tile and ground
reflection are removed. Keep the PNG's alpha channel when importing it into
a video editor; the globe and badge retain their own dark glass shading.

`ragtime-data2g-logo-monochrome-transparent.png` is the companion black,
white, and grayscale version, also with a transparent background. Both
files are 1254 × 1254 pixels.

Created with the built-in image generation tool, using
`app/res/ragtime-macos.png` as the edit target and transparent output enabled.

## Generation prompt

The color version's prompt follows below. The monochrome companion used
the same built-in image tool with the most recent transparent logo as its
edit target, transparency enabled, and this prompt:

> Edit the most recent transparent Earth/Data2G/Ragtime logo into a BLACK AND WHITE MONOCHROME VERSION. Preserve the exact logo composition, shape, globe size, recognizable continents, curved binary 0 and 1 digits with right-half fade, glass highlights and shading, overlapping lower-right rounded badge, and exact white badge text 'Data2G' above smaller 'Ragtime'. Change ONLY the color treatment to neutral black, white, and grayscale: luminous white/silver glass highlights, medium gray land and binary digits, charcoal gray ocean and badge shadows. Absolutely no teal, cyan, blue, green, or other color tint. Preserve REAL transparent alpha outside the combined globe and badge silhouette, including antialiased edges and subtle neutral white/gray semitransparent glow. Keep the globe and badge interiors shaded rather than punching holes through them. No background, no rounded-square app tile, no floor or ground reflection, no checkerboard baked into image pixels, no particles, no extra text, no watermark. This is a companion grayscale logo asset for video compositing, not a redesign.

Use case: background-extraction. Edit target: the provided approved Ragtime/Data2G macOS icon. Create a high-resolution transparent-background logo asset suitable for compositing onto videos, ideally 2048 by 2048 pixels. Remove ONLY the dark navy rounded-square app tile, its rounded border, all background outside the Earth and badge, and the ground reflection/shadow beneath the globe. Preserve the exact approved teal luminous 3D glass Earth, the recognizable continents, all integrated 0/1 binary digits with the existing right-half fading effect, and the exact lower-right overlapping rounded glass badge. Preserve exact badge text 'Data2G' in large white font and 'Ragtime' in smaller white font centered beneath INSIDE the badge. Both globe and badge stay in the same positions relative to each other and have the same design and proportions as input. Keep the globe's interior ocean shading and the badge's dark glass interior; do not punch holes through the globe or erase dark detail inside these objects. Outside their combined silhouette, use REAL fully transparent alpha, with clean antialiased edges and only very subtle semitransparent teal rim glow. No opaque background of any color, no tile, no floor, no shadow patch, no checkerboard baked into pixels, no new stars, no particles, no new text or watermark. Keep small transparent margins around the entire silhouette. This is the same approved logo artwork isolated as a reusable overlay, not a redesign.
