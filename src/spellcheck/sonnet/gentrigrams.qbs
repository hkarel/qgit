import qbs
import qbs.FileInfo

Product {
    name: "GenTrigrams"
    targetName: "gentrigrams"
    condition: true

    //type: "application"
    type: ["application"]
    destinationDirectory: "./bin"

    Depends { name: "cpp" }
    Depends { name: "SharedLib" }
    Depends { name: "Qt"; submodules: ["core"] }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

    files: [
        "gentrigrams.cpp"
    ]
}
