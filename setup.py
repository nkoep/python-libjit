#!/usr/bin/env python2

import sys
import os
import glob
from distutils.core import Extension, setup
from distutils.command.build import build
from distutils.command.build_ext import build_ext

CFLAGS_DEBUG = "-O0 -g3 -ggdb -DPYJIT_DEBUG".split()

class BuildExt(build_ext):
    def finalize_options(self):
        build_ext.finalize_options(self)
        if self.debug:
            for mod in self.distribution.ext_modules:
                mod.extra_compile_args.extend(CFLAGS_DEBUG)

class BuildAndInjectModule(build):
    def run(self):
        # Build the module first.
        build.run(self)

        # Add the build directory to the module search path.
        import imp
        _, pathname, _ = imp.find_module(
            "jit", [os.path.join(os.getcwd(), self.build_lib)])
        sys.path.append(os.path.dirname(pathname))

class Test(BuildAndInjectModule):
    description = "run the test suite"

    def run(self):
        # Chain up.
        BuildAndInjectModule.run(self)

        # Run the test suite.
        import unittest
        loader = unittest.TestLoader()
        suite = loader.discover("tests")
        unittest.TextTestRunner(verbosity=2).run(suite)

class Tutorials(BuildAndInjectModule):
    description = "run ported LibJIT tutorials"

    def run(self):
        # Chain up.
        BuildAndInjectModule.run(self)

        # Run tutorials.
        import importlib
        for file_ in os.listdir("tutorials"):
            if file_.endswith("py") and not file_.startswith("__"):
                mod = importlib.import_module(
                    "tutorials.%s" % os.path.splitext(file_)[0])
                sys.stdout.write("running tutorial '%s': " % file_)
                mod.run()

if __name__ == "__main__":
    NAME = "python-libjit"
    PACKAGE_NAME = "jit"
    AUTHOR = "Niklas Koep"
    CFLAGS = "-Wall -Werror -ansi -std=c89".split()

    ext_modules = [
        Extension("_jit", sources=glob.glob(os.path.join("src", "*.c")),
                  extra_compile_args=CFLAGS, libraries=["jit"])
    ]

    kwargs = {
        "cmdclass": {
            "build_ext": BuildExt,
            "test": Test,
            "tutorials": Tutorials
        },
        "name": NAME,
        "version": "0.1",
        "license": "Apache License, Version 2.0",
        "description": "Python bindings for LibJIT",
        "author": AUTHOR,
        "author_email": "%s@gmail.com" % AUTHOR.lower().replace(" ", "."),
        "url": "https://github.io/nkoep/projects/%s" % NAME,
        "ext_package": PACKAGE_NAME,
        "ext_modules": ext_modules,
        "package_dir": {PACKAGE_NAME: "src"},
        "packages": [PACKAGE_NAME]
    }

    setup(**kwargs)

