"""Config persistence and the Settings dialog for LUU."""

from __future__ import annotations

import copy
import json
import logging
import subprocess
from pathlib import Path
from typing import Any

from PyQt6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QHBoxLayout,
    QKeySequenceEdit,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
)
from PyQt6.QtGui import QKeySequence
from PyQt6.QtCore import Qt

# build/src/render/luu_capture_preview relative to the repo root (this file
# lives at src/gui/settings_ui.py). Duplicated from main.py's CAPTURE_BINARY
# rather than imported, to avoid a circular import between the two modules.
CAPTURE_BINARY = Path(__file__).resolve().parents[2] / "build" / "src" / "render" / "luu_capture_preview"

logger = logging.getLogger("luu.settings")

CONFIG_DIR = Path.home() / ".config" / "luu"
CONFIG_PATH = CONFIG_DIR / "settings.json"

DEFAULTS: dict[str, Any] = {
    "hotkey": "<alt>+u",
    "upscale_mode": "fsr",
    "target_resolution": [1920, 1080],
    "frame_gen_enabled": True,
    "framegen_method": "lsfg",
    "quality": "high",
    "capture_backend": "auto",
    "capture_output": "",
    "capture_target": "output",
    "capture_window_id": "",
    "capture_window_title": "",
    "capture_window_app_id": "",
}

UPSCALE_MODES = ["fsr", "lanczos", "bilinear", "nearest"]
FRAMEGEN_METHODS = ["lsfg", "interpolation"]
QUALITY_LEVELS = ["low", "medium", "high", "ultra"]
CAPTURE_TARGETS = ["output", "window"]


class ConfigManager:
    """Loads/saves ~/.config/luu/settings.json, merged onto DEFAULTS.

    A corrupt file is backed up (settings.json.bak) instead of being
    silently overwritten, so a bad manual edit never loses the user's data.
    """

    def __init__(self, path: Path = CONFIG_PATH):
        self.path = path
        self.data: dict[str, Any] = copy.deepcopy(DEFAULTS)

    def load(self) -> None:
        if not self.path.exists():
            logger.info("No config at %s, writing defaults", self.path)
            self.save()
            return

        try:
            with self.path.open("r", encoding="utf-8") as f:
                loaded = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            logger.error("Failed to read %s: %s", self.path, e)
            backup = self.path.with_suffix(self.path.suffix + ".bak")
            try:
                backup.write_bytes(self.path.read_bytes())
                logger.warning("Backed up corrupt config to %s", backup)
            except OSError:
                pass
            self.data = copy.deepcopy(DEFAULTS)
            return

        merged = copy.deepcopy(DEFAULTS)
        merged.update(loaded)
        self.data = merged

    def save(self) -> bool:
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open("w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=4)
            return True
        except OSError as e:
            logger.error("Failed to write %s: %s", self.path, e)
            return False

    def get(self, key: str, default: Any = None) -> Any:
        return self.data.get(key, default)

    def set(self, key: str, value: Any) -> None:
        self.data[key] = value


class WindowPickerDialog(QDialog):
    """Lists currently open windows (via `luu_capture_preview --list-windows`)
    and lets the user pick one. A picked window's identifier is only valid
    while that window stays open - closing and reopening it needs a re-pick,
    see WaylandToplevelCapture's class comment in the C++ source.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Choose a Window")
        self.setMinimumSize(420, 320)
        self.picked: dict[str, str] | None = None

        self.list_widget = QListWidget()
        self.status_label = QLabel()

        layout = QVBoxLayout()
        layout.addWidget(self.status_label)
        layout.addWidget(self.list_widget)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._on_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.setLayout(layout)

        self._load_windows()

    def _load_windows(self) -> None:
        if not CAPTURE_BINARY.exists():
            self.status_label.setText(
                f"luu_capture_preview not found at {CAPTURE_BINARY}.\nBuild it first with "
                "./scripts/build.sh."
            )
            return

        try:
            result = subprocess.run(
                [str(CAPTURE_BINARY), "--list-windows"],
                capture_output=True,
                text=True,
                timeout=5,
                check=True,
            )
            windows = json.loads(result.stdout)
        except (subprocess.SubprocessError, json.JSONDecodeError, OSError) as e:
            logger.error("Failed to list windows: %s", e)
            self.status_label.setText(f"Could not list windows: {e}")
            return

        if not windows:
            self.status_label.setText("No open windows found.")
            return

        self.status_label.setText(f"{len(windows)} window(s) open - pick one:")
        for win in windows:
            label = f"{win.get('title', '(no title)')} — {win.get('app_id', '')}"
            item = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, win)
            self.list_widget.addItem(item)

    def _on_accept(self) -> None:
        item = self.list_widget.currentItem()
        if item is None:
            QMessageBox.warning(self, "No selection", "Pick a window from the list first.")
            return
        self.picked = item.data(Qt.ItemDataRole.UserRole)
        self.accept()


class SettingsDialog(QDialog):
    """Modal form for editing upscale/frame-gen/hotkey settings."""

    def __init__(self, config: ConfigManager, parent=None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("LUU Settings")
        self.setMinimumWidth(360)

        self.hotkey_edit = QKeySequenceEdit(QKeySequence(self._hotkey_to_qt(config.get("hotkey"))))

        self.upscale_combo = QComboBox()
        self.upscale_combo.addItems(UPSCALE_MODES)
        self.upscale_combo.setCurrentText(config.get("upscale_mode", "fsr"))

        self.framegen_check = QCheckBox("Enable Frame Generation")
        self.framegen_check.setChecked(bool(config.get("frame_gen_enabled", True)))

        self.framegen_combo = QComboBox()
        self.framegen_combo.addItems(FRAMEGEN_METHODS)
        self.framegen_combo.setCurrentText(config.get("framegen_method", "lsfg"))

        self.quality_combo = QComboBox()
        self.quality_combo.addItems(QUALITY_LEVELS)
        self.quality_combo.setCurrentText(config.get("quality", "high"))

        width, height = config.get("target_resolution", [1920, 1080])
        self.width_spin = QSpinBox()
        self.width_spin.setRange(320, 7680)
        self.width_spin.setValue(width)
        self.height_spin = QSpinBox()
        self.height_spin.setRange(240, 4320)
        self.height_spin.setValue(height)

        self.capture_output_edit = QLineEdit(config.get("capture_output", ""))
        self.capture_output_edit.setPlaceholderText("e.g. DP-2 (see `hyprctl monitors`) - empty = auto")

        self.capture_target_combo = QComboBox()
        self.capture_target_combo.addItem("Output (whole monitor)", "output")
        self.capture_target_combo.addItem("Window (specific window)", "window")
        target_index = self.capture_target_combo.findData(config.get("capture_target", "output"))
        self.capture_target_combo.setCurrentIndex(max(target_index, 0))

        self._picked_window_id = config.get("capture_window_id", "")
        self._picked_window_title = config.get("capture_window_title", "")
        self._picked_window_app_id = config.get("capture_window_app_id", "")

        self.window_label = QLabel()
        self.window_button = QPushButton("Choose window...")
        self.window_button.clicked.connect(self._on_choose_window)
        self._refresh_window_label()

        window_row = QHBoxLayout()
        window_row.addWidget(self.window_label, stretch=1)
        window_row.addWidget(self.window_button)

        form = QFormLayout()
        form.addRow("Hotkey", self.hotkey_edit)
        form.addRow("Upscale mode", self.upscale_combo)
        form.addRow(self.framegen_check)
        form.addRow("Frame gen method", self.framegen_combo)
        form.addRow("Quality", self.quality_combo)
        form.addRow("Target width", self.width_spin)
        form.addRow("Target height", self.height_spin)
        form.addRow("Capture target", self.capture_target_combo)
        form.addRow("Capture output", self.capture_output_edit)
        form.addRow("Capture window", window_row)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._on_save)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

        self.setLayout(form)

    def _refresh_window_label(self) -> None:
        if self._picked_window_id:
            self.window_label.setText(f"{self._picked_window_title} — {self._picked_window_app_id}")
        else:
            self.window_label.setText("No window picked")

    def _on_choose_window(self) -> None:
        picker = WindowPickerDialog(parent=self)
        if picker.exec() and picker.picked:
            self._picked_window_id = picker.picked.get("identifier", "")
            self._picked_window_title = picker.picked.get("title", "")
            self._picked_window_app_id = picker.picked.get("app_id", "")
            self._refresh_window_label()

    @staticmethod
    def _hotkey_to_qt(hotkey: str) -> str:
        # pynput style "<alt>+u" -> Qt style "Alt+U"
        parts = [p.strip("<>") for p in hotkey.split("+")]
        return "+".join(p.capitalize() if len(p) > 1 else p.upper() for p in parts)

    @staticmethod
    def _qt_to_hotkey(qt_seq: QKeySequence) -> str:
        text = qt_seq.toString()
        if not text:
            return DEFAULTS["hotkey"]
        parts = [p.strip() for p in text.split("+")]
        out = []
        for p in parts[:-1]:
            out.append(f"<{p.lower()}>")
        out.append(parts[-1].lower())
        return "+".join(out)

    def _on_save(self):
        capture_target = self.capture_target_combo.currentData()
        if capture_target == "window" and not self._picked_window_id:
            QMessageBox.warning(
                self, "No window picked", "Choose a window first, or switch capture target back "
                "to Output."
            )
            return

        self.config.set("hotkey", self._qt_to_hotkey(self.hotkey_edit.keySequence()))
        self.config.set("upscale_mode", self.upscale_combo.currentText())
        self.config.set("frame_gen_enabled", self.framegen_check.isChecked())
        self.config.set("framegen_method", self.framegen_combo.currentText())
        self.config.set("quality", self.quality_combo.currentText())
        self.config.set(
            "target_resolution", [self.width_spin.value(), self.height_spin.value()]
        )
        self.config.set("capture_output", self.capture_output_edit.text().strip())
        self.config.set("capture_target", capture_target)
        self.config.set("capture_window_id", self._picked_window_id)
        self.config.set("capture_window_title", self._picked_window_title)
        self.config.set("capture_window_app_id", self._picked_window_app_id)

        if self.config.save():
            self.accept()
        else:
            QMessageBox.critical(
                self,
                "Save failed",
                f"Could not write settings to {self.config.path}.\n"
                "Check that the directory is writable.",
            )
