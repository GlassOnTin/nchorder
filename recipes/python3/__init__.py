"""
Custom python3 recipe that disables the _uuid module.

The _uuid module requires libuuid which is not available on Android NDK.
Python's uuid module falls back to a pure Python implementation automatically.
"""
from pythonforandroid.recipes.python3 import Python3Recipe


class Python3NoUuidRecipe(Python3Recipe):
    """Python3 recipe with _uuid module disabled."""

    def get_recipe_env(self, arch=None, with_flags_in_cc=True):
        env = super().get_recipe_env(arch, with_flags_in_cc)
        # Disable _uuid module - requires libuuid which doesn't exist on Android
        env['py_cv_module__uuid'] = 'n/a'
        return env

    def build_arch(self, arch):
        # Set environment variable before build
        import os
        os.environ['py_cv_module__uuid'] = 'n/a'
        super().build_arch(arch)


recipe = Python3NoUuidRecipe()
