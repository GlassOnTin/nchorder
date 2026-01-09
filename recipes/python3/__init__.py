"""Custom python3 recipe that disables _uuid module for Android.

Android NDK doesn't provide libuuid (uuid/uuid.h), so the _uuid extension
module fails to link. This recipe overrides get_recipe_env() to set
py_cv_module__uuid=n/a, telling configure to skip the _uuid module.

Python's uuid module falls back to pure Python implementation automatically.
"""
from pythonforandroid.recipes.python3 import Python3Recipe


class Python3NoUuidRecipe(Python3Recipe):
    """Python3 recipe with _uuid module disabled."""

    def get_recipe_env(self, arch=None, with_flags_in_cc=True):
        env = super().get_recipe_env(arch, with_flags_in_cc)
        # Disable modules requiring unavailable libraries on Android NDK
        env['py_cv_module__uuid'] = 'n/a'
        env['py_cv_module__tkinter'] = 'n/a'
        return env


recipe = Python3NoUuidRecipe()
