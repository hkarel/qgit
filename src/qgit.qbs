import qbs
import qbs.FileInfo
import QbsUtl

Product {
    name: "QGit"
    targetName: "qgit"

    type: "application"
    destinationDirectory: "./bin"

    Depends { name: "cpp" }
    Depends { name: "cppstdlib" }
    Depends { name: "SharedLib" }
    Depends { name: "Yaml" }
    Depends { name: "Qt"; submodules: ["core", "widgets"] }

    Depends { productTypes: ["trigrams-generator"] }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: {
        var cxx = project.cxxFlags.concat([
            "-Wno-non-virtual-dtor",
            "-Wno-long-long",
            "-pedantic",
        ]);

        //if (qbs.buildVariant !== "debug")
        //    cxx.push("-s");

        if (project.conversionWarnEnabled)
            cxx.push("-Wconversion");

        return cxx;
    }
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

//    cpp.driverLinkerFlags: [
//        // Using RPATH instead of RUNPATH
//        "-Wl,--disable-new-dtags",
//    ]

    cpp.includePaths: [
        "./",
    ]

    // Suppression a Qt warnings
    cpp.systemIncludePaths: Qt.core.cpp.includePaths

    cpp.rpaths: QbsUtl.concatPaths(
        cppstdlib.path
       ,"$ORIGIN/../lib"
    )

    cpp.dynamicLibraries: {
        var libs = ["pthread"];
        if (!project.standaloneBuild)
            libs.push("hunspell");
        return libs;
    }

    cpp.staticLibraries: {
        var libs = [];
        if (project.standaloneBuild)
            libs.push("/usr/lib/x86_64-linux-gnu/libhunspell.a");
        return libs;
    }

    Group {
        name: "resources"
        files: {
            var files = [
                "icons.qrc"
            ];
            if (qbs.targetOS.contains('windows')
                && qbs.toolchain && qbs.toolchain.contains('msvc'))
                files.push("app_icon.rc");

            if (qbs.targetOS.contains('macos') &&
                qbs.toolchain && qbs.toolchain.contains('gcc'))
                files.push("resources/app_icon.rc");

            return files;
        }
    }

    Group {
        name: "windows"
        files: [
            "commitimpl.cpp",
            "commitimpl.h",
            "commit.ui",
            "consoleimpl.cpp",
            "consoleimpl.h",
            "console.ui",
            "customaction.ui",
            "customactionimpl.cpp",
            "customactionimpl.h",
            "fileview.cpp",
            "fileview.h",
            "fileview.ui",
            "help.h",
            "help.ui",
            "mainimpl.cpp",
            "mainimpl.h",
            "mainwindow.ui",
            "patchview.cpp",
            "patchview.h",
            "patchview.ui",
            "rangeselectimpl.cpp",
            "rangeselectimpl.h",
            "rangeselect.ui",
            "revsview.cpp",
            "revsview.h",
            "revsview.ui",
            "settingsimpl.cpp",
            "settingsimpl.h",
            "settings.ui",
            "tabwidget.h",
        ]
    }

    Group {
        name: "others"
        files: [
            "../exception_manager.txt",
            "../README.md",
            "../README_WIN.txt",
            "../qgit_inno_setup.iss",
            "helpgen.sh",
            "todo.txt",
        ]
    }

    Group {
        name: "spellcheck"
        files: [
            "spellcheck/highlighter.cpp",
            "spellcheck/highlighter.h",
            "spellcheck/spellcheck.cpp",
            "spellcheck/spellcheck.h",
        ]
    }

    Group {
        fileTags: "trigrams"
        files: FileInfo.joinPaths(product.sourceDirectory, "spellcheck/sonnet/trigrams/*")
    }

    Group {
        fileTagsFilter: ["trigrams-map"]
        fileTags: ["qt.core.resource_data"]
    }
    Qt.core.resourceFileBaseName: "trigrams"
    Qt.core.resourcePrefix: "trigrams"

    Rule {
        id: idtrigrams
        inputs: ["trigrams"]
        explicitlyDependsOnFromDependencies: ["trigrams-generator"]

        Artifact {
            fileTags: ["trigrams-map"]
            filePath: FileInfo.joinPaths(project.buildDirectory, "trigrams", input.baseName + ".tmap")
        }
        prepare: {
            var runUtl = explicitlyDependsOn["trigrams-generator"][0].filePath
            var outputFile = FileInfo.joinPaths(project.buildDirectory, "trigrams", input.baseName + ".tmap");

//            console.info("=== runUtl ===");
//            console.info(input);
//            console.info(inputs);
//            console.info(runUtl);
//            console.info(input.filePath);
//            console.info(outputFile);

            var cmd = new Command(runUtl, [input.filePath, outputFile]);
            cmd.description = "sonnet parse trigrams";
            cmd.highlight = "filegen";
            return cmd;
        }
    }

    files: [
        "annotate.cpp",
        "annotate.h",
        "cache.cpp",
        "cache.h",
        "common.cpp",
        "common.h",
        "common_types.h",
        "config.h",
        "dataloader.cpp",
        "dataloader.h",
        "domain.cpp",
        "domain.h",
        "exceptionmanager.cpp",
        "exceptionmanager.h",
        "filecontent.cpp",
        "filecontent.h",
        "filelist.cpp",
        "filelist.h",
        "git.cpp",
        "git.h",
        "inputdialog.cpp",
        "inputdialog.h",
        "lanes.cpp",
        "lanes.h",
        "listview.cpp",
        "listview.h",
        "myprocess.cpp",
        "myprocess.h",
        "patchcontent.cpp",
        "patchcontent.h",
        "revdesc.cpp",
        "revdesc.h",
        "smartbrowse.cpp",
        "smartbrowse.h",
        "treeview.cpp",
        "treeview.h",
        "FileHistory.cc",
        "FileHistory.h",
        "qgit.cpp",
    ]

//    property var test: {
//        console.info("=== Qt.core.version ===");
//        console.info(Qt.core.version);
//        console.info("=== VERSION_PROJECT ===");
//        console.info(project.projectVersion[0]);
//    }
}
