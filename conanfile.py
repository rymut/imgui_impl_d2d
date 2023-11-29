from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.files import load, copy
from conan.tools.scm import Git
import re
import os

class ImGuiImplD2D(ConanFile):
    name = "ImGuiImplD2D"
    description = "ImGUI Direct2D backend"
    license = "MIT"
    author = "Boguslaw Rymut (boguslaw@rymut.org)"
    topics = ("gui", "graphical", "bloat-free", "backend")
    generators = "CMakeDeps"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def set_version(self):
        git = Git(self, self.recipe_folder)
        try:
            if git.is_dirty():
                self.version = "cci_%s" % datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%S')
        except:
            pass
        try:
            tag = git.run("describe --tags").strip()
            if tag.startswith("v"):
                tag = tag[1:].strip()
            if re.match("^(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$", tag):
                self.version = tag
                return
        except:
            pass
        try:
            self.version = load(self, "version.semver").strip()
            return
        except:
            pass
        try:
            self.version = "rev_%s" % git.get_commit().strip()
            return
        except:
            pass
        return None

    def requirements(self):
        self.requires("imgui/1.89.9")

    def configure(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        backends = os.path.join(self.export_sources_folder, "backends")
        copy(self, "imgui_impl_sdlrenderer2.*", self.dependencies["imgui"].cpp_info.srcdirs[0], os.path.join(self.build_folder, "backends"))
        copy(self, "imgui_impl_sdl2.*", self.dependencies["imgui"].cpp_info.srcdirs[0], os.path.join(self.build_folder, "backends"))
        copy(self, "imgui_impl_win32.*", self.dependencies["imgui"].cpp_info.srcdirs[0], os.path.join(self.build_folder, "backends"))
        tc = CMakeToolchain(self)
        tc.generate()

