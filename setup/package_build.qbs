import qbs
import qbs.TextFile

Product {
    name: "PackageBuild"
    condition: project.standaloneBuild

    Depends { name: "cpp" }
    Depends { name: "cppstdlib" }
    Depends { name: "Qt"; submodules: ["core", "network", "sql", "dbus", "gui", "widgets" ] }

    Probe {
        id: productProbe
        property string projectBuildDirectory: project.buildDirectory
        property string cppstdlibPath: cppstdlib.path
        property var qt: Qt

        configure: {
            file = new TextFile(projectBuildDirectory + "/package_build_info", TextFile.WriteOnly);
            try {
                if (!cppstdlibPath.startsWith("/usr/lib", 0)) {
                    file.writeLine(cppstdlibPath + "/" + "libstdc++.so*");
                    file.writeLine(cppstdlibPath + "/" + "libgcc_s.so*");
                }

                var libFiles = []
                libFiles.push(qt["core"].libFilePathRelease);
                libFiles.push(qt["network"].libFilePathRelease);
                libFiles.push(qt["sql"].libFilePathRelease);
                libFiles.push(qt["dbus"].libFilePathRelease);
                libFiles.push(qt["gui"].libFilePathRelease);
                libFiles.push(qt["widgets"].libFilePathRelease);
                libFiles.push(qt["core"].libPath + "/libicui18n.so.56");
                libFiles.push(qt["core"].libPath + "/libicuuc.so.56");
                libFiles.push(qt["core"].libPath + "/libicudata.so.56");
                libFiles.push(qt["core"].libPath + "/libQt5XcbQpa.so.5");

                for (var i in libFiles)
                    file.writeLine(libFiles[i].replace(/\.so\..*$/, ".so*"));
            }
            finally {
                file.close();
            }

            file = new TextFile(projectBuildDirectory + "/package_build_info2", TextFile.WriteOnly);
            try {
                file.writeLine(qt.core.pluginPath);
            }
            finally {
                file.close();
            }
        }
    }
}
