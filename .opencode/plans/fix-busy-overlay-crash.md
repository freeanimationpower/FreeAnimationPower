# Fix BusyOverlay crashes & disappearing overlay

## Diagnosis

### Crash cause
`showOverlay()` calls `parentWidget()->installEventFilter(this)` on EVERY call, but `hideOverlay()` calls `removeEventFilter(this)` only ONCE. After a re-emit of `busyStarted` (e.g., after modal dialog), the event filter is installed twice. The stale entry causes dangling pointer access when Qt processes events through the parent widget.

### Overlay disappearing
`emit busyStarted` re-asserts the overlay after the dialog, then `new VideoTrackWidget(...)` (now instant—no blocking probe inside). `emit busyFinished` runs microseconds later. The overlay's `show()`/`raise()` paint events are queued but NEVER processed before `hide()` is called.

## Fix 1: `busy_overlay.hpp` — Add `filterInstalled_` guard flag

```diff
     QColor spinnerColor_{255, 72, 0};
     QTimer* timer_ = nullptr;
     bool blocking_ = false;
+    bool filterInstalled_ = false;
```

## Fix 2: `busy_overlay.cpp` — Guard event filter install/uninstall

```diff
 void BusyOverlay::showOverlay()
 {
     if (parentWidget()) {
         setGeometry(parentWidget()->rect());
-        parentWidget()->installEventFilter(this);
+        if (!filterInstalled_) {
+            parentWidget()->installEventFilter(this);
+            filterInstalled_ = true;
+        }
     }
     QWidget::show();
     raise();
     setFocus();
-    timer_->start();
+    if (!timer_->isActive()) timer_->start();
     blocking_ = true;
 }

 void BusyOverlay::hideOverlay()
 {
     timer_->stop();
     blocking_ = false;
-    if (parentWidget()) {
+    if (filterInstalled_ && parentWidget()) {
         parentWidget()->removeEventFilter(this);
+        filterInstalled_ = false;
     }
     QWidget::hide();
 }
```

## Fix 3: `timeline_panel_v2.cpp` — processEvents before busyFinished (both import functions)

### In `onImportVideo()` (~line 2180):
```diff
     appState_->document().addVideoTrack(vt);

+    QApplication::processEvents();
     emit busyFinished();

     QTimer::singleShot(0, track, [track]() { track->update(); });
```

### In `onImportAudio()` (~line 2009):
```diff
     appState_->document().addAudioTrack(at);

+    QApplication::processEvents();
     emit busyFinished();

     QTimer::singleShot(0, track, [track]() {
         track->update();
     });
```

## Fix 4: `timeline_panel_v2.cpp` — addVideoTrackFromData use stored metadata (no probe during load)

```diff
 VideoTrackWidget* TimelinePanelV2::addVideoTrackFromData(const VideoTrackData& data)
 {
-    auto meta = fap::probeVideoMetadata(QString::fromStdString(data.filepath));
-    meta.valid = true; // trust saved data even if probe fails
-    if (meta.width == 0)   meta.width   = data.width;
-    if (meta.height == 0)  meta.height  = data.height;
-    if (meta.fps == 0.0)   meta.fps     = data.fps;
-    if (meta.totalFrames == 0) meta.totalFrames = data.totalFrames;
+    VideoMetadata meta;
+    meta.valid       = true;
+    meta.width       = data.width;
+    meta.height      = data.height;
+    meta.fps         = static_cast<double>(data.fps);
+    meta.totalFrames = data.totalFrames;

     auto* track = new VideoTrackWidget(
         QString::fromStdString(data.filepath),
         static_cast<int>(videoTrackWidgets_.size()),
         appState_, this, tracksContainer_, meta);
```

## Files affected

| File | Lines changed |
|------|--------------|
| `src/ui_v2/busy_overlay.hpp:33` | +1 bool flag |
| `src/ui_v2/busy_overlay.cpp:32-42,44-52` | eventFilter guard + timer guard |
| `src/ui_v2/timeline_panel_v2.cpp`(`onImportVideo`:~2181) | +1 `processEvents()` |
| `src/ui_v2/timeline_panel_v2.cpp`(`onImportAudio`:~2010) | +1 `processEvents()` |
| `src/ui_v2/timeline_panel_v2.cpp`(`addVideoTrackFromData`:~2188) | use Document metadata |
