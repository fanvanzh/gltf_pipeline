#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <stdio.h>

// convert string to lower format
std::string toLower(std::string str)
{
    for (auto & c : str)
        c = std::tolower(c);
    return str;
}

// read all into buffer from input file
char* readFileBuffer(std::string input, long& fsize)
{
    FILE* f = fopen(input.c_str(), "rb");
    if (!f)
    {
        printf("can`t open file: %s\n", input.c_str());
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char * buffer = new char[fsize + 1];
    if (fsize != fread(buffer, 1, fsize, f))
    {
        printf("read file %s error\n", input.c_str());
        fclose(f);
        delete []buffer;
        return NULL;
    }
    buffer[fsize] = 0;
    fclose(f);
    return buffer;
}

// convert glb to gltf json + image + bin
int glbToGltf(std::string input, std::string output)
{
    tinygltf::FsCallbacks fs = {
        &tinygltf::FileExists, &tinygltf::ExpandFilePath,
        &tinygltf::ReadWholeFile, &tinygltf::WriteWholeFile,
        nullptr
    };

    tinygltf::Model    model;
    tinygltf::TinyGLTF gltf;
    std::string error, warning;
    gltf.SetImageWriter(tinygltf::WriteImageData, &fs);
    if (!gltf.LoadBinaryFromFile(&model, &error, &warning, input))
    {
        printf("load glb failed: %s \n", input.c_str());
        printf("error: %s\n", error.c_str());
        return 1;
    }

    std::string filename = output.substr(output.find_last_of('/') + 1);
    int img_idx = 0;
    for (auto & image: model.images)
    {
        if (image.name.empty())
            image.name = filename + "._" + std::to_string(img_idx++);

        // only support jpg/png now
        std::string format = ".jpg";
        if (!image.mimeType.empty())
        {
            if (toLower(image.mimeType).find("png") != std::string::npos)
                format = ".png";
        }
        if (image.uri.empty())
            image.uri  = "./" + image.name + format;
    }

    if (!gltf.WriteGltfSceneToFile(&model, output, false, false, true, false))
    {
        printf("write gltf failed: %s \n", output.c_str());
        printf("error: %s\n", error.c_str());
        return 1;
    }
    return 0;
}

// convert gltf to glb file
int gltfToGlb(std::string input, std::string output)
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF gltf;
    std::string error, warning;

    if (!gltf.LoadASCIIFromFile(&model, &error, &warning, input))
    {
        printf("load gltf failed: %s\n", error.c_str());
        return 1;
    }
    if (!gltf.WriteGltfSceneToFile(&model, output, true, true, false, true))
    {
        printf("write glb failed: %s\n", output.c_str());
        return 1;
    }
    return 0;
}

// convert b3dm to glb
int b3dmToGlb(std::string input, std::string output)
{
    long fsize = 0;
    char * buffer = readFileBuffer(input, fsize);
    if (!buffer)
        return 1;

    FILE* f1 = fopen(output.c_str(), "wb");
    if (!f1)
    {
        printf("can`t open file: %s\n", output.c_str());
        return 1;
    }
    int glb_offset = 28 /* magic + version + byteLen */
                    + *(int*)(buffer + 12) /* feature json len */
                    + *(int*)(buffer + 16) /* feature binary len */
                    + *(int*)(buffer + 20) /* batch json len */
                    + *(int*)(buffer + 24); /* batch binary len */
    int glb_len = fsize - glb_offset;
    if (glb_len != fwrite(buffer + glb_offset, 1, glb_len, f1))
    {
        printf("write file %s error\n", input.c_str());
        return 1;
    }
    fclose(f1);
    delete buffer;
    return 0;
}

// convert glb to b3dm
int glbToB3dm(std::string input, std::string output)
{
    long fsize = 0;
    char * buffer = readFileBuffer(input, fsize);
    if (!buffer)
        return 1;

    int b3dmLen = fsize + 28;
    char * b3dmBuffer = new char[b3dmLen];
    memset(b3dmBuffer, 0, 28);
    memcpy(b3dmBuffer, "b3dm", 4);
    *(int*)(b3dmBuffer + 4) = 1;
    *(int*)(b3dmBuffer + 8) = b3dmLen;
    memcpy( b3dmBuffer + 28, buffer, fsize);
    delete buffer;

    FILE* f1 = fopen(output.c_str(), "wb");
    if (!f1)
    {
        printf("can`t open file: %s\n", output.c_str());
        return 1;
    }
    if (b3dmLen != fwrite(b3dmBuffer, 1, b3dmLen, f1))
    {
        printf("write file %s error\n", input.c_str());
        return 1;
    }
    fclose(f1);
    delete []b3dmBuffer;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("usage:\ngltf_pipeline a.glb/gltf/b3dm b.gltf/b3dm/glb.\n");
        return 1;
    }

    std::string input = argv[1];
    std::string output = argv[2];
    std::string ext_in = toLower(input.substr(input.find_last_of('.') + 1));
    std::string ext_out = toLower(output.substr(output.find_last_of('.') + 1));
    if (ext_in == ext_out)
    {
        printf("input and output are the same.\n");
        return 1;
    }

    if (ext_in == "glb")
    {
        if (ext_out == "gltf")
            return glbToGltf(input, output);
        else if (ext_out == "b3dm")
            return glbToB3dm(input, output);
    }
    else if (ext_in == "b3dm")
    {
        if (ext_out == "glb")
            return b3dmToGlb(input, output);
        else if (ext_out == "gltf")
        {
            std::string glb_path = output + ".glb";
            if (0 == b3dmToGlb(input, glb_path))
            {
                int result = glbToGltf(glb_path, output);
                // remove the tmp glb file
                remove(glb_path.c_str());
                return result;
            }
            return 1;
        }
    }
    else if (ext_in == "gltf")
    {
        if (ext_out == "glb")
            return gltfToGlb(input, output);
        else if (ext_out == "b3dm")
        {
            std::string glb_path = output + ".glb";
            if (0 == gltfToGlb(input, glb_path))
            {
                int result = glbToB3dm(glb_path, output);
                // remove the tmp glb file
                remove(glb_path.c_str());
                return result;
            }
            return 1;
        }
    }

    printf("input %s and output %s are not support now.\n", input.c_str(), output.c_str());
    return 1;
}