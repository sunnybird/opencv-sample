#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// 定义 NV21 图像结构体
typedef struct
{
    unsigned char* y_plane;    // Y 平面
    unsigned char* uv_plane;   // UV 平面 (交错的 VU)
    int            width;      // 图像宽度
    int            height;     // 图像高度
} NV21Image;

// 定义仿射变换矩阵结构体
typedef struct
{
    float m[3][3];   // 3x3 仿射变换矩阵
} AffineMatrix;

int read_nv21_file(NV21Image* img, const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (fp != NULL) {
        fread(img->y_plane, img->width * img->height, 1, fp);
        fread(img->uv_plane, img->width * img->height / 2, 1, fp);
        fclose(fp);
        printf("read nv21file: %s  \n", filename);
        return 0;
    }
    return -1;
}

int write_nv21_file(NV21Image* img, const char* filename)
{
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        fwrite(img->y_plane, img->width * img->height, 1, fp);
        fwrite(img->uv_plane, img->width * img->height / 2, 1, fp);
        fclose(fp);
        printf("write nv21file: %s  \n", filename);
        return 0;
    }
    return -1;
}

// 计算仿射矩阵的逆
int invert_affine_matrix(const AffineMatrix* mat, AffineMatrix* inv)
{
    float a = mat->m[0][0], b = mat->m[0][1], c = mat->m[0][2];
    float d = mat->m[1][0], e = mat->m[1][1], f = mat->m[1][2];

    float det = a * e - b * d;
    if (fabs(det) < 1e-6) return 0;   // 不可逆

    float inv_det = 1.0f / det;

    inv->m[0][0] = e * inv_det;
    inv->m[0][1] = -b * inv_det;
    inv->m[0][2] = (b * f - c * e) * inv_det;

    inv->m[1][0] = -d * inv_det;
    inv->m[1][1] = a * inv_det;
    inv->m[1][2] = (c * d - a * f) * inv_det;

    inv->m[2][0] = 0;
    inv->m[2][1] = 0;
    inv->m[2][2] = 1;

    return 1;
}

// 双线性插值（用于Y）
unsigned char bilinear_sample(const unsigned char* data, int width, int height, float x, float y)
{
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    if (x0 < 0 || y0 < 0 || x1 >= width || y1 >= height) return 0;

    float dx = x - x0;
    float dy = y - y0;

    unsigned char p00 = data[y0 * width + x0];
    unsigned char p01 = data[y0 * width + x1];
    unsigned char p10 = data[y1 * width + x0];
    unsigned char p11 = data[y1 * width + x1];

    float val =
        (1 - dx) * (1 - dy) * p00 + dx * (1 - dy) * p01 + (1 - dx) * dy * p10 + dx * dy * p11;

    return (unsigned char)(val + 0.5f);
}

// 取最近的UV（无插值，逐块复制）
void sample_uv(const unsigned char* src_uv, int src_width, int src_height, float x, float y,
               unsigned char* v_out, unsigned char* u_out)
{
    int uv_x = (int)(x / 2);
    int uv_y = (int)(y / 2);

    if (uv_x < 0 || uv_x >= src_width / 2 || uv_y < 0 || uv_y >= src_height / 2) {
        *v_out = 128;
        *u_out = 128;
        return;
    }

    int offset = (uv_y * src_width) + (uv_x * 2);
    *v_out     = src_uv[offset + 0];   // V
    *u_out     = src_uv[offset + 1];   // U
}

void affine_transform(NV21Image* dst, const NV21Image* src, AffineMatrix mat)
{
    AffineMatrix inv;
    if (!invert_affine_matrix(&mat, &inv)) {
        // 不可逆变换
        return;
    }

    int dst_w = dst->width;
    int dst_h = dst->height;
    int src_w = src->width;
    int src_h = src->height;

    // 处理 Y 分量
    for (int y_dst = 0; y_dst < dst_h; ++y_dst) {
        for (int x_dst = 0; x_dst < dst_w; ++x_dst) {
            // 计算源图像坐标
            float x_src = inv.m[0][0] * x_dst + inv.m[0][1] * y_dst + inv.m[0][2];
            float y_src = inv.m[1][0] * x_dst + inv.m[1][1] * y_dst + inv.m[1][2];

            dst->y_plane[y_dst * dst_w + x_dst] =
                bilinear_sample(src->y_plane, src_w, src_h, x_src, y_src);
        }
    }

    // 处理 UV 分量（每 2x2 像素一个块）
    for (int y = 0; y < dst_h; y += 2) {
        for (int x = 0; x < dst_w; x += 2) {
            float x_src = inv.m[0][0] * x + inv.m[0][1] * y + inv.m[0][2];
            float y_src = inv.m[1][0] * x + inv.m[1][1] * y + inv.m[1][2];

            unsigned char v, u;
            sample_uv(src->uv_plane, src_w, src_h, x_src, y_src, &v, &u);

            int offset                = (y / 2) * dst_w + x;
            dst->uv_plane[offset + 0] = v;
            dst->uv_plane[offset + 1] = u;
        }
    }
}

int main()
{
    // 创建源和目标 NV21 图像（假设图像大小为 640x480）
    NV21Image src, dst;
    src.width    = 640;
    src.height   = 480;
    src.y_plane  = malloc(src.width * src.height);
    src.uv_plane = malloc(src.width * src.height / 2);

    dst.width    = 224;
    dst.height   = 224;
    dst.y_plane  = malloc(dst.width * dst.height);
    dst.uv_plane = malloc(dst.width * dst.height / 2);


    char inputFile[]      = "../data/examples_from_paper/prn_example_face";
    char outputFile[]     = "../data/examples_from_paper/c_prn_example_face";
    char outputCropFile[] = "../data/examples_from_paper/c_prn_example_face_crop";

    // char inputFile[]      = "../data/image_158";
    // char outputFile[]     = "../data/c_output_image_158";
    // char outputCropFile[] = "../data/c_output_image_158_crop";


    char input_path[1024];
    sprintf(input_path, "%s_%dx%d.nv21", inputFile, src.width, src.height);

    char out_path[1024];
    sprintf(out_path, "%s_%dx%d.nv21", outputFile, dst.width, dst.height);

    // char out_crop_path[1024];
    // sprintf(out_crop_path, "%s_%dx%d.nv21", outputCropFile, dst_crop_width, dst_crop_height);

    // 填充源图像数据（这里只是示例，实际使用中需要加载图像数据）

    read_nv21_file(&src, input_path);

    // 定义仿射变换矩阵
    // AffineMatrix mat = {
    //     0.55722648, 0.12144679, -73.8135971,
    //     -0.12144679,0.55722648, 3.02248176
    // };

    AffineMatrix mat = {
        0.555642, 0.123476, -73.862274 ,
        -0.123476 ,0.555642, 3.995064,
    };


    // AffineMatrix mat = {0.881481, 0.000000, -36.529630, -0.000000, 0.881481, -9.288889};
    // AffineMatrix mat = {{
    //     {1.0, 0.0, 10.0}, // x' = x + 10
    //     {0.0, 1.0, 10.0}, // y' = y + 10
    //     {0.0, 0.0, 1.0}
    // }};

    // 执行仿射变换
    affine_transform(&dst, &src, mat);

    write_nv21_file(&dst, out_path);

    // 释放内存
    free(src.y_plane);
    free(src.uv_plane);
    free(dst.y_plane);
    free(dst.uv_plane);

    return 0;
}
