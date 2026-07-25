#!/usr/bin/env python3
"""Linux Universal Upscaler & Frame Gen - GUI entry point.

Primary flow, deliberately minimal: choose a window, optionally toggle
Frame Generation, hit Start. Everything else (upscale mode, quality,
capture_output, hotkey) lives behind "Advanced..." for anyone who wants
it, but isn't needed for the common case. The Start button launches the
C++ luu_capture_preview binary, which reads its settings from the same
~/.config/luu/settings.json this GUI writes.
"""

from __future__ import annotations

import logging
import sys
from pathlib import Path

from PyQt6.QtCore import QProcess
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from hotkey_listener import HotkeyListener
from settings_ui import ConfigManager, SettingsDialog, WindowPickerDialog

# build/src/render/luu_capture_preview relative to the repo root (this file
# lives at src/gui/main.py).
CAPTURE_BINARY = Path(__file__).resolve().parents[2] / "build" / "src" / "render" / "luu_capture_preview"

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("luu.main")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Linux Universal Upscaler & Frame Gen")
        self.setMinimumSize(420, 200)

        self.config = ConfigManager()
        self.config.load()

        self.capture_process: QProcess | None = None
        self._stopping_capture = False

        self._picked_window_id = self.config.get("capture_window_id", "")
        self._picked_window_title = self.config.get("capture_window_title", "")
        self._picked_window_app_id = self.config.get("capture_window_app_id", "")

        self.window_label = QLabel()
        self.choose_window_button = QPushButton("Choose window...")
        self.choose_window_button.clicked.connect(self._on_choose_window)
        window_row = QHBoxLayout()
        window_row.addWidget(self.window_label, 1)
        window_row.addWidget(self.choose_window_button)

        # Frame gen only ever has one real implementation
        # (framegen_method: "interpolation") - "lsfg" is still just a
        # placeholder name for a future real one, see settings_ui.py. This
        # checkbox skips that distinction entirely: checked always means
        # "interpolation".
        self.framegen_check = QCheckBox("Frame Generation")
        self.framegen_check.setChecked(
            bool(self.config.get("frame_gen_enabled", True))
            and self.config.get("framegen_method", "lsfg") == "interpolation"
        )

        self.status_label = QLabel()
        self.toggle_button = QPushButton("Start Preview")
        self.toggle_button.clicked.connect(self._toggle_enabled)
        self.advanced_button = QPushButton("Advanced...")
        self.advanced_button.clicked.connect(self._open_settings)
        button_row = QHBoxLayout()
        button_row.addWidget(self.toggle_button)
        button_row.addWidget(self.advanced_button)

        layout = QVBoxLayout()
        layout.addLayout(window_row)
        layout.addWidget(self.framegen_check)
        layout.addWidget(self.status_label)
        layout.addLayout(button_row)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        self.hotkey_listener = HotkeyListener(self.config.get("hotkey", "<alt>+u"), parent=self)
        self.hotkey_listener.triggered.connect(self._toggle_enabled)
        self.hotkey_listener.error.connect(self._on_hotkey_error)

        self._refresh_window_label()
        self._refresh_labels()
        self._start_hotkey_listener()

    def _start_hotkey_listener(self) -> None:
        # Known broken on native Wayland (pynput can't grab global hotkeys
        # there) - started anyway for X11 sessions, but not required for
        # the primary window+button flow.
        self.hotkey_listener.start()

    def _refresh_window_label(self) -> None:
        if self._picked_window_id:
            self.window_label.setText(f"{self._picked_window_title} — {self._picked_window_app_id}")
        else:
            self.window_label.setText("No window chosen")

    def _refresh_labels(self) -> None:
        running = self.capture_process is not None
        self.status_label.setText(f"Status: {'PREVIEW RUNNING' if running else 'idle'}")
        self.toggle_button.setText("Stop Preview" if running else "Start Preview")

    def _on_choose_window(self) -> None:
        picker = WindowPickerDialog(parent=self)
        if picker.exec() and picker.picked:
            self._picked_window_id = picker.picked.get("identifier", "")
            self._picked_window_title = picker.picked.get("title", "")
            self._picked_window_app_id = picker.picked.get("app_id", "")
            self._refresh_window_label()

    def _toggle_enabled(self) -> None:
        if self.capture_process is not None:
            # terminate() delivers SIGTERM, which Qt reports as a "crash" -
            # remember this was requested so the error handler doesn't show
            # a scary dialog for a stop the user asked for.
            self._stopping_capture = True
            self.capture_process.terminate()
        else:
            self._start_capture_preview()

    def _start_capture_preview(self) -> None:
        if not CAPTURE_BINARY.exists():
            QMessageBox.critical(
                self,
                "luu_capture_preview not found",
                f"Could not find:\n{CAPTURE_BINARY}\n\n"
                "Build it first with ./scripts/build.sh from the repo root.",
            )
            return

        # A picked window takes priority; otherwise fall back to whatever
        # capture_target Advanced settings already configured (e.g. a
        # monitor via capture_output), so that path still works.
        if self._picked_window_id:
            self.config.set("capture_target", "window")
            self.config.set("capture_window_id", self._picked_window_id)
            self.config.set("capture_window_title", self._picked_window_title)
            self.config.set("capture_window_app_id", self._picked_window_app_id)
        elif self.config.get("capture_target", "output") == "window":
            QMessageBox.warning(self, "No window chosen", "Choose a window first.")
            return

        self.config.set("frame_gen_enabled", self.framegen_check.isChecked())
        if self.framegen_check.isChecked():
            self.config.set("framegen_method", "interpolation")

        if not self.config.save():
            QMessageBox.critical(
                self,
                "Save failed",
                f"Could not write settings to {self.config.path}.\n"
                "Check that the directory is writable.",
            )
            return

        proc = QProcess(self)
        proc.setProgram(str(CAPTURE_BINARY))
        proc.finished.connect(self._on_capture_finished)
        proc.errorOccurred.connect(self._on_capture_error)
        proc.start()
        self.capture_process = proc
        logger.info("Started capture preview: %s", CAPTURE_BINARY)
        self._refresh_labels()

    def _on_capture_finished(self, exit_code: int, _exit_status) -> None:
        logger.info("Capture preview exited (code=%d)", exit_code)
        self.capture_process = None
        self._stopping_capture = False
        self._refresh_labels()

    def _on_capture_error(self, error) -> None:
        if self._stopping_capture:
            logger.info("Capture preview stopped")
        else:
            logger.error("Capture preview process error: %s", error)
            QMessageBox.warning(
                self, "Capture preview error", f"luu_capture_preview failed to run ({error})."
            )
        self.capture_process = None
        self._stopping_capture = False
        self._refresh_labels()

    def _open_settings(self) -> None:
        dialog = SettingsDialog(self.config, parent=self)
        if dialog.exec():
            new_hotkey = self.config.get("hotkey", "<alt>+u")
            if new_hotkey != self.hotkey_listener.hotkey:
                self.hotkey_listener.set_hotkey(new_hotkey)
            # Advanced may have changed the window/frame-gen state too -
            # re-sync the main window's controls with what got saved.
            self._picked_window_id = self.config.get("capture_window_id", "")
            self._picked_window_title = self.config.get("capture_window_title", "")
            self._picked_window_app_id = self.config.get("capture_window_app_id", "")
            self.framegen_check.setChecked(
                bool(self.config.get("frame_gen_enabled", True))
                and self.config.get("framegen_method", "lsfg") == "interpolation"
            )
            self._refresh_window_label()
            self._refresh_labels()

    def _on_hotkey_error(self, message: str) -> None:
        logger.error(message)

    def closeEvent(self, event) -> None:
        self.hotkey_listener.stop()
        if self.capture_process is not None:
            self.capture_process.terminate()
            self.capture_process.waitForFinished(1000)
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
