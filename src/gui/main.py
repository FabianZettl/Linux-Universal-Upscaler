#!/usr/bin/env python3
"""Linux Universal Upscaler & Frame Gen - GUI entry point (Phase 1).

Minimal main window: shows status, lets the user toggle upscaling on/off
via a global hotkey, and opens the settings dialog. The actual
capture/shader pipeline is added in Phase 2 - toggling here only flips
state and logs for now.
"""

from __future__ import annotations

import logging
import sys

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

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("luu.main")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Linux Universal Upscaler & Frame Gen")
        self.setMinimumSize(360, 220)

        self.config = ConfigManager()
        self.config.load()

        self.enabled = False

        self.status_label = QLabel()
        self.hotkey_label = QLabel()
        self.toggle_button = QPushButton("Enable")
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
        self.status_label.setText(f"Status: {'ENABLED' if self.enabled else 'disabled'}")
        self.toggle_button.setText("Disable" if self.enabled else "Enable")
        self.hotkey_label.setText(f"Hotkey: {self.hotkey_listener.hotkey}")

    def _toggle_enabled(self) -> None:
        self.enabled = not self.enabled
        logger.info("Upscaling %s", "enabled" if self.enabled else "disabled")
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
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
