#pragma once

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QtSvg/QSvgRenderer>
#include <QByteArray>
#include <cstring>

namespace fap::icons {

inline QIcon makeIcon(const char* svgData) {
    QIcon icon;
    QSvgRenderer renderer;
    renderer.load(QByteArray::fromRawData(svgData, static_cast<int>(std::strlen(svgData))));
    for (int size : {16, 24, 32}) {
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        renderer.render(&p);
        p.end();
        icon.addPixmap(pix);
    }
    return icon;
}

/* ============================================================
   A. LEFT TOOLBAR — Drawing & Manipulation Tools
   ============================================================ */

// 1. Brush — 45deg paintbrush: handle + ferrule + wedge bristles
inline QIcon Brush() {
    return makeIcon("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'><g transform='rotate(-45 12 12)'><rect x='10' y='3' width='6' height='12' rx='3'/><rect x='10' y='15' width='6' height='3' rx='1' fill='currentColor' stroke='none'/><path d='M13 17v5l-6-2 3-4z'/></g></svg>");
}

// 2. Eraser — rotated block with sleeve line
inline QIcon Eraser() {
    return makeIcon("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'><g transform='rotate(-40 12 12)'><rect x='2' y='5' width='20' height='14' rx='4'/><line x1='9' y1='3' x2='8' y2='20'/><line x1='14' y1='3' x2='13' y2='20' opacity='.3'/></g></svg>");
}

// 3. PickColor — classic eyedropper
inline QIcon PickColor() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m17.3 2.7 4 4L19.7 8.3l-4-4z"/>
  <path d="m10.4 9.6-7 7a2 2 0 0 0 0 2.8l1.2 1.2a2 2 0 0 0 2.8 0l7-7"/>
  <path d="m15.9 4.1-1.5 1.5 4 4 1.5-1.5"/>
  <path d="M2 22h20"/>
</svg>)");
}

// 4. Fill — tipped paint bucket with single drop
inline QIcon Fill() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M2.9 15.1 12.5 2.5l4 4L7 17H3l-.1-1.9z"/>
  <path d="m13 6-3 4"/>
  <path d="M18 16.5A2.5 2.5 0 0 1 20.5 14c0-1.5-2.5-4-2.5-4s-2.5 2.5-2.5 4a2.5 2.5 0 0 0 2.5 2.5z"/>
</svg>)");
}

// 5. Text — clean capital T
inline QIcon Text() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 4h16"/>
  <path d="M12 4v14"/>
  <path d="M7 20h10"/>
</svg>)");
}

// 6. Line — diagonal with circular endpoints
inline QIcon Line() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <line x1="5.5" y1="18.5" x2="18.5" y2="5.5"/>
  <circle cx="5.5" cy="18.5" r="2" fill="currentColor"/>
  <circle cx="18.5" cy="5.5" r="2" fill="currentColor"/>
</svg>)");
}

// 7. Rectangle — clean outlined box
inline QIcon Rectangle() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
</svg>)");
}

// 8. Ellipse — clean outlined circle
inline QIcon Ellipse() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="12" cy="12" r="9"/>
</svg>)");
}

// 9. Move — four-way arrow cross
inline QIcon Move() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M12 2v20M3 12h18"/>
  <path d="m7 7 5-5 5 5"/>
  <path d="m7 17 5 5 5-5"/>
  <path d="m17 7 5 5-5 5"/>
  <path d="m7 7-5 5 5 5"/>
</svg>)");
}

// 10. Select — dashed rectangle marquee
inline QIcon Select() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="4" y="4" width="16" height="16" rx="1" stroke-dasharray="3 3"/>
  <path d="m4 2 3-2"/>
</svg>)");
}

// 11. Hand — palm box + 4 finger pills + thumb pill (clean pan cursor)
inline QIcon Hand() {
    return makeIcon("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.5' stroke-linecap='round' stroke-linejoin='round'><rect x='7' y='11' width='10' height='9' rx='4'/><rect x='8' y='3' width='3' height='9' rx='1.5'/><rect x='12' y='2' width='3' height='10' rx='1.5'/><rect x='16' y='4' width='3' height='8' rx='1.5'/><rect x='4' y='7' width='3' height='7' rx='1.5'/><rect x='4' y='13' width='4' height='3.5' rx='1.5'/></svg>");
}

/* ============================================================
   B. RIGHT PANEL — Layer Operations
   ============================================================ */

// 12. Layer — two stacked transparent sheets
inline QIcon Layer() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="14" height="14" rx="1.5" stroke-dasharray="2 2" opacity=".4"/>
  <rect x="7" y="7" width="14" height="14" rx="1.5"/>
  <line x1="7" y1="12" x2="16" y2="12"/>
</svg>)");
}

// 13. Duplicate — overlapping pages with + badge
inline QIcon Duplicate() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="13" height="13" rx="1.5"/>
  <rect x="8" y="8" width="13" height="13" rx="1.5"/>
  <line x1="14.5" y1="11" x2="14.5" y2="18"/>
  <line x1="11" y1="14.5" x2="18" y2="14.5"/>
</svg>)");
}

// 14. MoveUp — chevron up
inline QIcon MoveUp() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="m18 15-6-6-6 6"/>
</svg>)");
}

// 15. MoveDown — chevron down
inline QIcon MoveDown() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="m6 9 6 6 6-6"/>
</svg>)");
}

// 16. Delete — classic UI trash can (no crumpled paper)
inline QIcon Delete() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M3 6h18"/>
  <path d="M8 6V4.5A1.5 1.5 0 0 1 9.5 3h5A1.5 1.5 0 0 1 16 4.5V6"/>
  <path d="M19 6v13a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"/>
  <line x1="10" y1="11" x2="10" y2="17"/>
  <line x1="14" y1="11" x2="14" y2="17"/>
</svg>)");
}

/* ============================================================
   C. TOP BAR — View Controls
   ============================================================ */

// 17. ZoomIn — magnifying glass with +
inline QIcon ZoomIn() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="11" cy="11" r="7"/>
  <path d="m16 16 6 6"/>
  <path d="M11 8v6M8 11h6"/>
</svg>)");
}

// 18. ZoomOut — magnifying glass with -
inline QIcon ZoomOut() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="11" cy="11" r="7"/>
  <path d="m16 16 6 6"/>
  <path d="M8 11h6"/>
</svg>)");
}

// 19. Fit — fit-to-screen (outward corner arrows)
inline QIcon Fit() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M8 3H5a2 2 0 0 0-2 2v3m13-5h3a2 2 0 0 1 2 2v3M8 21H5a2 2 0 0 1-2-2v-3m13 5h3a2 2 0 0 0 2-2v-3"/>
</svg>)");
}

// 20. Flip — horizontal mirror
inline QIcon Flip() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M12 3v18"/>
  <path d="m12 3 4 7-4-2-4 2z"/>
  <path d="m12 21 4-7-4 2-4-2z"/>
</svg>)");
}

// 21. Rot — circular rotation arrow
inline QIcon Rot() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M21 12a9 9 0 0 0-8-8.9V6"/>
  <path d="M3 12a9 9 0 0 1 15.4-6.4L22 9"/>
  <path d="M22 3v6h-6"/>
</svg>)");
}

// 22. Grid — 3x3 grid with outline
inline QIcon Grid() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
  <line x1="12" y1="3" x2="12" y2="21"/>
  <line x1="3" y1="12" x2="21" y2="12"/>
</svg>)");
}

/* ============================================================
   D. BOTTOM PANEL — Timeline
   ============================================================ */

// 23. Frame — filmstrip frame
inline QIcon Frame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="5" width="18" height="14" rx="1.5"/>
  <line x1="3" y1="9" x2="21" y2="9"/>
</svg>)");
}

// 24. Keyframe — diamond
inline QIcon Keyframe() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="currentColor" stroke-width="0.5" stroke-linejoin="round">
  <path d="M12 2 22 12 12 22 2 12z"/>
</svg>)");
}

// 25. Play — solid triangle
inline QIcon Play() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="none">
  <path d="M7 3.5v17l12-8.5z"/>
</svg>)");
}

// 26. Pause — two solid bars
inline QIcon Pause() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="none">
  <rect x="5" y="4" width="4.5" height="16" rx="1"/>
  <rect x="14.5" y="4" width="4.5" height="16" rx="1"/>
</svg>)");
}

// 27. Stop — solid square
inline QIcon Stop() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="none">
  <rect x="5" y="5" width="14" height="14" rx="2"/>
</svg>)");
}

// 28. PrevFrame — vertical bar + left-pointing triangle
inline QIcon PrevFrame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="none">
  <rect x="3" y="5" width="3" height="14" rx="1"/>
  <path d="M8 12 19 5v14z"/>
</svg>)");
}

// 29. NextFrame — right-pointing triangle + vertical bar
inline QIcon NextFrame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" stroke="none">
  <path d="M5 5 16 12 5 19z"/>
  <rect x="18" y="5" width="3" height="14" rx="1"/>
</svg>)");
}

// 30. NewEmptyFrame — square with + inside
inline QIcon NewEmptyFrame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
  <line x1="12" y1="8" x2="12" y2="16"/>
  <line x1="8" y1="12" x2="16" y2="12"/>
</svg>)");
}

// 31. DuplicateFrame — two offset squares with + badge
inline QIcon DuplicateFrame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="2" y="2" width="15" height="15" rx="1.5"/>
  <rect x="7" y="7" width="15" height="15" rx="1.5"/>
  <line x1="14.5" y1="11" x2="14.5" y2="18"/>
  <line x1="11" y1="14.5" x2="18" y2="14.5"/>
</svg>)");
}

// 32. DeleteFrame — square with X inside
inline QIcon DeleteFrame() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
  <line x1="8" y1="8" x2="16" y2="16"/>
  <line x1="16" y1="8" x2="8" y2="16"/>
</svg>)");
}

// 33. NewLayer — single sheet with + in center
inline QIcon NewLayer() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
  <line x1="12" y1="8" x2="12" y2="16"/>
  <line x1="8" y1="12" x2="16" y2="12"/>
</svg>)");
}

// 34. LockClosed — padlock closed
inline QIcon LockClosed() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="5" y="11" width="14" height="10" rx="2"/>
  <path d="M8 11V7a4 4 0 0 1 8 0v4"/>
</svg>)");
}

// 35. LockOpen — padlock open (shackle detached)
inline QIcon LockOpen() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="5" y="11" width="14" height="10" rx="2"/>
  <path d="M8 11V7a4 4 0 0 1 5-3.9"/>
  <path d="M16 7v4"/>
</svg>)");
}

// 36. Eye — visibility toggle
inline QIcon Eye() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
  <circle cx="12" cy="12" r="3"/>
</svg>)");
}

// 37. ImportImage — picture with down arrow
inline QIcon ImportImage() {
    return makeIcon(R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="18" height="18" rx="2"/>
  <circle cx="8.5" cy="8.5" r="1.5"/>
  <polyline points="21 15 16 10 5 21"/>
  <line x1="12" y1="18" x2="12" y2="5"/>
  <polyline points="9 15 12 18 15 15"/>
</svg>)");
}

/* ============================================================
   Utility
   ============================================================ */

inline QIcon tinted(const QIcon& base, const QColor& color) {
    QIcon result;
    for (const QSize& s : base.availableSizes()) {
        QPixmap pix = base.pixmap(s);
        QPainter p(&pix);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pix.rect(), color);
        p.end();
        result.addPixmap(pix);
    }
    return result;
}

} // namespace fap::icons
