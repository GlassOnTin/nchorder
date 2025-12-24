"""Pytest fixtures for Northern Chorder tests."""

import pytest
from pathlib import Path


@pytest.fixture
def project_root() -> Path:
    """Return the project root directory."""
    return Path(__file__).parent.parent


@pytest.fixture
def configs_dir(project_root) -> Path:
    """Return the configs directory."""
    return project_root / "configs"


@pytest.fixture
def sample_v7_config(configs_dir) -> Path:
    """Return path to MirrorWalk config (v7 format)."""
    return configs_dir / "mirrorwalk.cfg"


@pytest.fixture
def sample_csv(configs_dir) -> Path:
    """Return path to MirrorWalk CSV source."""
    return configs_dir / "mirrorwalk_source.csv"
