import qbs
import qbs.FileInfo

Product {
    name: "ParseTrigrams"
    targetName: "parsetrigrams"
    condition: true

    //type: "application"
    type: ["application", "trigrams-generator"]
    destinationDirectory: "./bin"

    Depends { name: "cpp" }
    Depends { name: "Qt"; submodules: ["core"] }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

    files: [
        "parsetrigrams.cpp"
    ]

    Group {
        fileTagsFilter: "application"
        fileTags: "trigrams-generator"
    }
}
