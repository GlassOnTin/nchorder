#!/bin/bash
cd "$(dirname "$0")"
.venv/bin/python -m nchorder_tools.gui.app "$@"
