"""
Custom python3 recipe that disables the _uuid module.

The _uuid module requires libuuid which is not available on Android NDK.
Python's uuid module falls back to a pure Python implementation automatically.
"""
import os
from os.path import dirname, join
from pythonforandroid.recipes.python3 import Python3Recipe
import pythonforandroid.recipes.python3


class Python3NoUuidRecipe(Python3Recipe):
    """Python3 recipe with _uuid module disabled."""

    # Point patches to the original recipe's patches directory
    # This is necessary because local recipes override the patches path
    @property
    def patches(self):
        base_recipe_dir = dirname(pythonforandroid.recipes.python3.__file__)
        # Return the same patches as the base recipe but with correct paths
        return [
            join(base_recipe_dir, 'patches', 'pyconfig_detection.patch'),
        ]

    def get_recipe_env(self, arch=None, with_flags_in_cc=True):
        env = super().get_recipe_env(arch, with_flags_in_cc)
        # Disable _uuid module - requires libuuid which doesn't exist on Android
        env['py_cv_module__uuid'] = 'n/a'
        return env

    def prebuild_arch(self, arch):
        # Set environment variable before any build steps
        os.environ['py_cv_module__uuid'] = 'n/a'
        super().prebuild_arch(arch)

    def build_arch(self, arch):
        # Ensure environment variable is set during build
        os.environ['py_cv_module__uuid'] = 'n/a'
        super().build_arch(arch)


recipe = Python3NoUuidRecipe()
