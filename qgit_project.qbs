import qbs
import "qbs/imports/QbsUtl/qbsutl.js" as QbsUtl

Project {
    name: "QGit (Project)"
    minimumQbsVersion: "1.18.0"
    qbsSearchPaths: ["qbs"]

    readonly property string minimumQtVersion: "5.12.6"
    readonly property bool conversionWarnEnabled: true
    readonly property bool standaloneBuild: false

    readonly property var projectVersion: projectProbe.projectVersion
    readonly property string projectGitRevision: projectProbe.projectGitRevision

    Probe {
        id: projectProbe
        property var projectVersion;
        property string projectGitRevision;

        readonly property string projectBuildDirectory:  buildDirectory
        readonly property string projectSourceDirectory: sourceDirectory

        configure: {
            projectVersion = QbsUtl.getVersions(projectSourceDirectory + "/VERSION");
            projectGitRevision = QbsUtl.gitRevision(projectSourceDirectory);
            //if (File.exists(projectBuildDirectory + "/package_build_info"))
            //    File.remove(projectBuildDirectory + "/package_build_info")
        }
    }

    property var cppDefines: {
        var def = [
            "VERSION_PROJECT=\"" + projectVersion[0] + "\"",
            "VERSION_PROJECT_MAJOR=" + projectVersion[1],
            "VERSION_PROJECT_MINOR=" + projectVersion[2],
            "VERSION_PROJECT_PATCH=" + projectVersion[3],
            "GIT_REVISION=\"" + projectGitRevision + "\"",
            "LOGGER_LESS_SNPRINTF",
        ];

        if (qbs.buildVariant === "release")
            def.push("NDEBUG");

        if (qbs.targetOS.contains("windows")
            && qbs.toolchain && qbs.toolchain.contains("mingw"))
        {
            def.push("CONFIG_DIR=\"ProgramData/qgit/config\"");
        }
        else
            def.push("CONFIG_DIR=\"~/.config/qgit\"");

        return def;
    }

    property var cxxFlags: [
        "-ggdb3",
        //"-Winline",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-variadic-macros",
    ]
    property string cxxLanguageVersion: "c++17"

    references: [
        "src/qgit.qbs",
        "src/shared/shared.qbs",
        "src/spellcheck/sonnet/gentrigrams.qbs",
        "src/spellcheck/sonnet/parsetrigrams.qbs",
        "src/yaml/yaml.qbs",
        "setup/package_build.qbs",
    ]
}
