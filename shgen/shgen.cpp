#include <cassert>
#include <cstdint>

#include <map>
#include <memory>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "DirectXTex.h"

#include "ibl/spherical_harmonics.h"
#include "json11/json11.hpp"
#include "fsutil.h"

#define VERSION "1.0.0"

#define ABORT(msg) { puts(msg); return 1; }
#define ARG_CASE(s) if (kv.first == s)
#define ARG_CASE2(s1, s2) if (kv.first == s1 || kv.first == s2)
#define CHECK_NUM_ARGS(p) if (kv.second.size() < p) continue;

namespace
{
    const char helpText[] =
        "\n"
        "shgen v" VERSION "\n"
        "---------------------------------------------------------------------\n"
        "  Usage: shgen <input specification> <options>\n"
        "\n"
        "INPUT SPECIFICATION\n"
        "  -i, --input <filename>\n"
            "\t入力ファイルパスを指定します。\n"
        "\n"
        "OPTIONS\n"
        "  -h, --help\n"
            "\tこれを表示します。\n"
        "  -o, --output <filename>\n"
            "\t出力ファイルパスを指定します。初期値は\"diffuse.json\"です。\n"
        "  -v, --verbose\n"
            "\t詳細な出力を行います。\n"
        "\n";

    struct Spec
    {
        std::string source;
        std::string output = "diffuse.json";
        std::string diffuse = "diffuse.dds";
        bool verboseSpecified = false;
    };

    std::wstring utf8ToUtf16(const std::string& u8str)
    {
        int u16strLen = ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, NULL, 0);
        if (u16strLen <= 0) return std::wstring();
        std::wstring u16str;
        u16str.resize(u16strLen-1);
        ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, &u16str[0], u16strLen);
        return u16str;
    }

    std::string utf16ToUtf8(const std::wstring& u16str)
    {
        int u8strLen = ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, NULL, 0, NULL, NULL);
        if (u8strLen <= 0) return std::string();
        std::string u8str;
        u8str.resize(u8strLen - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, &u8str[0], u8strLen, NULL, NULL);
        return u8str;
    }

    std::map<std::string, std::vector<std::string>> parseOptions(int argc, char* argv[])
    {
        std::map<std::string, std::vector<std::string>> options;
        int argPos = 1;
        bool suggestHelp = false;
        while (argPos < argc) {
            const char* arg = argv[argPos];
            if (arg[0] != '-') {
                printf("Unknown setting or insufficient parameters: %s\n", arg);
                suggestHelp = true;
            }
            else {
                int pos = argPos;
                while (pos+1 < argc) {
                    if (argv[pos+1][0] == '-') break;
                    ++pos;
                }
                int count = pos - argPos;
                if (count <= 0) {
                    options[arg].clear();
                    ++argPos;
                }
                else {
                    for (int i = argPos+1, end = i+count; i < end; ++i)
                        options[arg].push_back(argv[i]);
                    argPos += 1 + count;
                }
            }
        }
        if (suggestHelp) printf("Use --help for more information.\n");
        return options;
    }

    int parseArguments(Spec& spec, int argc, char* argv[])
    {
        auto options = parseOptions(argc, argv);
        bool inputSpecified = false;
        bool outputSpecified = false;
        for (auto& kv : options) {
            ARG_CASE2("-i", "--input") {
                CHECK_NUM_ARGS(1);
                inputSpecified = true;
                spec.source = kv.second[0];
                continue;
            }
            ARG_CASE2("-o", "--output") {
                CHECK_NUM_ARGS(1);
                outputSpecified = true;
                spec.output = kv.second[0];
                continue;
            }
            ARG_CASE2("-v", "--verbose") {
                spec.verboseSpecified = true;
                continue;
            }
            ARG_CASE2("-h", "--help") {
                ABORT(helpText);
            }
        }
        if (!inputSpecified) ABORT("No input source specified! Use --input <filename/folder>, or see --help");
        return 0;
    }

    std::unique_ptr<DirectX::ScratchImage> loadImageFromFile(const std::string& filename)
    {
        auto images = std::make_unique<DirectX::ScratchImage>();
        DirectX::TexMetadata meta;
        HRESULT hr = DirectX::LoadFromDDSFile(utf8ToUtf16(filename).c_str(), DirectX::DDS_FLAGS_NONE, &meta, *images);
        if (FAILED(hr)) {
            return nullptr;
        }
        return images;
    }

    ibl::Cubemap createCubemap(const DirectX::ScratchImage* images)
    {
        enum {
            DDS_CUBEMAP_FACE_PX,
            DDS_CUBEMAP_FACE_NX,
            DDS_CUBEMAP_FACE_PY,
            DDS_CUBEMAP_FACE_NY,
            DDS_CUBEMAP_FACE_PZ,
            DDS_CUBEMAP_FACE_NZ,
        };

        auto setFaceFromImage = [](ibl::Cubemap& cm, ibl::Cubemap::Face face, const DirectX::Image* image)
        {
            ibl::Image subImage(image->pixels, image->width, image->height);
            assert(subImage.getBytesPerRow() == image->rowPitch);
            cm.setImageForFace(face, subImage);
        };

        size_t dim = images->GetMetadata().width;

        ibl::Cubemap cm(dim);

        setFaceFromImage(cm, ibl::Cubemap::Face::NX, images->GetImage(0, DDS_CUBEMAP_FACE_NX, 0));
        setFaceFromImage(cm, ibl::Cubemap::Face::PX, images->GetImage(0, DDS_CUBEMAP_FACE_PX, 0));
        setFaceFromImage(cm, ibl::Cubemap::Face::NY, images->GetImage(0, DDS_CUBEMAP_FACE_NY, 0));
        setFaceFromImage(cm, ibl::Cubemap::Face::PY, images->GetImage(0, DDS_CUBEMAP_FACE_PY, 0));
        setFaceFromImage(cm, ibl::Cubemap::Face::NZ, images->GetImage(0, DDS_CUBEMAP_FACE_NZ, 0));
        setFaceFromImage(cm, ibl::Cubemap::Face::PZ, images->GetImage(0, DDS_CUBEMAP_FACE_PZ, 0));
        return cm;
    }

    bool saveSphericalHarmonics(const Spec& spec, const std::unique_ptr<ibl::math::double3[]>& sh)
    {
        json11::Json::array jsonSH;
        jsonSH.resize(9);

        for (size_t i = 0; i < jsonSH.size(); ++i) {
            jsonSH[i] = json11::Json::array{sh[i].x, sh[i].y, sh[i].z};
        }

        std::string str = json11::Json(jsonSH).dump();

        fs::FileHandle f = fs::openFile(spec.output, fs::FileMode::Create | fs::FileAccess::Write);
        if (f.isInvalid()) {
            return false;
        }

        fs::writeFile(f, str.c_str(), str.length());
        fs::closeFile(f);
        return true;
    }
}

int main(int argc, char* argv[])
{
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
        return 1;

    Spec spec;
    if (parseArguments(spec, argc, argv) != 0)
        return 1;

    auto images = loadImageFromFile(spec.source);
    if (!images)
        ABORT("DirectX::LoadFromXXXFile failed.");

    if (images->GetMetadata().format != DXGI_FORMAT_R32G32B32_FLOAT)
        ABORT("Given cubemap format must be DXGI_FORMAT_R32G32B32_FLOAT");

    ibl::Cubemap cm = createCubemap(images.get());

    auto sh = ibl::computeIrradianceSH3Bands(cm);

    saveSphericalHarmonics(spec, sh);

    if (spec.verboseSpecified) {
        ibl::renderPreScaledSH3Bands(cm, sh);
        if (FAILED(DirectX::SaveToDDSFile(images->GetImages(), images->GetImageCount(), images->GetMetadata(),
                                          DirectX::DDS_FLAGS_NONE, utf8ToUtf16(spec.diffuse).c_str())))
        {
            ABORT("DirectX::SaveToDDSFile failed.");
        }
    }

    CoUninitialize();
    return 0;
}