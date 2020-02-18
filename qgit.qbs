import qbs
import "qbs/imports/QbsUtl/qbsutl.js" as QbsUtl

Project {
    name: "QGit"
    minimumQbsVersion: "1.12.0"
    qbsSearchPaths: ["qbs"]

    readonly property string minimumQtVersion: "4.8.6"
    readonly property bool conversionWarnEnabled: true

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
        ];

        if (qbs.buildVariant === "release")
            def.push("NDEBUG");

        return def;
    }

//    property var cxxFlags: [
//        //"-std=c++11",
//        "-ggdb3",
//        //"-Winline",
//        "-Wall",
//        "-Wextra",
//        "-Wno-unused-parameter",
//        "-Wno-variadic-macros",
//    ]
//    property string cxxLanguageVersion: "c++11"

    references: [
        "src/src.qbs",
    ]
}
