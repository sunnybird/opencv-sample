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


// 示例主函数
int main()
{
    int ret = -1;
    // 创建测试图像(640x480)
    NV21Image* src = create_nv21(640, 480);
    NV21Image* dst = create_nv21(640, 480);

    ret = read_nv21_file(src, "../input_640x480.nv21");
    if (ret != 0) {
        goto free_src_dst;
    }

    {
        // 创建30度旋转矩阵[2](@ref)
        // AffineMatrix mat =create_rotation_matrix(90.0f);
        // 创建复合变换矩阵：缩放1.5倍 + 平移(50,30) + 旋转30度
        AffineMatrix scale     = create_centered_scale(1.5f, 1.5f, src->width, src->height);
        AffineMatrix translate = create_translation_matrix(50, 30);
        AffineMatrix rotate    = create_rotation_matrix(30.0f);


        // AffineMatrix combined = matrix_multiply(translate, matrix_multiply(rotate, scale));
        // AffineMatrix combined = matrix_multiply(translate, scale);
        // AffineMatrix combined = scale;
        AffineMatrix combined = translate;

        // 执行仿射变换
        affine_transform(dst, src, combined);
    }


    ret = write_nv21_file(dst, "../output_640x480_dpseek.nv21");
    if (ret != 0) {
        goto free_src_dst;
    }

    // 执行裁剪（左上角(100,50)，尺寸400x300）
    NV21Image* dst2_cropped = crop_nv21(dst, 100, 50, 400, 300);
    mirror_nv21(dst2_cropped);

    ret = write_nv21_file(dst2_cropped, "../output_400x300_dpseek_crop.nv21");
    if (ret != 0) {
        goto exit;
    }

    return 0;

    // 释放资源
exit:
    free_nv21(dst2_cropped);

free_src_dst:
    free_nv21(src);
    free_nv21(dst);

    return ret;

    // // 创建测试图像(640x480)
    // NV21Image* src = create_nv21(640, 480);
    // read_nv21_file(src, "../input_640x480.nv21");
    //
    // // 执行镜像处理[6,8](@ref)
    // mirror_nv21(src);
    //
    // // 创建复合变换矩阵
    // AffineMatrix rotate = create_rotation_matrix(30.0f);
    // AffineMatrix scale = create_scale_matrix(1.5f, 1.5f);
    // AffineMatrix trans = matrix_multiply(rotate, scale);
    //
    // // 执行仿射变换
    // NV21Image* transformed = create_nv21(640, 480);
    // affine_transform(transformed, src, trans);
    // write_nv21_file(transformed, "../output_640x480_dpseek.nv21");
    //
    // // 裁剪结果图像
    // NV21Image* cropped = crop_nv21_v1(transformed, 100, 50, 400, 300);
    // write_nv21_file(cropped, "../output_640x480_dpseek_crop.nv21");
    //
    // // 释放资源
    // free_nv21(src);
    // free_nv21(transformed);
    // free_nv21(cropped);
    return 0;
}
