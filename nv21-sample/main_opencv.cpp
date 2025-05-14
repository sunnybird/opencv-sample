#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

void nv21_affine_transform(const uint8_t* src_nv21, int src_width, int src_height,
                           uint8_t* dst_nv21, int dst_width, int dst_height, const Mat& affine_mat)
{
    int src_y_size = src_width * src_height;
    int dst_y_size = dst_width * dst_height;

    // 分离 Y 和 VU
    Mat src_y(src_height, src_width, CV_8UC1, (void*)src_nv21);
    Mat src_vu(src_height / 2, src_width / 2, CV_8UC2, (void*)(src_nv21 + src_y_size));

    // 仿射变换 Y
    Mat dst_y;
    warpAffine(src_y,
               dst_y,
               affine_mat,
               Size(dst_width, dst_height),
               INTER_LINEAR,
               BORDER_CONSTANT,
               Scalar(0));

    // 仿射变换 UV（注意：UV 是 subsampled，需要 scale 坐标）
    Mat src_vu_up;
    resize(src_vu, src_vu_up, Size(src_width, src_height), 0, 0, INTER_LINEAR);

    Mat dst_vu_up;
    warpAffine(src_vu_up,
               dst_vu_up,
               affine_mat,
               Size(dst_width, dst_height),
               INTER_LINEAR,
               BORDER_CONSTANT,
               Scalar(128, 128));

    // 下采样 VU（从 dst_vu_up → dst_vu）
    Mat dst_vu;
    resize(dst_vu_up, dst_vu, Size(dst_width / 2, dst_height / 2), 0, 0, INTER_LINEAR);

    // 合并输出
    memcpy(dst_nv21, dst_y.data, dst_y.total());
    memcpy(dst_nv21 + dst_y_size, dst_vu.data, dst_vu.total() * 2);
}


inline Mat getRotationMatrix2D_1(Point2f center, double angle, double scale)
{
    // return Mat(getRotationMatrix2D_(center, angle, scale), true);

    angle *= CV_PI / 180;
    double alpha = std::cos(angle) * scale;
    double beta  = std::sin(angle) * scale;

    Matx23d M(alpha,
              beta,
              (1 - alpha) * center.x - beta * center.y,
              -beta,
              alpha,
              beta * center.x + (1 - alpha) * center.y);

    return Mat(M, true);
}

inline Mat getDefaultMat()
{
    Matx23d M(0.55722648, 0.12144679, -73.8135971, -0.12144679, 0.55722648, 3.02248176);

    // Matx23d  M(0.881481,   0.000000,    -36.529630,  -0.000000,   0.881481,   -9.288889);
    return Mat(M, true);
}

int main()
{
    const int src_width = 640, src_height = 480;
    const int dst_width = 224, dst_height = 224;

    size_t src_size = src_width * src_height * 3 / 2;
    size_t dst_size = dst_width * dst_height * 3 / 2;

    vector<uint8_t> src_nv21(src_size);
    vector<uint8_t> dst_nv21(dst_size);

    // std::string input_path = "../data/image_158_640x480.nv21";
    // std::string out_path =
    //     "../data/output_image_158_" + to_string(dst_width) + "x" + to_string(dst_height) +
    //     ".nv21";


    std::string input_path = "../data/examples_from_paper/prn_example_face_640x480.nv21";
    std::string out_path   = "../data/examples_from_paper/output_prn_example_face_" +
                           to_string(dst_width) + "x" + to_string(dst_height) + ".nv21";
    // === 加载 NV21 文件 ===
    ifstream fin(input_path, ios::binary);
    if (!fin) {
        cerr << "❌ 无法读取 input_640x480.nv21" << endl;
        return -1;
    }
    fin.read((char*)src_nv21.data(), src_size);
    fin.close();

    // === 构建仿射矩阵（旋转 + 缩放 + 中心对齐）===
    Point2f center(src_width / 2.0f, src_height / 2.0f);
    float   angle = 30.0f;   // 旋转角度
    float   scale = 0.5f;    // 缩放系数

    // Mat affine_mat = getRotationMatrix2D_1(center, angle, scale);

    Mat affine_mat = getDefaultMat();

    // // 可加平移微调
    // affine_mat.at<double>(0, 2) += -100;
    // affine_mat.at<double>(1, 2) += -60;

    cout << "affine_mat: " << affine_mat << endl;

    // === 执行仿射变换 ===
    nv21_affine_transform(
        src_nv21.data(), src_width, src_height, dst_nv21.data(), dst_width, dst_height, affine_mat);

    // === 写出结果 ===
    ofstream fout(out_path, ios::binary);
    fout.write((char*)dst_nv21.data(), dst_size);
    fout.close();

    cout << "✅ 仿射变换完成，输出：" << out_path << endl;
    return 0;
}

// ffplay -f rawvideo -pixel_format nv21 -video_size 320x240 output_320x240.nv21

// ffmpeg -f rawvideo -pixel_format nv21 -video_size 320x240 -i output_320x240.nv21 -frames:v 1
// output_320x240.png
