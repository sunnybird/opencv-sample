#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>

// 裁剪 NV21 数据
bool cropNV21(const cv::Mat& nv21, cv::Mat& croppedNv21,
              int width, int height,
              int cropX, int cropY, int cropWidth, int cropHeight,
              const std::string& debugDir,
              bool showDebugWindows)
{
    if (nv21.empty() || nv21.rows != height * 3 / 2 || nv21.cols != width) {
        std::cerr << "Invalid NV21 input size.\n";
        return false;
    }
    if (cropX < 0 || cropY < 0 || cropX + cropWidth > width || cropY + cropHeight > height) {
        std::cerr << "Crop region out of bounds.\n";
        return false;
    }
    if ((cropWidth % 2) != 0 || (cropHeight % 2) != 0) {
        std::cerr << "Crop width and height must be even.\n";
        return false;
    }

    // NV21 to BGR
    cv::Mat bgr;
    cv::cvtColor(nv21, bgr, cv::COLOR_YUV2BGR_NV21);

    // Crop rect
    cv::Rect roi(cropX, cropY, cropWidth, cropHeight);

    // Draw rectangle for visualization
    cv::Mat bgrWithRoi = bgr.clone();
    cv::rectangle(bgrWithRoi, roi, cv::Scalar(0, 0, 255), 2); // red box

    // Debug: save images
    if (!debugDir.empty()) {
        cv::imwrite(debugDir + "/input_bgr.jpg", bgr);
        cv::imwrite(debugDir + "/input_bgr_with_roi.jpg", bgrWithRoi);
    }

    if (showDebugWindows) {
        cv::imshow("Input BGR", bgr);
        cv::imshow("Input BGR with ROI", bgrWithRoi);
    }

    // Crop BGR
    cv::Mat croppedBGR = bgr(roi);

    if (!debugDir.empty()) {
        cv::imwrite(debugDir + "/cropped_bgr.jpg", croppedBGR);
    }

    if (showDebugWindows) {
        cv::imshow("Cropped BGR", croppedBGR);
        cv::waitKey(0);
        cv::destroyAllWindows();
    }

    // Convert cropped BGR to YUV I420
    cv::Mat yuvI420;
    cv::cvtColor(croppedBGR, yuvI420, cv::COLOR_BGR2YUV_I420);

    int ySize = cropWidth * cropHeight;
    int uvSize = ySize / 4;
    const uchar* yPlane = yuvI420.data;
    const uchar* uPlane = yuvI420.data + ySize;
    const uchar* vPlane = yuvI420.data + ySize + uvSize;

    croppedNv21.create(cropHeight * 3 / 2, cropWidth, CV_8UC1);
    uchar* dstY = croppedNv21.data;
    uchar* dstVU = dstY + ySize;

    memcpy(dstY, yPlane, ySize);
    for (int i = 0; i < uvSize; ++i) {
        dstVU[i * 2]     = vPlane[i];
        dstVU[i * 2 + 1] = uPlane[i];
    }

    return true;
}

// Function to convert NV21 to BGR format
cv::Mat nv21ToBGR(const unsigned char *nv21Data, int width, int height) {
    // Create a YUV420sp Mat (NV21 format)
    cv::Mat nv21(height + height / 2, width, CV_8UC1, const_cast<unsigned char *>(nv21Data));

    int cropX = 50, cropY = 50, cropWidth = 300, cropHeight = 400;
    // 裁剪 NV21 数据
    cv::Mat croppedNv21;
    if (!cropNV21(nv21, croppedNv21, width, height, cropX, cropY, cropWidth, cropHeight,"../data/",true)) {
        // return -1;
    }

    // Create an empty Mat to store BGR image
    cv::Mat bgrImage;
    // Convert NV21 to BGR
    cv::cvtColor(croppedNv21, bgrImage, cv::COLOR_YUV2BGR_NV21);

    return bgrImage;
}

// 将 I420 格式转换为 NV21 格式
void I420ToNV21(const uint8_t* i420, uint8_t* nv21, int width, int height) {
    int frameSize = width * height;
    const uint8_t* yPlane = i420;
    const uint8_t* uPlane = i420 + frameSize;
    const uint8_t* vPlane = i420 + frameSize + (frameSize / 4);

    // 复制 Y 分量
    memcpy(nv21, yPlane, frameSize);

    // 交错 VU
    uint8_t* vuPlane = nv21 + frameSize;
    for (int i = 0; i < frameSize / 4; ++i) {
        *vuPlane++ = vPlane[i]; // V
        *vuPlane++ = uPlane[i]; // U
    }
}

std::string removeFileExtension(const std::string& filePath) {
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot == std::string::npos) {
        return filePath; // 没有扩展名
    }
    return filePath.substr(0, lastDot);
}

std::string getFileExtension(const std::string& filePath) {
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot == std::string::npos) {
        return ""; // 没有扩展名
    }
    return filePath.substr(lastDot+1); // 包含点的扩展名
}

std::string convertbgr2yuv(std::string bgrFilePath) {
    cv::Mat bgrImage = cv::imread(bgrFilePath, cv::IMREAD_COLOR);
    if (bgrImage.empty()) {
        std::cerr << "无法读取图片: " << bgrFilePath << std::endl;
        return std::string("");
    }
    int width = bgrImage.cols;
    int height = bgrImage.rows;

    // 转换为 YUV I420 格式
    cv::Mat yuvI420;
    cv::cvtColor(bgrImage, yuvI420, cv::COLOR_BGR2YUV_I420);

    // 分配 NV21 数据缓冲区
    long nv21Size = width * height * 3 / 2;
    std::vector<uint8_t> nv21Data(nv21Size);

    I420ToNV21(yuvI420.data, nv21Data.data(), width, height);

    std::string outputPath = removeFileExtension(bgrFilePath)
    .append("_").append(std::to_string(width)).append("x").append(std::to_string(height))
    .append(".nv21");

    // 保存 NV21 到文件
    std::ofstream outFile(outputPath, std::ios::binary);
    outFile.write(reinterpret_cast<char*>(nv21Data.data()), nv21Size);
    outFile.close();

    std::cout <<"转换完成，保存到: " << outputPath << std::endl;

    return outputPath;
}

// xxx_640x480.nv21
void displayNV21File(std::string& nv21FilePath) {
    int width= 0;
    int height =0;

    std::string ext = getFileExtension(nv21FilePath);
    if (ext != "nv21") {
        std::cout << "不支持: " << nv21FilePath << std::endl;
        return;
    }

    std::string filename = removeFileExtension(nv21FilePath);
    std::string sizestr= filename.substr(filename.find_last_of('_') + 1, filename.length());

    width = atoi(sizestr.substr(0,sizestr.find_last_of('x')).c_str());
    height = atoi(sizestr.substr(sizestr.find_last_of('x') + 1, sizestr.length()).c_str());


    long nv21Size = width * height * 3 / 2;
    std::vector<uint8_t> nv21Data(nv21Size);

    std::ifstream inputFile(nv21FilePath, std::ios::binary);
    inputFile.read(reinterpret_cast<char*>(nv21Data.data()), nv21Size);
    inputFile.close();

    // Convert NV21 to BGR
    cv::Mat bgrImage = nv21ToBGR(nv21Data.data(), width, height);

    std::string outputPath = removeFileExtension(nv21FilePath);
    std::string outputPng =outputPath.substr(0,outputPath.find_last_of("_")).append("_out").append(".png");
    // 保存为 PNG
    if (!cv::imwrite(outputPng, bgrImage)) {
        std::cerr << "无法保存 PNG 文件: " << outputPng << std::endl;
    }else {
        std::cout << "保存 PNG 文件: " << outputPng << std::endl;
    }

    // Display the BGR image
    cv::imshow("NV21 Image", bgrImage);

}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file_path>" << std::endl;
        std::cerr << "file_path supported: \n" << " xxx.png \n xxx.jpg \n xxx_widthxheight.nv21 " << std::endl;
        return -1;
    }

    std::string inputPath = argv[1];
    std::string ext = getFileExtension(inputPath);
    std::string nv21Path;
    if (ext == "jpg" || ext == "png") {
        std::cout << "开始转换: " << ext << "---->" << "nv21" << std::endl;
       nv21Path= convertbgr2yuv(inputPath);
    }else if (ext == "nv21") {
        nv21Path = inputPath;
    }

    displayNV21File(nv21Path);

    cv::waitKey(0);
    return 0;
}
