#!/usr/bin/env python3
"""Linux Universal Upscaler & Frame Gen - GUI entry point.

Minimal main window: shows status, lets the user trigger a capture preview
via a global hotkey or button, and opens the settings dialog. The hotkey/
button launches the C++ luu_capture_preview binary (Phase 2 MVP: a
single-shot screencopy -> GL upscale -> window pipeline), which reads its
settings from the same ~/.config/luu/settings.json this GUI writes.
"""

from __future__ import annotations

import logging
import sys
from pathlib import Path

from PyQt6.QtCore import QProcess
from PyQt6.QtWidgets import (
    QApplication,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from hotkey_listener import HotkeyListener
from settings_ui import ConfigManager, SettingsDialog

# build/src/render/luu_capture_preview relative to the repo root (this file
# lives at src/gui/main.py).
CAPTURE_BINARY = Path(__file__).resolve().parents[2] / "build" / "src" / "render" / "luu_capture_preview"

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("luu.main")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Linux Universal Upscaler & Frame Gen")
        self.setMinimumSize(360, 220)

        self.config = ConfigManager()
        self.config.load()

        self.capture_process: QProcess | None = None
        self._stopping_capture = False

        self.status_label = QLabel()
        self.hotkey_label = QLabel()
        self.toggle_button = QPushButton("Start Preview")
        self.settings_button = QPushButton("Settings")

        layout = QVBoxLayout()
        layout.addWidget(self.status_label)
        layout.addWidget(self.hotkey_label)
        layout.addWidget(self.toggle_button)
        layout.addWidget(self.settings_button)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        self.toggle_button.clicked.connect(self._toggle_enabled)
        self.settings_button.clicked.connect(self._open_settings)

        self.hotkey_listener = HotkeyListener(self.config.get("hotkey", "<alt>+u"), parent=self)
        self.hotkey_listener.triggered.connect(self._toggle_enabled)
        self.hotkey_listener.error.connect(self._on_hotkey_error)

        self._refresh_labels()
        self._start_hotkey_listener()

    def _start_hotkey_listener(self) -> None:
        if not self.hotkey_listener.start():
            self.hotkey_label.setText("Hotkey: unavailable (see error)")

    def _refresh_labels(self) -> None:
        running = self.capture_process is not None
        self.status_label.setText(f"Status: {'PREVIEW RUNNING' if running else 'idle'}")
        self.toggle_button.setText("Stop Preview" if running else "Start Preview")
        self.hotkey_label.setText(f"Hotkey: {self.hotkey_listener.hotkey}")

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
            self._refresh_labels()

    def _on_hotkey_error(self, message: str) -> None:
        logger.error(message)
        QMessageBox.warning(self, "Hotkey error", message)
        self._refresh_labels()

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
