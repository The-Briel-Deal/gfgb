import pathlib

from setuptools import Extension, setup
from setuptools.command.sdist import sdist


class SDist(sdist):

  def make_release_tree(self, base_dir, files) -> None:
    self.mkpath(base_dir)
    self.copy_tree(
        infile="../../src/",
        outfile=str((pathlib.Path(base_dir) / "c_src/").absolute()),
    )
    return super().make_release_tree(base_dir, files)


setup(
    ext_modules=[
        Extension(
            name="gfgb_py",
            sources=["src/*.pyx"],
            depends=["src/gfgb.pxd"],
            include_dirs=["c_src/"],
            libraries=["gfgb"]
        ),
    ],
    cmdclass={
        "sdist": SDist,
    },
)
