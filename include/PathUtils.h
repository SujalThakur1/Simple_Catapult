// PathUtils.h
#pragma once
#include <string>
#include <fstream>

inline std::string FindImagePath(const std::string& relativePath)
{
    // relativePath example: "Terrain/Tree/Tree1.obj" or "Skybox/my.hdr"

    const std::string candidates[] = {
        "../images/"  + relativePath,
        "../../images/" + relativePath,
        "images/"     + relativePath
    };

    for (const auto& path : candidates)
    {
        std::ifstream file(path);
        if (file.good())
        {
            return path;
        }
    }

    // Not found → return the most common one so Assimp still shows normal error
    return "../images/" + relativePath;
}