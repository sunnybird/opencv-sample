#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

typedef struct
{
    uint8_t* y;    // 亮度分量
    uint8_t* vu;   // 色度分量(VU交错)
    int      width;
    int      height;
} NV21Image;

typedef struct
{
    float a, b, c;   // 线性变换参数
    float d, e, f;   // 平移参数
} AffineMatrix;

NV21Image* create_nv21(int w, int h)
{
    NV21Image* img = malloc(sizeof(NV21Image));
    img->y         = malloc(w * h);
    img->vu        = malloc(w * h / 2);
    img->width     = w;
    img->height    = h;
    return img;
}

void free_nv21(NV21Image* img)
{
    free(img->y);
    free(img->vu);
    free(img);
}

int read_nv21_file(NV21Image* img, const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (fp != NULL) {
        fread(img->y, img->width * img->height, 1, fp);
        fread(img->vu, img->width * img->height / 2, 1, fp);
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
        fwrite(img->y, img->width * img->height, 1, fp);
        fwrite(img->vu, img->width * img->height / 2, 1, fp);
        fclose(fp);
        printf("write nv21file: %s  \n", filename);
        return 0;
    }
    return -1;
}

AffineMatrix matrix_multiply(AffineMatrix m1, AffineMatrix m2)
{
    return (AffineMatrix){.a = m1.a * m2.a + m1.b * m2.d,
                          .b = m1.a * m2.b + m1.b * m2.e,
                          .c = m1.a * m2.c + m1.b * m2.f + m1.c,
                          .d = m1.d * m2.a + m1.e * m2.d,
                          .e = m1.d * m2.b + m1.e * m2.e,
                          .f = m1.d * m2.c + m1.e * m2.f + m1.f};
}

AffineMatrix create_rotation_matrix(float theta)
{
    float rad = theta * M_PI / 180.0f;
    return (AffineMatrix){cos(rad), -sin(rad), 0, sin(rad), cos(rad), 0};
}

AffineMatrix create_translation_matrix(float tx, float ty)
{
    return (AffineMatrix){1, 0, tx, 0, 1, ty};
}

AffineMatrix create_scale_matrix(float sx, float sy)
{
    return (AffineMatrix){sx, 0, 0, 0, sy, 0};
}

AffineMatrix create_centered_scale(float sx, float sy, int w, int h)
{
    AffineMatrix move_back  = create_translation_matrix(-w / 2, -h / 2);
    AffineMatrix scale      = create_scale_matrix(sx, sy);
    AffineMatrix move_front = create_translation_matrix(w / 2, h / 2);
    return matrix_multiply(move_front, matrix_multiply(scale, move_back));
}

// 裁剪 NV21 图像
NV21Image* crop_nv21(const NV21Image* src, int left, int top, int crop_w, int crop_h)
{
    // 检查裁剪范围是否合法
    if (left < 0 || top < 0 || left + crop_w > src->width || top + crop_h > src->height) {
        printf("裁剪范围超出源图像范围！\n");
        return NULL;
    }

    // 强制坐标和尺寸为偶数（关键步骤）
    left   = (left / 2) * 2;
    top    = (top / 2) * 2;
    crop_w = (crop_w / 2) * 2;
    crop_h = (crop_h / 2) * 2;

    // 分配新的 NV21 图像结构
    NV21Image* cropped = create_nv21(crop_w, crop_h);

    if (!cropped) {
        printf("裁剪图像内存分配失败！\n");
        return NULL;
    }

    // 裁剪 Y 平面
    for (int row = 0; row < crop_h; row++) {
        memcpy(cropped->y + row * crop_w, src->y + (top + row) * src->width + left, crop_w);
    }

    // 裁剪 VU 平面
    for (int row = 0; row < crop_h / 2; row++) {
        memcpy(cropped->vu + row * crop_w, src->vu + ((top / 2) + row) * src->width + left, crop_w);
    }

    return cropped;
}

// 镜像处理[6,8](@ref)
void mirror_nv21(NV21Image* img)
{
    // Y分量镜像
    for (int y = 0; y < img->height; y++) {
        uint8_t* row = img->y + y * img->width;
        for (int left = 0, right = img->width - 1; left < right; left++, right--) {
            uint8_t tmp = row[left];
            row[left]   = row[right];
            row[right]  = tmp;
        }
    }

    // UV分量镜像[8](@ref)
    uint8_t* uv = img->vu;
    for (int y = 0; y < img->height / 2; y++) {
        for (int left = 0, right = img->width - 2; left < right; left += 2, right -= 2) {
            uint8_t tmp_v = uv[left];
            uint8_t tmp_u = uv[left + 1];
            uv[left]      = uv[right];
            uv[left + 1]  = uv[right + 1];
            uv[right]     = tmp_v;
            uv[right + 1] = tmp_u;
        }
        uv += img->width;
    }
}

uint8_t bilinear_interpolate_y(const NV21Image* src, float x, float y)
{
    int   x0 = (int)x, y0 = (int)y;
    int   x1 = x0 + 1, y1 = y0 + 1;
    float dx = x - x0, dy = y - y0;

    x0 = CLAMP(x0, 0, src->width - 1);
    y0 = CLAMP(y0, 0, src->height - 1);
    x1 = CLAMP(x1, 0, src->width - 1);
    y1 = CLAMP(y1, 0, src->height - 1);

    uint8_t v00 = src->y[y0 * src->width + x0];
    uint8_t v01 = src->y[y0 * src->width + x1];
    uint8_t v10 = src->y[y1 * src->width + x0];
    uint8_t v11 = src->y[y1 * src->width + x1];

    return (uint8_t)(v00 * (1 - dx) * (1 - dy) + v01 * dx * (1 - dy) + v10 * (1 - dx) * dy +
                     v11 * dx * dy);
}


void process_uv_component(uint8_t* dst_uv, const NV21Image* src, float x, float y)
{
    int src_x   = CLAMP((int)(x / 2 + 0.5f), 0, src->width / 2 - 1);
    int src_y   = CLAMP((int)(y / 2 + 0.5f), 0, src->height / 2 - 1);
    int src_idx = src_y * (src->width / 2) * 2 + src_x * 2;
    dst_uv[0]   = src->vu[src_idx];       // V
    dst_uv[1]   = src->vu[src_idx + 1];   // U
}


void affine_transform(NV21Image* dst, const NV21Image* src, AffineMatrix mat)
{
    memset(dst->y, 0, dst->width * dst->height);          // 初始化为黑色背景
    memset(dst->vu, 128, dst->width * dst->height / 2);   // UV填充灰色

    for (int y = 0; y < dst->height; y++) {
        for (int x = 0; x < dst->width; x++) {
            float src_x = mat.a * x + mat.b * y + mat.c;
            float src_y = mat.d * x + mat.e * y + mat.f;

            if (src_x >= 0 && src_x < src->width && src_y >= 0 && src_y < src->height) {
                // Y分量处理
                dst->y[y * dst->width + x] = bilinear_interpolate_y(src, src_x, src_y);

                // UV分量处理（每2x2块）
                if (x % 2 == 0 && y % 2 == 0) {
                    int uv_idx = (y / 2) * (dst->width / 2) * 2 + (x / 2) * 2;
                    process_uv_component(dst->vu + uv_idx, src, src_x, src_y);
                }
            }
        }
    }
}


// 双线性插值
uint8_t bilinear_interp(float x, float y, const uint8_t* img, int width, int height)
{
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // 边界处理
    x0 = (x0 < 0) ? 0 : (x0 >= width ? width - 1 : x0);
    y0 = (y0 < 0) ? 0 : (y0 >= height ? height - 1 : y0);
    x1 = (x1 < 0) ? 0 : (x1 >= width ? width - 1 : x1);
    y1 = (y1 < 0) ? 0 : (y1 >= height ? height - 1 : y1);

    float dx = x - x0;
    float dy = y - y0;

    // 四个相邻像素值
    uint8_t val00 = img[y0 * width + x0];
    uint8_t val01 = img[y0 * width + x1];
    uint8_t val10 = img[y1 * width + x0];
    uint8_t val11 = img[y1 * width + x1];

    // 插值计算
    float val = (1 - dx) * (1 - dy) * val00 + dx * (1 - dy) * val01 + (1 - dx) * dy * val10 +
                dx * dy * val11;

    return (uint8_t)(val + 0.5f);
}

// YUV仿射变换核心函数
void warp_affine(const NV21Image* src, NV21Image* dst, const AffineMatrix* mat)
{
    // Y分量处理
    for (int y = 0; y < dst->height; ++y) {
        for (int x = 0; x < dst->width; ++x) {
            // 反向映射
            float src_x = mat->a * x + mat->b * y + mat->c;
            float src_y = mat->d * x + mat->e * y + mat->f;

            // 边界检查
            if (src_x < 0 || src_x >= src->width || src_y < 0 || src_y >= src->height) {
                dst->y[y * dst->width + x] = 0;
                continue;
            }

            // 双线性插值
            dst->y[y * dst->width + x] =
                bilinear_interp(src_x, src_y, src->y, src->width, src->height);
        }
    }

    // UV分量处理（NV21格式）
    for (int y = 0; y < dst->height / 2; ++y) {
        for (int x = 0; x < dst->width; x += 2) {
            float src_x = mat->a * (x * 2) + mat->b * (y * 2) + mat->c;
            float src_y = mat->d * (x * 2) + mat->e * (y * 2) + mat->f;

            // 计算UV坐标（NV21的UV分量是交错存储的）
            int uv_x = (int)(src_x / 2);
            int uv_y = (int)(src_y / 2);

            // 边界检查
            if (uv_x < 0 || uv_x >= src->width / 2 || uv_y < 0 || uv_y >= src->height / 2) {
                dst->vu[y * dst->width + x]     = 128;   // 中性灰色
                dst->vu[y * dst->width + x + 1] = 128;
                continue;
            }

            // 直接采样（可改为插值）
            int src_index                   = uv_y * src->width + 2 * uv_x;
            dst->vu[y * dst->width + x]     = src->vu[src_index];       // V分量
            dst->vu[y * dst->width + x + 1] = src->vu[src_index + 1];   // U分量
        }
    }
}

// 示例主函数
int main()
{
    int ret = -1;

    int src_width  = 640;
    int src_height = 480;

    int dst_width  = 640;
    int dst_height = 480;

    int dst_crop_width  = 224;
    int dst_crop_height = 224;

    NV21Image* src = create_nv21(src_width, src_height);
    NV21Image* dst = create_nv21(dst_crop_width, dst_crop_height);

    char inputFile[] = "../data/examples_from_paper/prn_example_face";
    char outputFile[] = "../data/examples_from_paper/c_prn_example_face";
    char outputCropFile[] = "../data/examples_from_paper/c_prn_example_face_crop";


    // char inputFile[] = "../data/image_158";
    // char outputFile[] = "../data/c_output_image_158";
    // char outputCropFile[] = "../data/c_output_image_158_crop";

    char input_path[1026];
    sprintf(input_path,
            "%s_%dx%d.nv21",
            inputFile,
            src->width,
            src->height);

    char out_path[1204];
    sprintf(out_path,
            "%s_%dx%d.nv21",
            outputFile,
            src->width,
            src->height);

    char out_crop_path[1204];
    sprintf(out_crop_path,
            "%s_%dx%d.nv21",
            outputCropFile,
            dst_crop_width,
            dst_crop_height);

    ret = read_nv21_file(src, input_path);
    if (ret != 0) {
        return ret;
    }


    AffineMatrix param = {0.55722648, 0.12144679, -73.8135971, -0.12144679, 0.55722648, 3.02248176};

    // AffineMatrix param = {0.881481,   0.000000,    -36.529630,  -0.000000,   0.881481,   -9.288889};

    float angle   = atanf(param.b / param.e) * 180.0f / M_PI;
    float sx      = param.a / cosf(angle);
    float sy      = param.e / cosf(angle);
    float trans_x = param.c;
    float trans_y = param.f;

    // 创建30度旋转矩阵[2](@ref)
    // AffineMatrix mat =create_rotation_matrix(90.0f);
    // 创建复合变换矩阵：缩放1.5倍 + 平移(50,30) + 旋转30度
    // AffineMatrix scale     = create_centered_scale(2.0f, 2.0f, src->width, src->height);
    // AffineMatrix translate = create_translation_matrix((src->width/2)-(-73), src->height/2 - 3);
    // AffineMatrix rotate    = create_rotation_matrix(6.973f + 3.0f);

    AffineMatrix scale     = create_centered_scale( 1/sx, 1/sy, src->width, src->height);
    AffineMatrix translate = create_translation_matrix( trans_x,  trans_y);
    AffineMatrix rotate    = create_rotation_matrix(angle);


    // AffineMatrix combined = matrix_multiply(translate, matrix_multiply(rotate, scale));
    AffineMatrix combined = matrix_multiply(translate, matrix_multiply(scale, rotate));
    // AffineMatrix combined = matrix_multiply(scale, matrix_multiply(rotate, translate));
    // AffineMatrix combined = matrix_multiply(scale, rotate);

    // AffineMatrix combined = scale;
    // AffineMatrix combined = {
    //     0.55722648, 0.12144679, -73.8135971,
    //     -0.12144679, 0.55722648, 3.02248176};


    // AffineMatrix combined = {
    //     0.55722648, 0.12144679, -73.8135971,
    //     -0.12144679, 0.55722648, 3.02248176};

    // 执行仿射变换
    affine_transform(dst, src, param);
    // warp_affine(src, dst, &combined);

   int offset_left = tanf(angle) * dst_crop_height;

    ret = write_nv21_file(dst, out_crop_path);

    // 执行裁剪（左上角(100,50)，尺寸400x300）
    // NV21Image* dst2_cropped = NULL;
    // dst2_cropped            = crop_nv21(dst,
    //                          abs(trans_x),
    //                          abs(trans_y),
    //                          dst_crop_width,
    //                          dst_crop_height);
    // ret                     = write_nv21_file(dst2_cropped, out_crop_path);




    // 释放资源
exit:
    // free_nv21(dst2_cropped);

free_src_dst:
    free_nv21(src);
    free_nv21(dst);

    return ret;
}
