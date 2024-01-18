from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.microsoft import check_min_vs, is_msvc_static_runtime, is_msvc
from conan.tools.files import apply_conandata_patches, export_conandata_patches, get, copy, rm, rmdir, replace_in_file
from conan.tools.build import check_min_cppstd
from conan.tools.scm import Version
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv
import os


required_conan_version = ">=1.53.0"

class open62541pp(ConanFile):
    name = "open62541pp"
    description = "open62541++ is a C++ wrapper built on top of the amazing open62541 OPC UA (OPC Unified Architecture) library"
    license = "MPL-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/open62541pp/open62541pp"
    topics = ("cpp", "cpp17", "opcua", "open62541")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "internal_open62541": [True, False],
        "build_documentation": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "internal_open62541": False,
        "build_documentation": False,
    }

    @property
    def _min_cppstd(self):
        return 17

    @property
    def _compilers_minimum_version(self):
        return {
            "gcc": "7",
            "clang": "7",
            "apple-clang": "10",
        }

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self_version = Version(self.version)
        open62541_version = "1.2.6"

        if self_version >= "0.2.0":
            open62541_version = "1.3.9"

        self.requires(f"open62541/{open62541_version}")

    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        check_min_vs(self, 191)
        if not is_msvc(self):
            minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler), False)
            if minimum_version and Version(self.settings.compiler.version) < minimum_version:
                raise ConanInvalidConfiguration(
                    f"{self.ref} requires C++{self._min_cppstd}, which your compiler does not support."
                )
        if is_msvc(self) and self.options.shared:
            raise ConanInvalidConfiguration(f"{self.ref} can not be built as shared on Visual Studio and msvc.")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["UAPP_INTERNAL_OPEN62541"] = self.options.internal_open62541
        tc.variables["UAPP_BUILD_DOCUMENTATION"] = self.options.build_documentation
        if is_msvc(self):
            tc.variables["USE_MSVC_RUNTIME_LIBRARY_DLL"] = not is_msvc_static_runtime(self)
        tc.cache_variables["CMAKE_POLICY_DEFAULT_CMP0077"] = "NEW"
        tc.generate()
        cmake_deps = CMakeDeps(self)
        cmake_deps.generate()

    def _patch_sources(self):
        apply_conandata_patches(self)

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        self_version = Version(self.version)
        if self_version >= "0.3.0":
            cmake = CMake(self)
            cmake.install()
        else:
            copy(self, pattern="*.h", dst=os.path.join(self.package_folder, "include"), src=os.path.join(self.source_folder, "include"))    
            copy(self, pattern="*.a", src=os.path.join(self.build_folder, "bin"), dst=os.path.join(self.package_folder, "lib"), keep_path=False)

        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)

        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "lib"))
        rm(self, "*.pdb", os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.libs = ["open62541pp"]
        self.cpp_info.includedirs = ["include"]
        
        self.cpp_info.components["open62541pp"].libs = ["open62541pp"]

        self.cpp_info.set_property("cmake_file_name", "open62541pp")
        self.cpp_info.set_property("cmake_target_name", "open62541pp::open62541pp")

        self.cpp_info.components["open62541pp"].requires.append("open62541::open62541")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("m")
            self.cpp_info.system_libs.append("pthread")
            self.cpp_info.system_libs.append("dl")

        # TODO: to remove in conan v2 once cmake_find_package_* generators removed
        self.cpp_info.filenames["cmake_find_package"] = "open62541pp"
        self.cpp_info.filenames["cmake_find_package_multi"] = "open62541pp"
        self.cpp_info.names["cmake_find_package"] = "open62541pp"
        self.cpp_info.names["cmake_find_package_multi"] = "open62541pp"
        self.cpp_info.components["open62541pp"].set_property("cmake_target_name", "open62541pp::open62541pp")
