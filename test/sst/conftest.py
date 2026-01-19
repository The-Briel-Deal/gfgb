import importlib.util
import sys
import pytest


def pytest_addoption(parser):
  parser.addoption(
      "--gfgb-py-ext",
      action="store",
      help="path to gfgb_py extension module shared object",
  )


def import_from_path(module_name, file_path):
  spec = importlib.util.spec_from_file_location(module_name, file_path)
  assert spec
  module = importlib.util.module_from_spec(spec)
  sys.modules[module_name] = module
  assert spec.loader
  spec.loader.exec_module(module)
  return module


@pytest.fixture
def gfgb_py_mod(request):
  mod_path = request.config.getoption("--gfgb-py-ext")
  return import_from_path(
      "gfgb_py",
      mod_path,
  )
